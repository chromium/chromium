// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.OTHER;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.REPARENTING;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.USER_NAVIGATION;

import android.app.Activity;
import android.content.Intent;
import android.net.Uri;
import android.os.Bundle;
import android.os.StrictMode;
import android.os.SystemClock;
import android.support.v4.app.ActivityOptionsCompat;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.browserservices.BrowserServicesActivityTabController;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CloseButtonNavigator;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.PageTransition;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

import javax.inject.Inject;

import dagger.Lazy;

/**
 * Responsible for navigating to new pages and going back to previous pages.
 */
@ActivityScope
public class CustomTabActivityNavigationController implements StartStopWithNativeObserver {

    @IntDef({USER_NAVIGATION, REPARENTING, OTHER})
    @Retention(RetentionPolicy.SOURCE)
    public @interface FinishReason {
        int USER_NAVIGATION = 0;
        int REPARENTING = 1;
        int OTHER = 2;
    }

    /** A handler of back presses. */
    public interface BackHandler {
        /**
         * Called when back button is pressed, unless already handled by another handler.
         * The implementation should do one of the following:
         * 1) Synchronously accept and handle the event and return true;
         * 2) Synchronously reject the event by returning false;
         * 3) Accept the event by returning true, handle it asynchronously, and if the handling
         * fails, trigger the default handling routine by running the defaultBackHandler.
         */
        boolean handleBackPressed(Runnable defaultBackHandler);
    }

    /** Interface encapsulating the process of handling the custom tab closing. */
    public interface FinishHandler {
        void onFinish(@FinishReason int reason);
    }

    private final BrowserServicesActivityTabController mTabController;
    private final CustomTabActivityTabProvider mTabProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabsConnection mConnection;
    private final Lazy<CustomTabObserver> mCustomTabObserver;
    private final CloseButtonNavigator mCloseButtonNavigator;
    private final ChromeBrowserInitializer mChromeBrowserInitializer;
    private final Activity mActivity;
    private final Lazy<ChromeFullscreenManager> mFullscreenManager;

    @Nullable
    private ToolbarManager mToolbarManager;

    @Nullable
    private BackHandler mBackHandler;

    @Nullable
    private FinishHandler mFinishHandler;

    private boolean mIsFinishing;

    private boolean mIsHandlingUserNavigation;

    private final CustomTabActivityTabProvider.Observer mTabObserver =
            new CustomTabActivityTabProvider.Observer() {

        @Override
        public void onAllTabsClosed() {
            finish(mIsHandlingUserNavigation ? USER_NAVIGATION : OTHER);
        }
    };

    @Inject
    public CustomTabActivityNavigationController(
            BrowserServicesActivityTabController tabController,
            CustomTabActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabsConnection connection,
            Lazy<CustomTabObserver> customTabObserver,
            CloseButtonNavigator closeButtonNavigator,
            ChromeBrowserInitializer chromeBrowserInitializer,
            ChromeActivity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            Lazy<ChromeFullscreenManager> fullscreenManager) {
        mTabController = tabController;
        mTabProvider = tabProvider;
        mIntentDataProvider = intentDataProvider;
        mConnection = connection;
        mCustomTabObserver = customTabObserver;
        mCloseButtonNavigator = closeButtonNavigator;
        mChromeBrowserInitializer = chromeBrowserInitializer;
        mActivity = activity;
        mFullscreenManager = fullscreenManager;

        lifecycleDispatcher.register(this);
        mTabProvider.addObserver(mTabObserver);
    }

    /**
     * Notifies the navigation controller that the ToolbarManager has been created and is ready for
     * use. ToolbarManager isn't passed directly to the constructor because it's not guaranteed to
     * be initialized yet.
     */
    public void onToolbarInitialized(ToolbarManager manager) {
        assert manager != null : "Toolbar manager not initialized";
        mToolbarManager = manager;
    }

    /**
     * Navigates to given url.
     */
    public void navigate(String url) {
        navigate(new LoadUrlParams(url), SystemClock.elapsedRealtime());
    }

    /**
     * Performs navigation using given {@link LoadUrlParams}.
     * Uses provided timestamp as the initial time for tracking page loading times
     * (see {@link CustomTabObserver}).
     */
    public void navigate(final LoadUrlParams params, long timeStamp) {
        assert mIntentDataProvider.getWebappExtras() == null;

        Tab tab = mTabProvider.getTab();
        if (tab == null) {
            assert false;
            return;
        }

        mCustomTabObserver.get().trackNextPageLoadFromTimestamp(tab, timeStamp);

        IntentHandler.addReferrerAndHeaders(params, mIntentDataProvider.getIntent());
        if (params.getReferrer() == null) {
            params.setReferrer(mConnection.getReferrerForSession(mIntentDataProvider.getSession()));
        }

        // Launching a TWA would count as a TOPLEVEL transition since it opens up an app-like
        // experience, and should count towards site engagement scores. This matches WebAPK
        // behaviour. CCTs on the other hand still count as LINK transitions.
        int transition;
        if (mIntentDataProvider.isTrustedWebActivity()) {
          transition = PageTransition.AUTO_TOPLEVEL | PageTransition.FROM_API;
        } else if (mIntentDataProvider.isOpenedByWebApk()) {
          transition = PageTransition.LINK;
          params.setHasUserGesture(true);
        } else {
          transition = PageTransition.LINK | PageTransition.FROM_API;
        }

        params.setTransitionType(IntentHandler.getTransitionTypeFromIntent(
                mIntentDataProvider.getIntent(), transition));
        tab.loadUrl(params);
    }

    /**
     * Handles back button navigation.
     */
    public boolean navigateOnBack() {
        if (!mChromeBrowserInitializer.hasNativeInitializationCompleted()) return false;

        RecordUserAction.record("CustomTabs.SystemBack");
        if (mTabProvider.getTab() == null) return false;

        if (mFullscreenManager.get().getPersistentFullscreenMode()) {
            mFullscreenManager.get().exitPersistentFullscreenMode();
            return true;
        }

        if (mBackHandler != null
                && mBackHandler.handleBackPressed(this::executeDefaultBackHandling)) {
            return true;
        }

        executeDefaultBackHandling();
        return true;
    }

    private void executeDefaultBackHandling() {
        if (mToolbarManager != null && mToolbarManager.back() != null) return;

        // mTabController.closeTab may result in either closing the only tab (through the back
        // button or the close button), or swapping to the previous tab. In the first case we need
        // finish to be called with USER_NAVIGATION reason.
        mIsHandlingUserNavigation = true;
        mTabController.closeTab();
        mIsHandlingUserNavigation = false;
    }

    /**
     * Handles close button navigation.
     */
    public void navigateOnClose() {
        mIsHandlingUserNavigation = true;
        mCloseButtonNavigator.navigateOnClose();
        mIsHandlingUserNavigation = false;
    }

    /**
     * Opens the URL currently being displayed in the Custom Tab in the regular browser.
     * @param forceReparenting Whether tab reparenting should be forced for testing.
     *
     * @return Whether or not the tab was sent over successfully.
     */
    public boolean openCurrentUrlInBrowser(boolean forceReparenting) {
        assert mIntentDataProvider.getWebappExtras() == null;

        Tab tab = mTabProvider.getTab();
        if (tab == null) return false;

        String url = tab.getUrl();
        if (DomDistillerUrlUtils.isDistilledPage(url)) {
            url = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(url);
        }
        if (TextUtils.isEmpty(url)) url = mIntentDataProvider.getUrlToLoad();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        boolean willChromeHandleIntent =
                mIntentDataProvider.isOpenedByChrome() || mIntentDataProvider.isIncognito();

        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
        try {
            willChromeHandleIntent |= ExternalNavigationDelegateImpl
                    .willChromeHandleIntent(intent, true);
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        Bundle startActivityOptions = ActivityOptionsCompat.makeCustomAnimation(
                mActivity, R.anim.abc_fade_in, R.anim.abc_fade_out).toBundle();
        if (willChromeHandleIntent || forceReparenting) {
            // Remove observer to not trigger finishing in onAllTabsClosed() callback - we'll use
            // reparenting finish callback instead.
            mTabProvider.removeObserver(mTabObserver);
            mTabController.detachAndStartReparenting(intent, startActivityOptions,
                    () -> finish(REPARENTING));
        } else {
            // Temporarily allowing disk access while fixing. TODO: http://crbug.com/581860
            StrictMode.allowThreadDiskWrites();
            try {
                if (mIntentDataProvider.isInfoPage()) {
                    IntentHandler.startChromeLauncherActivityForTrustedIntent(intent);
                } else {
                    mActivity.startActivity(intent, startActivityOptions);
                }
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
        return true;
    }

    /**
     * Finishes the Custom Tab activity and removes the reference from the Android recents.
     *
     * @param reason The reason for finishing.
     */
    public void finish(@FinishReason int reason) {
        if (mIsFinishing) return;
        mIsFinishing = true;

        if (reason != REPARENTING) {
            // Closing the activity destroys the renderer as well. Re-create a spare renderer some
            // time after, so that we have one ready for the next tab open. This does not increase
            // memory consumption, as the current renderer goes away. We create a renderer as a lot
            // of users open several Custom Tabs in a row. The delay is there to avoid jank in the
            // transition animation when closing the tab.
            PostTask.postDelayedTask(UiThreadTaskTraits.DEFAULT,
                    CustomTabsConnection::createSpareWebContents, 500);
        }

        if (mFinishHandler != null) {
            mFinishHandler.onFinish(reason);
        }
    }

    /**
     * See {@link BackHandler}. Only one BackHandler at a time should be set.
     */
    public void setBackHandler(BackHandler handler) {
        assert mBackHandler == null : "Multiple BackHandlers not supported";
        mBackHandler = handler;
    }

    /**
     * Sets a {@link FinishHandler} to be notified when the custom tab is being closed.
     */
    public void setFinishHandler(FinishHandler finishHandler) {
        assert mFinishHandler == null :
                "Multiple FinishedHandlers not supported, replace with ObserverList if necessary";
        mFinishHandler = finishHandler;
    }

    /**
     * Sets a criterion to choose a page to land to when close button is pressed.
     * Only one such criterion can be set.
     * If no page in the navigation history meets the criterion, or there is no criterion, then
     * pressing close button will finish the Custom Tab activity.
     */
    public void setLandingPageOnCloseCriterion(CloseButtonNavigator.PageCriteria criterion) {
        mCloseButtonNavigator.setLandingPageCriteria(criterion);
    }

    @Override
    public void onStartWithNative() {
        mIsFinishing = false;
    }

    @Override
    public void onStopWithNative() {
        if (mIsFinishing) {
            mTabController.closeAndForgetTab();
        } else {
            mTabController.saveState();
        }
    }
}
