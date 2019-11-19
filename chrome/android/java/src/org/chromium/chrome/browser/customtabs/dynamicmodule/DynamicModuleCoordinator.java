// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.dynamicmodule;

import static org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleConstants.ON_BACK_PRESSED_ASYNC_API_VERSION;

import android.content.ComponentName;
import android.content.Context;
import android.net.Uri;
import android.text.TextUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsService;
import androidx.browser.customtabs.PostMessageBackend;

import org.chromium.base.Callback;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.browserservices.PostMessageHandler;
import org.chromium.chrome.browser.customtabs.CloseButtonNavigator;
import org.chromium.chrome.browser.customtabs.CloseButtonNavigator.PageCriteria;
import org.chromium.chrome.browser.customtabs.CustomTabBottomBarDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabTopBarDelegate;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.lifecycle.NativeInitObserver;
import org.chromium.chrome.browser.metrics.PageLoadMetrics;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.util.UrlConstants;
import org.chromium.chrome.browser.util.UrlUtilities;
import org.chromium.chrome.browser.util.UrlUtilitiesJni;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.regex.Pattern;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Class to control a CCT dynamic module.
 */
@ActivityScope
public class DynamicModuleCoordinator implements NativeInitObserver, Destroyable {
    private final CustomTabIntentDataProvider mIntentDataProvider;
    private final TabObserverRegistrar mTabObserverRegistrar;
    private final CustomTabsConnection mConnection;
    private final CustomTabActivityTabProvider mTabProvider;
    private final CustomTabActivityNavigationController mNavigationController;

    private final ChromeActivity mActivity;

    private final Lazy<CustomTabTopBarDelegate> mTopBarDelegate;
    private final Lazy<CustomTabBottomBarDelegate> mBottomBarDelegate;
    private final Lazy<ChromeFullscreenManager> mFullscreenManager;
    private final Lazy<DynamicModuleToolbarController> mToolbarController;

    @Nullable
    private LoadModuleCallback mModuleCallback;
    @Nullable
    private ModuleEntryPoint mModuleEntryPoint;

    private ActivityDelegate mActivityDelegate;

    @Nullable
    private PostMessageHandler mDynamicModulePostMessageHandler;

    @IntDef({View.VISIBLE, View.INVISIBLE, View.GONE})
    @Retention(RetentionPolicy.SOURCE)
    private @interface ToolbarVisibility {}

    // Default visibility of the Toolbar prior to any header customization.
    @ToolbarVisibility
    private int mDefaultToolbarVisibility;
    // The value is either View.VISIBLE, View.INVISIBLE, or View.GONE.
    @ToolbarVisibility
    private int mDefaultToolbarShadowVisibility;

    // Default height of the top control container prior to any header customization.
    private int mDefaultTopControlContainerHeight;
    private boolean mHasSetOverlayView;

    // Whether isModuleManagedUrl(url) must check the URL's port number or not.
    // This makes it easier to run tests with the EmbeddedTestServer.
    private static boolean sAllowNonStandardPortNumber; // false by default.

    @VisibleForTesting
    public static void setAllowNonStandardPortNumber(boolean allowNonStandardPortNumber) {
        sAllowNonStandardPortNumber = allowNonStandardPortNumber;
    }

    private final EmptyTabObserver mHeaderVisibilityObserver = new EmptyTabObserver() {
        @Override
        public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
            if (!navigation.isInMainFrame() || !navigation.hasCommitted()) return;
            maybeCustomizeCctHeader(navigation.getUrl());
        }
    };

    // Update the request's header on module managed URLs.
    private final EmptyTabObserver mCustomRequestHeaderModifier = new EmptyTabObserver() {
        @Override
        public void onDidStartNavigation(Tab tab, NavigationHandle navigation) {
            updateCustomRequestHeader(navigation, /* isRedirect */ false);
        }

        @Override
        public void onDidRedirectNavigation(Tab tab, NavigationHandle navigation) {
            updateCustomRequestHeader(navigation, /* is_redirect */ true);
        }

        private void updateCustomRequestHeader(NavigationHandle navigation, boolean isRedirect) {
            // Update an header only when the navigation emit a network request.²
            if (!navigation.isInMainFrame() || navigation.isSameDocument()
                    || navigation.isErrorPage()
                    || !ChromeFeatureList.isEnabled(
                            ChromeFeatureList.CCT_MODULE_CUSTOM_REQUEST_HEADER)) {
                return;
            }

            try (TraceEvent e = TraceEvent.scoped(
                         "DynamicModuleCoordinator.updateCustomRequestHeader")) {
                if (isModuleManagedUrl(navigation.getUrl())) {
                    String headerValue = mIntentDataProvider.getExtraModuleManagedUrlsHeaderValue();
                    if (headerValue != null) {
                        navigation.setRequestHeader(
                                DynamicModuleConstants.MANAGED_URL_HEADER, headerValue);
                    }
                } else if (isRedirect) {
                    navigation.removeRequestHeader(DynamicModuleConstants.MANAGED_URL_HEADER);
                }
            }
        }
    };

    private final DynamicModuleNavigationEventObserver mModuleNavigationEventObserver =
            new DynamicModuleNavigationEventObserver();
    private final DynamicModulePageLoadObserver mPageLoadObserver;
    private final KeyboardVisibilityDelegate.KeyboardVisibilityListener mKeyboardVisibilityListener;
    private final PageCriteria mPageCriteria;

    @Inject
    public DynamicModuleCoordinator(CustomTabIntentDataProvider intentDataProvider,
                                    CloseButtonNavigator closeButtonNavigator,
                                    TabObserverRegistrar tabObserverRegistrar,
                                    ActivityLifecycleDispatcher activityLifecycleDispatcher,
                                    CustomTabActivityNavigationController navigationController,
                                    ActivityDelegate activityDelegate,
                                    Lazy<CustomTabTopBarDelegate> topBarDelegate,
                                    Lazy<CustomTabBottomBarDelegate> bottomBarDelegate,
                                    Lazy<ChromeFullscreenManager> fullscreenManager,
                                    Lazy<DynamicModuleToolbarController> toolbarController,
                                    CustomTabsConnection connection, ChromeActivity activity,
                                    CustomTabActivityTabProvider tabProvider,
                                    DynamicModulePageLoadObserver pageLoadObserver) {
        mIntentDataProvider = intentDataProvider;
        mTabObserverRegistrar = tabObserverRegistrar;
        mNavigationController = navigationController;
        mActivity = activity;
        mTabProvider = tabProvider;
        mConnection = connection;

        mTabObserverRegistrar.registerTabObserver(mModuleNavigationEventObserver);
        mTabObserverRegistrar.registerTabObserver(mHeaderVisibilityObserver);
        mTabObserverRegistrar.registerTabObserver(mCustomRequestHeaderModifier);

        mPageLoadObserver = pageLoadObserver;
        mTabObserverRegistrar.registerPageLoadMetricsObserver(mPageLoadObserver);

        mActivityDelegate = activityDelegate;
        mTopBarDelegate = topBarDelegate;
        mBottomBarDelegate = bottomBarDelegate;
        mFullscreenManager = fullscreenManager;
        mToolbarController = toolbarController;

        mPageCriteria = url -> (isModuleLoading() || isModuleLoaded()) && isModuleManagedUrl(url);
        closeButtonNavigator.setLandingPageCriteria(mPageCriteria);
        mNavigationController.setBackHandler(this::onBackPressedAsync);

        mKeyboardVisibilityListener = isShowing ->
            mBottomBarDelegate.get().hideBottomBar(isShowing);
        KeyboardVisibilityDelegate.getInstance()
                .addKeyboardVisibilityListener(mKeyboardVisibilityListener);

        activityLifecycleDispatcher.register(this);
    }

    @Override
    public void onFinishNativeInitialization() {
        loadModule();
    }

    /**
     * Dynamically loads a module using the component name specified in the intent if the feature is
     * enabled, the package is Google-signed, and it is not loaded yet.
     *
     * @return whether or not module loading starts.
     */
    @VisibleForTesting
    /* package */ void loadModule() {
        ModuleLoader moduleLoader = getModuleLoader();
        moduleLoader.loadModule();
        mModuleCallback = new LoadModuleCallback();
        moduleLoader.addCallbackAndIncrementUseCount(mModuleCallback);
    }

    @Override
    public void destroy() {
        mModuleEntryPoint = null;
        getModuleLoader().removeCallbackAndDecrementUseCount(mModuleCallback);
    }

    private ModuleLoader getModuleLoader() {
        ComponentName componentName = mIntentDataProvider.getModuleComponentName();
        String dexAssetName = mIntentDataProvider.getModuleDexAssetName();
        return mConnection.getModuleLoader(componentName, dexAssetName);
    }

    /* package */ Context getActivityContext() {
        return mActivity;
    }

    /* package */ void setBottomBarContentView(View view) {
        // All known usages of this method require the shadow to be hidden.
        // If this requirement ever changes, we could introduce an explicit API for that.
        mBottomBarDelegate.get().setShowShadow(false);
        mBottomBarDelegate.get().setBottomBarContentView(view);
        mBottomBarDelegate.get().showBottomBarIfNecessary();
    }

    /* package */ void setOverlayView(View view) {
        assert !mHasSetOverlayView;
        mHasSetOverlayView = true;
        ViewGroup.LayoutParams layoutParams = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT);
        mActivity.addContentView(view, layoutParams);
    }

    /* package */ void setBottomBarHeight(int height) {
        mBottomBarDelegate.get().setBottomBarHeight(height);
    }

    /* package */ void loadUri(Uri uri) {
        mNavigationController.navigate(uri.toString());
    }

    @VisibleForTesting
    public IActivityDelegate getActivityDelegateForTesting() {
        return mActivityDelegate.getActivityDelegateForTesting();
    }

    @VisibleForTesting
    public void setTopBarContentView(View view) {
        mTopBarDelegate.get().setTopBarContentView(view);
        maybeCustomizeCctHeader(mIntentDataProvider.getUrlToLoad());
    }

    @VisibleForTesting
    public void maybeInitialiseDynamicModulePostMessageHandler(PostMessageBackend backend) {
        // Only initialise the handler if the feature is enabled.
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_MODULE_POST_MESSAGE)) return;

        mDynamicModulePostMessageHandler = new PostMessageHandler(backend);
        mDynamicModulePostMessageHandler.reset(mActivity.getCurrentWebContents());
    }

    public void resetPostMessageHandlersForCurrentSession(WebContents newWebContents) {
        if (mDynamicModulePostMessageHandler != null) {
            mDynamicModulePostMessageHandler.reset(newWebContents);
        }
    }

    /**
     * @see IActivityDelegate#onBackPressedAsync
     */
    public boolean onBackPressedAsync(Runnable defaultBackHandler) {
        if (mModuleEntryPoint != null &&
                mModuleEntryPoint.getModuleVersion() >= ON_BACK_PRESSED_ASYNC_API_VERSION) {
            mActivityDelegate.onBackPressedAsync(defaultBackHandler);
            return true;
        }

        return false;
    }

    /**
     * Requests a postMessage channel for a loaded dynamic module.
     *
     * <p>The initialisation work is posted to the UI thread because this method will be called by
     * the dynamic module so we can't be sure of the thread it will be called on.
     *
     * @param postMessageOrigin The origin to use for messages posted to this channel.
     * @return Whether it was possible to request a channel. Will return false if the dynamic module
     *         has not been loaded.
     */
    public boolean requestPostMessageChannel(Uri postMessageOrigin) {
        if (mDynamicModulePostMessageHandler == null) return false;

        PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                ()
                        -> mDynamicModulePostMessageHandler.initializeWithPostMessageUri(
                                postMessageOrigin));
        return true;
    }

    /**
     * Posts a message from a loaded dynamic module.
     *
     * @param message The message to post to the page. Nothing is assumed about the format of the
     *                message; we just post it as-is.
     * @return Whether it was possible to post the message. Will always return {@link
     *         CustomTabsService#RESULT_FAILURE_DISALLOWED} if the dynamic module has not been
     *         loaded.
     */
    public int postMessage(String message) {
        // Use of the postMessage API is disallowed when the module has not been loaded.
        if (mDynamicModulePostMessageHandler == null) {
            return CustomTabsService.RESULT_FAILURE_DISALLOWED;
        }

        return mDynamicModulePostMessageHandler.postMessageFromClientApp(message);
    }

    /**
     * Callback to receive the entry point if it was loaded successfully,
     * or null if there was a problem. This is always called on the UI thread.
     */
    private class LoadModuleCallback implements Callback<ModuleEntryPoint> {
        @Override
        public void onResult(@Nullable ModuleEntryPoint entryPoint) {
            mToolbarController.get().releaseAndroidControlsHidingToken();
            mDefaultToolbarVisibility = mActivity.getToolbarManager().getToolbarVisibility();
            mDefaultToolbarShadowVisibility =
                    mActivity.getToolbarManager().getToolbarShadowVisibility();
            mDefaultTopControlContainerHeight = mFullscreenManager.get().getTopControlsHeight();

            mModuleCallback = null;

            if (entryPoint == null) {
                unregisterModuleObservers();
                mActivityDelegate = null;
            } else {
                mModuleEntryPoint = entryPoint;
                long createActivityDelegateStartTime = ModuleMetrics.now();
                IActivityDelegate activityDelegate = entryPoint.createActivityDelegate(
                        new ActivityHostImpl(DynamicModuleCoordinator.this));
                ModuleMetrics.recordCreateActivityDelegateTime(createActivityDelegateStartTime);

                mActivityDelegate.setActivityDelegate(activityDelegate);

                if (mModuleEntryPoint.getModuleVersion()
                        >= DynamicModuleConstants.ON_NAVIGATION_EVENT_MODULE_API_VERSION) {
                    mModuleNavigationEventObserver.setActivityDelegate(mActivityDelegate);
                } else {
                    unregisterObserver(mModuleNavigationEventObserver);
                }

                if (mModuleEntryPoint.getModuleVersion()
                        >= DynamicModuleConstants.ON_PAGE_LOAD_METRIC_API_VERSION) {
                    mPageLoadObserver.setActivityDelegate(mActivityDelegate);
                } else {
                    PageLoadMetrics.removeObserver(mPageLoadObserver);
                }

                // Initialise the PostMessageHandler for the current web contents.

                maybeInitialiseDynamicModulePostMessageHandler(
                        new ActivityDelegatePostMessageBackend(mActivityDelegate));
            }
            // Show CCT header (or top bar) if module fails (or succeeds) to load.
            maybeCustomizeCctHeader(mIntentDataProvider.getUrlToLoad());
        }
    }

    /* package */ boolean isModuleLoaded() {
        return mModuleEntryPoint != null;
    }

    /* package */ boolean isModuleLoading() {
        return mModuleCallback != null;
    }

    /* package */ boolean hasModuleFailedToLoad() {
        return mActivityDelegate == null;
    }

    private boolean isModuleManagedUrl(String url) {
        if (TextUtils.isEmpty(url)) {
            return false;
        }
        Pattern urlsPattern = mIntentDataProvider.getExtraModuleManagedUrlsPattern();
        if (urlsPattern == null) {
            return false;
        }
        String pathAndQuery = url.substring(UrlUtilities.stripPath(url).length());
        if (!urlsPattern.matcher(pathAndQuery).matches()) {
            return false;
        }
        Uri parsed = Uri.parse(url);
        String scheme = parsed.getScheme();
        if (!UrlConstants.HTTPS_SCHEME.equals(scheme)) {
            return false;
        }
        if (!UrlUtilitiesJni.get().isGoogleDomainUrl(url, sAllowNonStandardPortNumber)) {
            return false;
        }
        return true;
    }

    public void setTopBarHeight(int height) {
        mTopBarDelegate.get().setTopBarHeight(height);
        maybeCustomizeCctHeader(getContentUrl());
    }

    private String getContentUrl() {
        Tab tab = mTabProvider.getTab();
        if (tab != null && tab.getWebContents() != null && !tab.getWebContents().isDestroyed()
                && tab.getWebContents().getLastCommittedUrl() != null) {
            return tab.getWebContents().getLastCommittedUrl();
        }
        return mIntentDataProvider.getUrlToLoad();
    }

    private int getTopBarHeight() {
        Integer topBarHeight = mTopBarDelegate.get().getTopBarHeight();
        // Custom top bar height must not be larger than the height of the web content.
        if (topBarHeight != null && topBarHeight >= 0
                && mActivity.getWindow() != null
                && topBarHeight < mActivity.getWindow().getDecorView().getHeight() / 2) {
            return topBarHeight;
        }
        return mDefaultTopControlContainerHeight;
    }

    private boolean shouldHideCctHeaderOnModuleManagedUrls() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_MODULE_CUSTOM_HEADER)) return false;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_MODULE_USE_INTENT_EXTRAS)
                && mIntentDataProvider.shouldHideCctHeaderOnModuleManagedUrls()) {
            return true;
        }

        return mConnection.shouldHideTopBarOnModuleManagedUrlsForSession(
                mIntentDataProvider.getSession());
    }

    private View getProgressBarAnchorView(boolean showTopBar) {
        View anchorView = null;
        if (showTopBar) {
            View topBarContentView = mTopBarDelegate.get().getTopBarContentView();
            if (topBarContentView != null && topBarContentView.getVisibility() == View.VISIBLE) {
                anchorView = topBarContentView;
            }
        } else {
            anchorView = mActivity.getToolbarManager().getToolbarView();
        }
        return anchorView;
    }

    private void maybeCustomizeCctHeader(String url) {
        // Since some of the tool bar default settings are not obtained until module loading is
        // finished, we do not allow customization until then.
        if (!isModuleLoaded() && !hasModuleFailedToLoad()) return;

        boolean showTopBar = mPageCriteria.matches(url);
        mTopBarDelegate.get().showTopBarIfNecessary(showTopBar);
        if (shouldHideCctHeaderOnModuleManagedUrls()) {
            mActivity.getToolbarManager().setToolbarVisibility(
                    showTopBar ? View.GONE : mDefaultToolbarVisibility);
            mActivity.getToolbarManager().setToolbarShadowVisibility(
                    showTopBar ? View.GONE : mDefaultToolbarShadowVisibility);
            mFullscreenManager.get().setTopControlsHeight(
                    showTopBar ? getTopBarHeight() : mDefaultTopControlContainerHeight);
            mActivity.getToolbarManager().setProgressBarAnchorView(
                    getProgressBarAnchorView(showTopBar));
        }
    }

    private void unregisterModuleObservers() {
        unregisterObserver(mModuleNavigationEventObserver);
        unregisterObserver(mHeaderVisibilityObserver);
        unregisterObserver(mCustomRequestHeaderModifier);
        PageLoadMetrics.removeObserver(mPageLoadObserver);
        KeyboardVisibilityDelegate.getInstance()
                .removeKeyboardVisibilityListener(mKeyboardVisibilityListener);
    }

    private void unregisterObserver(TabObserver observer) {
        mActivity.getActivityTab().removeObserver(observer);
        mTabObserverRegistrar.unregisterTabObserver(observer);
    }
}
