// Copyright 2019 The Chromium Authors
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
import android.provider.Browser;
import android.text.TextUtils;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.core.app.ActivityOptionsCompat;

import dagger.Lazy;

import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler;
import org.chromium.chrome.browser.back_press.MinimizeAppAndCloseTabBackPressHandler.MinimizeAppAndCloseTabType;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CloseButtonNavigator;
import org.chromium.chrome.browser.customtabs.CustomTabObserver;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.ToolbarManager;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.dom_distiller.core.DomDistillerUrlUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.function.Predicate;

import javax.inject.Inject;

/** Responsible for navigating to new pages and going back to previous pages. */
@ActivityScope
public class CustomTabActivityNavigationController
        implements StartStopWithNativeObserver, BackPressHandler {
    @IntDef({
        FinishReason.USER_NAVIGATION,
        FinishReason.REPARENTING,
        FinishReason.OTHER,
        FinishReason.OPEN_IN_BROWSER
    })
    @Target(ElementType.TYPE_USE)
    @Retention(RetentionPolicy.SOURCE)
    public @interface FinishReason {
        int USER_NAVIGATION = 0;
        // The web page is opened in the same browser by reparenting the tab into the browser.
        int REPARENTING = 1;
        int OTHER = 2;
        // The web page is opened in the default browser by starting a new activity.
        int OPEN_IN_BROWSER = 3;
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
        void onFinish(@FinishReason int reason, boolean warmupOnFinish);
    }

    /** Interface which gets the package name of the default web browser on the device. */
    public interface DefaultBrowserProvider {
        /** Returns the package name for the default browser on the device as a string. */
        @Nullable
        String getDefaultBrowser();
    }

    private final OneshotSupplier<ProfileProvider> mProfileProviderSupplier;
    private final CustomTabActivityTabController mTabController;
    private final CustomTabActivityTabProvider mTabProvider;
    private final BrowserServicesIntentDataProvider mIntentDataProvider;
    private final CustomTabsConnection mConnection;
    private final Lazy<CustomTabObserver> mCustomTabObserver;
    private final CloseButtonNavigator mCloseButtonNavigator;
    private final ChromeBrowserInitializer mChromeBrowserInitializer;
    private final Activity mActivity;
    private final DefaultBrowserProvider mDefaultBrowserProvider;
    private final ObservableSupplierImpl<Boolean> mBackPressStateSupplier =
            new ObservableSupplierImpl<>(false);

    @Nullable private ToolbarManager mToolbarManager;

    @Nullable private FinishHandler mFinishHandler;

    private boolean mIsFinishing;

    private boolean mIsHandlingUserNavigation;

    private @FinishReason int mFinishReason;

    private final CustomTabActivityTabProvider.Observer mTabObserver =
            new CustomTabActivityTabProvider.Observer() {
                @Override
                public void onInitialTabCreated(@NonNull Tab tab, int mode) {
                    mBackPressStateSupplier.set(shouldInterceptBackPress());
                }

                @Override
                public void onTabSwapped(@NonNull Tab tab) {
                    mBackPressStateSupplier.set(shouldInterceptBackPress());
                }

                @Override
                public void onAllTabsClosed() {
                    mBackPressStateSupplier.set(shouldInterceptBackPress());
                    finish(mIsHandlingUserNavigation ? USER_NAVIGATION : OTHER);
                }

                private boolean shouldInterceptBackPress() {
                    return mTabProvider.getTab() != null
                            && mChromeBrowserInitializer.isFullBrowserInitialized();
                }
            };

    @Inject
    public CustomTabActivityNavigationController(
            OneshotSupplier<ProfileProvider> profileProviderSupplier,
            CustomTabActivityTabController tabController,
            CustomTabActivityTabProvider tabProvider,
            BrowserServicesIntentDataProvider intentDataProvider,
            CustomTabsConnection connection,
            Lazy<CustomTabObserver> customTabObserver,
            CloseButtonNavigator closeButtonNavigator,
            ChromeBrowserInitializer chromeBrowserInitializer,
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            DefaultBrowserProvider customTabsDefaultBrowserProvider) {
        mProfileProviderSupplier = profileProviderSupplier;
        mTabController = tabController;
        mTabProvider = tabProvider;
        mIntentDataProvider = intentDataProvider;
        mConnection = connection;
        mCustomTabObserver = customTabObserver;
        mCloseButtonNavigator = closeButtonNavigator;
        mChromeBrowserInitializer = chromeBrowserInitializer;
        mActivity = activity;
        mDefaultBrowserProvider = customTabsDefaultBrowserProvider;

        lifecycleDispatcher.register(this);
        mTabProvider.addObserver(mTabObserver);
        mChromeBrowserInitializer.runNowOrAfterFullBrowserStarted(
                () -> {
                    mBackPressStateSupplier.set(mTabProvider.getTab() != null);
                });
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
     * Performs navigation using given {@link LoadUrlParams}.
     * The source Intent is used for tracking page loading times (see {@link CustomTabObserver}).
     */
    public void navigate(final LoadUrlParams params, Intent sourceIntent) {
        Tab tab = mTabProvider.getTab();
        if (tab == null) {
            assert false;
            return;
        }

        if (tab.isDestroyed()) {
            // This code path may be called asynchronously, assume that if the tab has been
            // destroyed there is no point in continuing.
            return;
        }

        // TODO(pkotwicz): Figure out whether we want to record these metrics for WebAPKs.
        if (mIntentDataProvider.getWebappExtras() == null) {
            mCustomTabObserver.get().trackNextPageLoadForLaunch(tab, sourceIntent);
        }

        IntentHandler.addReferrerAndHeaders(params, mIntentDataProvider.getIntent());

        // Launching a TWA, WebAPK or a standalone-mode homescreen shortcut counts as a TOPLEVEL
        // transition since it opens up an app-like experience, and should count towards site
        // engagement scores. CCTs on the other hand still count as LINK transitions.
        int transition;
        if (mIntentDataProvider.isTrustedWebActivity()
                || mIntentDataProvider.isWebappOrWebApkActivity()) {
            transition = PageTransition.AUTO_TOPLEVEL | PageTransition.FROM_API;
        } else {
            transition = PageTransition.LINK | PageTransition.FROM_API;
        }

        params.setTransitionType(
                IntentHandler.getTransitionTypeFromIntent(
                        mIntentDataProvider.getIntent(), transition));

        // The sender of an intent can't be trusted, so we navigate from an opaque Origin to
        // avoid sending same-site cookies.
        params.setInitiatorOrigin(Origin.createOpaqueOrigin());

        tab.loadUrl(params);
    }

    /** Handles back button navigation. */
    public boolean navigateOnBack() {
        if (!mChromeBrowserInitializer.isFullBrowserInitialized()) return false;

        boolean separateTask =
                (mIntentDataProvider.getIntent().getFlags()
                                & (Intent.FLAG_ACTIVITY_NEW_TASK
                                        | Intent.FLAG_ACTIVITY_NEW_DOCUMENT))
                        != 0;
        RecordUserAction.record("CustomTabs.SystemBack");
        if (mTabProvider.getTab() == null) return false;
        if (!BackPressManager.isEnabled()) {
            // If enabled, BackPressManager, rather than this class, will trigger their custom
            // logic of handling back press.
            final WebContents webContents = mTabProvider.getTab().getWebContents();
            if (webContents != null) {
                RenderFrameHost focusedFrame = webContents.getFocusedFrame();
                if (focusedFrame != null && focusedFrame.signalCloseWatcherIfActive()) {
                    BackPressManager.record(BackPressHandler.Type.CLOSE_WATCHER);
                    BackPressManager.recordForCustomTab(
                            BackPressHandler.Type.CLOSE_WATCHER, separateTask);
                    return true;
                }
            }

            if (mToolbarManager != null && mToolbarManager.back()) {
                BackPressManager.record(BackPressHandler.Type.TAB_HISTORY);
                BackPressManager.recordForCustomTab(
                        BackPressHandler.Type.TAB_HISTORY, separateTask);
                return true;
            }
            // If enabled, BackPressManager will record this internally. Otherwise, this should
            // be recorded manually.
            BackPressManager.record(BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB);
            BackPressManager.recordForCustomTab(
                    BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB, separateTask);
        } else if (BackPressManager.correctTabNavigationOnFallback()) {
            if (mTabProvider.getTab().canGoBack()) {
                return false;
            }
        }

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_BEFORE_UNLOAD)
                && mTabController.onlyOneTabRemaining()) {
            finishActivity(separateTask);
            return true;
        }

        if (mTabController.dispatchBeforeUnloadIfNeeded()) {
            MinimizeAppAndCloseTabBackPressHandler.record(MinimizeAppAndCloseTabType.CLOSE_TAB);
            MinimizeAppAndCloseTabBackPressHandler.recordForCustomTab(
                    MinimizeAppAndCloseTabType.CLOSE_TAB, separateTask);
            return true;
        }
        if (mTabController.onlyOneTabRemaining()) {
            finishActivity(separateTask);
        } else {
            MinimizeAppAndCloseTabBackPressHandler.record(MinimizeAppAndCloseTabType.CLOSE_TAB);
            MinimizeAppAndCloseTabBackPressHandler.recordForCustomTab(
                    MinimizeAppAndCloseTabType.CLOSE_TAB, separateTask);
            mTabController.closeTab();
        }

        return true;
    }

    private void finishActivity(boolean separateTask) {
        // If we're closing the last tab and it doesn't have beforeunload, just finish the Activity
        // manually. If we had called mTabController.closeTab() and waited for the Activity to close
        // as a result we would have a visual glitch: https://crbug.com/1087108.
        MinimizeAppAndCloseTabBackPressHandler.record(MinimizeAppAndCloseTabType.MINIMIZE_APP);
        MinimizeAppAndCloseTabBackPressHandler.recordForCustomTab(
                MinimizeAppAndCloseTabType.MINIMIZE_APP, separateTask);
        finish(USER_NAVIGATION);
    }

    @Override
    public int handleBackPress() {
        return navigateOnBack() ? BackPressResult.SUCCESS : BackPressResult.FAILURE;
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressStateSupplier;
    }

    /** Handles close button navigation. */
    public void navigateOnClose() {
        mIsHandlingUserNavigation = true;
        mCloseButtonNavigator.navigateOnClose(this::finish);
        mIsHandlingUserNavigation = false;
    }

    /**
     * Opens the URL currently being displayed in the Custom Tab in the regular browser.
     *
     * @return Whether or not the tab was sent over successfully.
     */
    public boolean openCurrentUrlInBrowser() {
        Tab tab = mTabProvider.getTab();
        if (tab == null) return false;

        GURL gurl = tab.getUrl();
        if (DomDistillerUrlUtils.isDistilledPage(gurl)) {
            gurl = DomDistillerUrlUtils.getOriginalUrlFromDistillerUrl(gurl);
        }
        String url = gurl.getSpec();
        if (TextUtils.isEmpty(url)) url = mIntentDataProvider.getUrlToLoad();
        Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.setFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.putExtra(IntentHandler.EXTRA_FROM_OPEN_IN_BROWSER, true);
        String packageName = mDefaultBrowserProvider.getDefaultBrowser();
        if (packageName != null) {
            intent.setPackage(packageName);
            // crbug.com/1265223
            if (intent.resolveActivity(mActivity.getPackageManager()) == null) {
                intent.setPackage(null);
            }
        }

        boolean isOffTheRecord = mIntentDataProvider.isOffTheRecord();
        boolean willChromeHandleIntent = mIntentDataProvider.isOpenedByChrome();

        // If the tab is opened by TWA or Webapp, do not reparent and finish the Custom Tab
        // activity because we still want to keep the app alive.
        boolean canFinishActivity =
                !mIntentDataProvider.isTrustedWebActivity()
                        && !mIntentDataProvider.isWebappOrWebApkActivity();

        willChromeHandleIntent |=
                ExternalNavigationDelegateImpl.willChromeHandleIntent(intent, true);

        Bundle startActivityOptions =
                ActivityOptionsCompat.makeCustomAnimation(
                                mActivity, R.anim.abc_fade_in, R.anim.abc_fade_out)
                        .toBundle();

        if (isOffTheRecord) {
            // If "Open in browser" was triggered in an OTR CCT, always open in a new Chrome
            // Incognito tab instead of re-parenting the tab to prevent profile-mismatch with the
            // TabModel as both eCCT & iCCT have a different OTRProfileID from the primary OTR
            // profile.
            intent.setClass(ContextUtils.getApplicationContext(), ChromeLauncherActivity.class);
            intent.setPackage(ContextUtils.getApplicationContext().getPackageName());
            intent.putExtra(
                    Browser.EXTRA_APPLICATION_ID,
                    ContextUtils.getApplicationContext().getPackageName());
            intent.putExtra(IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, true);
            IntentUtils.addTrustedIntentExtras(intent);

            mActivity.startActivity(intent, startActivityOptions);
            finish(FinishReason.OPEN_IN_BROWSER);
        } else if (canFinishActivity && willChromeHandleIntent) {
            // Remove observer to not trigger finishing in onAllTabsClosed() callback - we'll use
            // reparenting finish callback instead.
            mTabProvider.removeObserver(mTabObserver);
            mTabController.detachAndStartReparenting(
                    intent, startActivityOptions, () -> finish(FinishReason.REPARENTING));
        } else {
            if (mIntentDataProvider.isInfoPage()) {
                IntentHandler.startChromeLauncherActivityForTrustedIntent(intent);
            } else {
                mActivity.startActivity(intent, startActivityOptions);
                finish(FinishReason.OPEN_IN_BROWSER);
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
        mFinishReason = reason;
        // Closing the activity destroys the renderer as well. Re-create a spare renderer some
        // time after, so that we have one ready for the next tab open. This does not increase
        // memory consumption, as the current renderer goes away. We create a renderer as a lot
        // of users open several Custom Tabs in a row. The delay is there to avoid jank in the
        // transition animation when closing the tab.
        boolean warmupOnFinish = reason != REPARENTING;

        if (mFinishHandler != null) {
            mFinishHandler.onFinish(reason, warmupOnFinish);
        }
    }

    public @FinishReason int getFinishReason() {
        return mFinishReason;
    }

    /** Sets a {@link FinishHandler} to be notified when the custom tab is being closed. */
    public void setFinishHandler(FinishHandler finishHandler) {
        assert mFinishHandler == null
                : "Multiple FinishedHandlers not supported, replace with ObserverList if necessary";
        mFinishHandler = finishHandler;
    }

    /**
     * Sets a criterion to choose a page to land to when close button is pressed.
     * Only one such criterion can be set.
     * If no page in the navigation history meets the criterion, or there is no criterion, then
     * pressing close button will finish the Custom Tab activity.
     */
    public void setLandingPageOnCloseCriterion(Predicate<String> criterion) {
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
