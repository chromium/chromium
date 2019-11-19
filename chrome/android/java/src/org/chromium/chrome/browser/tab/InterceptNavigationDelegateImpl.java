// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.UserData;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler;
import org.chromium.chrome.browser.externalnav.ExternalNavigationHandler.OverrideUrlLoadingResult;
import org.chromium.chrome.browser.externalnav.ExternalNavigationParams;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.components.navigation_interception.NavigationParams;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ConsoleMessageLevel;

/**
 * Class that controls navigations and allows to intercept them. It is used on Android to 'convert'
 * certain navigations to Intents to 3rd party applications.
 * Note the Intent is often created together with a new empty tab which then should be closed
 * immediately. Closing the tab will cancel the navigation that this delegate is running for,
 * hence can cause UAF error. It should be done in an asynchronous fashion to avoid it.
 * See https://crbug.com/732260.
 */
public class InterceptNavigationDelegateImpl implements InterceptNavigationDelegate, UserData {
    private static final Class<InterceptNavigationDelegateImpl> USER_DATA_KEY =
            InterceptNavigationDelegateImpl.class;

    private final Tab mTab;
    private final AuthenticatorNavigationInterceptor mAuthenticatorHelper;
    private @OverrideUrlLoadingResult int mLastOverrideUrlLoadingResult =
            OverrideUrlLoadingResult.NO_OVERRIDE;
    private final TabObserver mDelegateObserver;
    private WebContents mWebContents;
    private ExternalNavigationHandler mExternalNavHandler;

    /**
     * Whether forward history should be cleared after navigation is committed.
     */
    private boolean mClearAllForwardHistoryRequired;
    private boolean mShouldClearRedirectHistoryForTabClobbering;

    public static void createForTab(Tab tab) {
        assert get(tab) == null;
        tab.getUserDataHost().setUserData(USER_DATA_KEY, new InterceptNavigationDelegateImpl(tab));
    }

    public static InterceptNavigationDelegateImpl get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }

    /**
     * Default constructor of {@link InterceptNavigationDelegateImpl}.
     */
    @VisibleForTesting
    InterceptNavigationDelegateImpl(Tab tab) {
        mTab = tab;
        mAuthenticatorHelper = AppHooks.get().createAuthenticatorNavigationInterceptor(mTab);
        mDelegateObserver = new EmptyTabObserver() {
            @Override
            public void onContentChanged(Tab tab) {
                associateWithWebContents(tab.getWebContents());
            }

            @Override
            public void onActivityAttachmentChanged(Tab tab, boolean attached) {
                if (attached) {
                    setExternalNavigationHandler(
                            tab.getDelegateFactory().createExternalNavigationHandler(tab));
                }
            }

            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (!navigation.hasCommitted() || !navigation.isInMainFrame()) return;
                maybeUpdateNavigationHistory();
            }

            @Override
            public void onDestroyed(Tab tab) {
                associateWithWebContents(null);
            }
        };
        mTab.addObserver(mDelegateObserver);
        associateWithWebContents(mTab.getWebContents());
    }

    @Override
    public void destroy() {
        mTab.removeObserver(mDelegateObserver);
    }

    @VisibleForTesting
    void setExternalNavigationHandler(ExternalNavigationHandler handler) {
        mExternalNavHandler = handler;
    }

    private void associateWithWebContents(WebContents webContents) {
        if (mWebContents == webContents) return;
        mWebContents = webContents;
        if (mWebContents == null) return;

        // Lazily initialize the external navigation handler.
        if (mExternalNavHandler == null) {
            setExternalNavigationHandler(
                    mTab.getDelegateFactory().createExternalNavigationHandler(mTab));
        }
        InterceptNavigationDelegateImplJni.get().associateWithWebContents(this, mWebContents);
    }

    public boolean shouldIgnoreNewTab(String url, boolean incognito) {
        if (mAuthenticatorHelper != null && mAuthenticatorHelper.handleAuthenticatorUrl(url)) {
            return true;
        }

        ExternalNavigationParams params = new ExternalNavigationParams.Builder(url, incognito)
                .setTab(mTab)
                .setOpenInNewTab(true)
                .build();
        mLastOverrideUrlLoadingResult = mExternalNavHandler.shouldOverrideUrlLoading(params);
        return mLastOverrideUrlLoadingResult
                != ExternalNavigationHandler.OverrideUrlLoadingResult.NO_OVERRIDE;
    }

    @VisibleForTesting
    public @OverrideUrlLoadingResult int getLastOverrideUrlLoadingResultForTests() {
        return mLastOverrideUrlLoadingResult;
    }

    @Override
    public boolean shouldIgnoreNavigation(NavigationParams navigationParams) {
        String url = navigationParams.url;
        ChromeActivity associatedActivity = mTab.getActivity();
        long lastUserInteractionTime =
                (associatedActivity == null) ? -1 : associatedActivity.getLastUserInteractionTime();

        if (mAuthenticatorHelper != null && mAuthenticatorHelper.handleAuthenticatorUrl(url)) {
            return true;
        }

        TabRedirectHandler tabRedirectHandler = null;
        if (navigationParams.isMainFrame) {
            tabRedirectHandler = TabRedirectHandler.from(mTab);
        } else if (navigationParams.isExternalProtocol) {
            // Only external protocol navigations are intercepted for iframe navigations.  Since
            // we do not see all previous navigations for the iframe, we can not build a complete
            // redirect handler for each iframe.  Nor can we use the top level redirect handler as
            // that has the potential to incorrectly give access to the navigation due to previous
            // main frame gestures.
            //
            // By creating a new redirect handler for each external navigation, we are specifically
            // not covering the case where a gesture is carried over via a redirect.  This is
            // currently not feasible because we do not see all navigations for iframes and it is
            // better to error on the side of caution and require direct user gestures for iframes.
            tabRedirectHandler = TabRedirectHandler.create();
        } else {
            assert false;
            return false;
        }
        tabRedirectHandler.updateNewUrlLoading(navigationParams.pageTransitionType,
                navigationParams.isRedirect,
                navigationParams.hasUserGesture || navigationParams.hasUserGestureCarryover,
                lastUserInteractionTime, getLastCommittedEntryIndex());

        boolean shouldCloseTab = shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent();
        ExternalNavigationParams params = buildExternalNavigationParams(navigationParams,
                tabRedirectHandler,
                shouldCloseTab).build();
        @OverrideUrlLoadingResult
        int result = mExternalNavHandler.shouldOverrideUrlLoading(params);
        mLastOverrideUrlLoadingResult = result;

        RecordHistogram.recordEnumeratedHistogram("Android.TabNavigationInterceptResult", result,
                OverrideUrlLoadingResult.NUM_ENTRIES);
        switch (result) {
            case OverrideUrlLoadingResult.OVERRIDE_WITH_EXTERNAL_INTENT:
                assert mExternalNavHandler.canExternalAppHandleUrl(url);
                if (navigationParams.isMainFrame) onOverrideUrlLoadingAndLaunchIntent();
                return true;
            case OverrideUrlLoadingResult.OVERRIDE_WITH_CLOBBERING_TAB:
                mShouldClearRedirectHistoryForTabClobbering = true;
                return true;
            case OverrideUrlLoadingResult.OVERRIDE_WITH_ASYNC_ACTION:
                if (!shouldCloseTab && navigationParams.isMainFrame) {
                    onOverrideUrlLoadingAndLaunchIntent();
                }
                return true;
            case OverrideUrlLoadingResult.NO_OVERRIDE:
            default:
                if (navigationParams.isExternalProtocol) {
                    logBlockedNavigationToDevToolsConsole(url);
                    return true;
                }
                return false;
        }
    }

    /**
     * Returns ExternalNavigationParams.Builder to generate ExternalNavigationParams for
     * ExternalNavigationHandler#shouldOverrideUrlLoading().
     */
    public ExternalNavigationParams.Builder buildExternalNavigationParams(
            NavigationParams navigationParams, TabRedirectHandler tabRedirectHandler,
            boolean shouldCloseTab) {
        boolean isInitialTabLaunchInBackground =
                mTab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND && shouldCloseTab;
        // http://crbug.com/448977: If a new tab is closed by this overriding, we should open an
        // Intent in a new tab when Chrome receives it again.
        return new ExternalNavigationParams
                .Builder(navigationParams.url, mTab.isIncognito(), navigationParams.referrer,
                        navigationParams.pageTransitionType, navigationParams.isRedirect)
                .setTab(mTab)
                .setApplicationMustBeInForeground(true)
                .setRedirectHandler(tabRedirectHandler)
                .setOpenInNewTab(shouldCloseTab)
                .setIsBackgroundTabNavigation(mTab.isHidden() && !isInitialTabLaunchInBackground)
                .setIsMainFrame(navigationParams.isMainFrame)
                .setHasUserGesture(navigationParams.hasUserGesture)
                .setShouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent(
                        shouldCloseTab && navigationParams.isMainFrame);
    }

    /**
     * Updates navigation history if navigation is canceled due to intent handler. We go back to the
     * last committed entry index which was saved before the navigation, and remove the empty
     * entries from the navigation history. See crbug.com/426679
     */
    public void maybeUpdateNavigationHistory() {
        WebContents webContents = mTab.getWebContents();
        if (mClearAllForwardHistoryRequired && webContents != null) {
            webContents.getNavigationController().pruneForwardEntries();
        } else if (mShouldClearRedirectHistoryForTabClobbering
                && webContents != null) {
            // http://crbug/479056: Even if we clobber the current tab, we want to remove
            // redirect history to be consistent.
            NavigationController navigationController =
                    webContents.getNavigationController();
            int indexBeforeRedirection =
                    TabRedirectHandler.from(mTab)
                            .getLastCommittedEntryIndexBeforeStartingNavigation();
            int lastCommittedEntryIndex = getLastCommittedEntryIndex();
            for (int i = lastCommittedEntryIndex - 1; i > indexBeforeRedirection; --i) {
                boolean ret = navigationController.removeEntryAtIndex(i);
                assert ret;
            }
        }
        mClearAllForwardHistoryRequired = false;
        mShouldClearRedirectHistoryForTabClobbering = false;
    }

    @VisibleForTesting
    public AuthenticatorNavigationInterceptor getAuthenticatorNavigationInterceptor() {
        return mAuthenticatorHelper;
    }

    private int getLastCommittedEntryIndex() {
        if (mTab.getWebContents() == null) return -1;
        return mTab.getWebContents().getNavigationController().getLastCommittedEntryIndex();
    }

    private boolean shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent() {
        if (mTab.getWebContents() == null) return false;
        if (!mTab.getWebContents().getNavigationController().canGoToOffset(0)) return true;

        // http://crbug/415948 : if the last committed entry index which was saved before this
        // navigation is invalid, it means that this navigation is the first one since this tab was
        // created.
        // In such case, we would like to close this tab.
        if (TabRedirectHandler.from(mTab).isOnNavigation()) {
            return TabRedirectHandler.from(mTab)
                           .getLastCommittedEntryIndexBeforeStartingNavigation()
                    == TabRedirectHandler.INVALID_ENTRY_INDEX;
        }
        return false;
    }

    /**
     * Called when Chrome decides to override URL loading and launch an intent or an asynchronous
     * action.
     */
    private void onOverrideUrlLoadingAndLaunchIntent() {
        if (mTab.getWebContents() == null) return;

        // Before leaving Chrome, close the empty child tab.
        // If a new tab is created through JavaScript open to load this
        // url, we would like to close it as we will load this url in a
        // different Activity.
        if (shouldCloseContentsOnOverrideUrlLoadingAndLaunchIntent()) {
            if (mTab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP) {
                // Moving task back before closing the tab allows back button to function better
                // when Chrome was an intermediate link redirector between two apps.
                // crbug.com/487938.
                mTab.getActivity().moveTaskToBack(false);
            }
            // Defer closing a tab (and the associated WebContents) till the navigation
            // request and the throttle finishes the job with it.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    TabModelSelector.from(mTab).closeTab(mTab);
                }
            });
        } else if (TabRedirectHandler.from(mTab).isOnNavigation()) {
            int lastCommittedEntryIndexBeforeNavigation =
                    TabRedirectHandler.from(mTab)
                            .getLastCommittedEntryIndexBeforeStartingNavigation();
            if (getLastCommittedEntryIndex() > lastCommittedEntryIndexBeforeNavigation) {
                // http://crbug/426679 : we want to go back to the last committed entry index which
                // was saved before this navigation, and remove the empty entries from the
                // navigation history.
                mClearAllForwardHistoryRequired = true;
                mTab.getWebContents().getNavigationController().goToNavigationIndex(
                        lastCommittedEntryIndexBeforeNavigation);
            }
        }
    }

    private void logBlockedNavigationToDevToolsConsole(String url) {
        int resId = mExternalNavHandler.canExternalAppHandleUrl(url)
                ? R.string.blocked_navigation_warning
                : R.string.unreachable_navigation_warning;
        mTab.getWebContents().addMessageToDevToolsConsole(ConsoleMessageLevel.WARNING,
                ContextUtils.getApplicationContext().getString(resId, url));
    }

    @VisibleForTesting
    static void initDelegateForTesting(Tab tab, InterceptNavigationDelegateImpl delegate) {
        delegate.associateWithWebContents(tab.getWebContents());
        tab.getUserDataHost().setUserData(USER_DATA_KEY, delegate);
    }

    @NativeMethods
    interface Natives {
        void associateWithWebContents(
                InterceptNavigationDelegateImpl nativeInterceptNavigationDelegateImpl,
                WebContents webContents);
    }
}
