// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.content.Intent;

import org.chromium.base.ContextUtils;
import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingTask;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.external_intents.InterceptNavigationDelegateClient;
import org.chromium.components.external_intents.InterceptNavigationDelegateImpl;
import org.chromium.components.external_intents.RedirectHandler;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Class that provides embedder-level information to InterceptNavigationDelegateImpl based off a
 * Tab.
 */
@NullMarked
public class InterceptNavigationDelegateClientImpl implements InterceptNavigationDelegateClient {
    private static @Nullable Boolean sIsInDesktopWindowingModeForTesting;
    private final TabImpl mTab;
    private final TabObserver mTabObserver;
    private InterceptNavigationDelegateImpl mInterceptNavigationDelegate;

    public static InterceptNavigationDelegateClientImpl createForTesting(Tab tab) {
        return new InterceptNavigationDelegateClientImpl(tab);
    }

    InterceptNavigationDelegateClientImpl(Tab tab) {
        mTab = (TabImpl) tab;
        mTabObserver =
                new EmptyTabObserver() {
                    @Override
                    public void onContentChanged(Tab tab) {
                        mInterceptNavigationDelegate.associateWithWebContents(tab.getWebContents());
                    }

                    @Override
                    public void onActivityAttachmentChanged(
                            Tab tab, @Nullable WindowAndroid window) {
                        if (window != null) {
                            mInterceptNavigationDelegate.setExternalNavigationHandler(
                                    createExternalNavigationHandler());
                        }
                        mInterceptNavigationDelegate.onActivityAttachmentChanged(window != null);
                    }

                    @Override
                    public void onDidFinishNavigationInPrimaryMainFrame(
                            Tab tab, NavigationHandle navigation) {
                        mInterceptNavigationDelegate.onNavigationFinishedInPrimaryMainFrame(
                                navigation);
                    }

                    @Override
                    public void onDestroyed(Tab tab) {
                        mInterceptNavigationDelegate.associateWithWebContents(null);
                    }
                };
    }

    @Override
    public @Nullable WebContents getWebContents() {
        return mTab.getWebContents();
    }

    @Override
    public @Nullable ExternalNavigationHandler createExternalNavigationHandler() {
        TabDelegateFactory delegateFactory = mTab.getDelegateFactory();
        if (delegateFactory == null) return null;
        return delegateFactory.createExternalNavigationHandler(mTab);
    }

    @Override
    public RedirectHandler getOrCreateRedirectHandler() {
        return RedirectHandlerTabHelper.getOrCreateHandlerFor(mTab);
    }

    @Override
    public boolean isIncognito() {
        return mTab.isIncognitoBranded();
    }

    @Override
    public @Nullable Activity getActivity() {
        return mTab.getActivity();
    }

    @Override
    public boolean wasTabLaunchedFromExternalApp() {
        return mTab.getLaunchType() == TabLaunchType.FROM_EXTERNAL_APP;
    }

    @Override
    public boolean wasTabLaunchedFromLongPressInBackground() {
        return mTab.getLaunchType() == TabLaunchType.FROM_LONGPRESS_BACKGROUND;
    }

    @Override
    public void closeTab() {
        if (mTab.isClosing()) return;
        ChromeActivity activity = assumeNonNull(mTab.getActivity());
        if (mTab.isCustomTab() && !activity.didFinishNativeInitialization()) {
            // Test the assumption that the tab hasn't been added to a tab model yet.
            assert activity.getTabModelSelector().getModelForTabId(mTab.getId()) == null;
            // Tab is closing before being attached to a tab model. Delay the closing until native
            // initialization finishes.
            mTab.setDidCloseWhileDetached();
        } else {
            activity.getTabModelSelector()
                    .tryCloseTab(
                            TabClosureParams.closeTab(mTab).allowUndo(false).build(),
                            /* allowDialog= */ false);
        }
    }

    @Override
    public void handleShouldCloseTab() {
        // Tab was destroyed before this task ran.
        if (getWebContents() == null) return;

        // If the launch was from an External app, Chrome came from the
        // background and acted as an intermediate link redirector between two
        // apps (crbug.com/487938).
        if (wasTabLaunchedFromExternalApp()) {
            Activity activity = assumeNonNull(getActivity());
            if (ChromeFeatureList.sCctDestroyTabWhenModelIsEmpty.isEnabled()
                    && mTab.isCustomTab()) {
                activity.finish();
            } else if (getOrCreateRedirectHandler().wasTaskStartedByExternalIntent()) {
                // If Chrome was only launched to perform a redirect, don't keep
                // its task in history.
                activity.finishAndRemoveTask();
            } else {
                // Takes Chrome out of the back stack.
                activity.moveTaskToBack(false);
            }
        }
        // Closing tab must happen after we potentially call
        // finishAndRemoveTask, as closing tabs can lead to the Activity being
        // finished, which would cause Android to ignore the
        // finishAndRemoveTask call, leaving the task
        // around.
        closeTab();
    }

    @Initializer
    public void initializeWithDelegate(InterceptNavigationDelegateImpl delegate) {
        mInterceptNavigationDelegate = delegate;
        mTab.addObserver(mTabObserver);
    }

    public void destroy() {
        mTab.removeObserver(mTabObserver);
    }

    @Override
    public void loadUrlIfPossible(LoadUrlParams loadUrlParams) {
        if (mTab.isDestroyed() || mTab.isClosing()) return;
        mTab.loadUrl(loadUrlParams);
    }

    @Override
    public boolean isTabInPWA() {
        return mTab.isTabInPWA();
    }

    @Override
    public boolean isTabInBrowser() {
        return mTab.isTabInBrowser();
    }

    @Override
    public boolean isTabDetached() {
        return mTab.isDetached();
    }

    @Override
    public boolean isInDesktopWindowingMode() {
        if (sIsInDesktopWindowingModeForTesting != null) {
            return sIsInDesktopWindowingModeForTesting;
        }

        // TODO(crbug.com/417047079): Replace the following desktop windowing checks with a better
        // approach.
        // return MultiWindowUtils.getInstance().isInMultiWindowMode(getActivity());
        return DeviceInfo.isDesktop();
    }

    @Override
    public void startReparentingTask() {
        Intent intent = new Intent();
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        ReparentingTask.from(mTab)
                .begin(
                        ContextUtils.getApplicationContext(),
                        intent,
                        /* startActivityOptions= */ null,
                        cleanupPendingTabClosure());
    }

    private Runnable cleanupPendingTabClosure() {
        final boolean isChromeTabbedActivityRunning =
                LaunchIntentDispatcher.chromeTabbedTaskExists(assertNonNull(getActivity()));
        return () -> {
            if (mTab.didCloseWhileDetached()) {
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            Activity activity = assumeNonNull(getActivity());
                            if (!isChromeTabbedActivityRunning) {
                                activity.finishAndRemoveTask();
                            } else {
                                activity.moveTaskToBack(false);
                            }
                            closeTab();
                        });
            }
        };
    }

    public static void setIsDesktopWindowingModeForTesting(boolean v) {
        sIsInDesktopWindowingModeForTesting = v;
        ResettersForTesting.register(() -> sIsInDesktopWindowingModeForTesting = null);
    }
}
