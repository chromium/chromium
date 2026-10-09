// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.HANDLED_BY_OS;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.text.format.DateUtils;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.annotation.AnimRes;
import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.DeviceInfo;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.blink.mojom.DisplayMode;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabmodel.AllTabObserver;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.browserservices.InstalledWebappDataRegister;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.browserservices.intents.CustomTabIntentDataHolder;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaFinishHandler;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.TwaIntentHandlingStrategy;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.controller.TrustedWebActivityBrowserControlsVisibilityManager;
import org.chromium.chrome.browser.browserservices.trustedwebactivityui.sharing.TwaSharingController;
import org.chromium.chrome.browser.browserservices.ui.SharedActivityCoordinator;
import org.chromium.chrome.browser.browserservices.ui.TrustedWebActivityModel;
import org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.CurrentPageVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.EmptyVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.ClientPackageNameProvider;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.TrustedWebActivityDisclosureController;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.TrustedWebActivityOpenTimeRecorder;
import org.chromium.chrome.browser.browserservices.ui.controller.trustedwebactivity.TwaVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.webapps.AddToHomescreenVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.webapps.WebApkVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.webapps.WebappDisclosureController;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.SplashController;
import org.chromium.chrome.browser.browserservices.ui.splashscreen.webapps.WebappSplashController;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.DisclosureUiPicker;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureNotification;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosurePersistentSnackbar;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureSnackbar;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.HiddenTabHolder.HiddenTab;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.DefaultCustomTabIntentHandlingStrategy;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
import org.chromium.chrome.browser.customtabs.features.WebappInsetsConsumer;
import org.chromium.chrome.browser.customtabs.features.desktop_popup_header.DesktopPopupHeaderUtils;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManagerHolder;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager;
import org.chromium.chrome.browser.customtabs.features.toolbar.BrowserServicesThemeColorProvider;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarColorController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.CustomTabProfileType;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager.Observer;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDestroyStatus;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.SupportedProfileType;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.ToolbarThemeColorProvider;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.browser_window.BrowserWindowType;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderLayoutCoordinator;
import org.chromium.chrome.browser.ui.web_app_header.WebAppHeaderUtils;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.webapps.SameTaskWebApkActivity;
import org.chromium.chrome.browser.webapps.WebApkActivityCoordinator;
import org.chromium.chrome.browser.webapps.WebApkActivityLifecycleUmaTracker;
import org.chromium.chrome.browser.webapps.WebApkUpdateManager;
import org.chromium.chrome.browser.webapps.WebappActionsNotificationManager;
import org.chromium.chrome.browser.webapps.WebappActivityCoordinator;
import org.chromium.chrome.browser.webapps.WebappDeferredStartupWithStorageHandler;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.Supplier;

/**
 * Contains functionality which is shared between {@link WebappActivity} and {@link
 * CustomTabActivity}. Purpose of the class is to simplify merging {@link WebappActivity} and {@link
 * CustomTabActivity}.
 */
@NullMarked
public abstract class BaseCustomTabActivity extends ChromeActivity {
    private static final String KEY_CUSTOM_TAB_INTENT_DATA_HOLDER =
            "CustomTabActivity.custom_tab_intent_data_holder";

    /**
     * Prevents Tapjacking on T-. See crbug.com/40063907.
     *
     * <p>On Android T+ the platform's own {@code ActivityRecordInputSink} drops touches routed
     * through an overlay owned by another app, so the activity-level guard is only needed below
     * that.
     */
    private static final boolean sPreventTouches =
            Build.VERSION.SDK_INT < Build.VERSION_CODES.TIRAMISU;

    private final CipherFactory mCipherFactory = new CipherFactory();

    private @MonotonicNonNull BaseCustomTabRootUiCoordinator mBaseCustomTabRootUiCoordinator;
    private @MonotonicNonNull BrowserServicesIntentDataProvider mIntentDataProvider;
    private @Nullable CustomTabDelegateFactory mDelegateFactory;
    private @MonotonicNonNull CustomTabToolbarCoordinator mToolbarCoordinator;
    private @MonotonicNonNull CustomTabActivityNavigationController mNavigationController;
    private @MonotonicNonNull CustomTabActivityTabController mTabController;
    private @MonotonicNonNull CustomTabActivityTabProvider mTabProvider;
    private @Nullable CustomTabStatusBarColorProvider mStatusBarColorProvider;
    private @Nullable CustomTabActivityTabFactory mTabFactory;
    private @MonotonicNonNull CustomTabIntentHandler mCustomTabIntentHandler;
    private @Nullable CustomTabResumeManager mResumeManager;
    private @Nullable CustomTabNightModeStateController mNightModeStateController;
    private @Nullable WebappActivityCoordinator mWebappActivityCoordinator;
    private @Nullable TrustedWebActivityCoordinator mTwaCoordinator;
    private @Nullable AuthTabVerifier mAuthTabVerifier;
    private @Nullable Verifier mVerifier;
    private @Nullable FullscreenManager mFullscreenManager;
    private @MonotonicNonNull CustomTabMinimizationManagerHolder mMinimizationManagerHolder;
    private boolean mWarmupOnDestroy;
    private @MonotonicNonNull TabObserverRegistrar mTabObserverRegistrar;
    private @MonotonicNonNull CustomTabObserver mCustomTabObserver;
    private @MonotonicNonNull CustomTabNavigationEventObserver mCustomTabNavigationEventObserver;
    private @MonotonicNonNull ClientPackageNameProvider mClientPackageNameProvider;
    private @Nullable TwaFinishHandler mTwaFinishHandler;
    private @Nullable CloseButtonVisibilityManager mCloseButtonVisibilityManager;
    private @Nullable CustomTabBrowserControlsVisibilityDelegate
            mCustomTabBrowserControlsVisibilityDelegate;
    private @MonotonicNonNull CurrentPageVerifier mCurrentPageVerifier;
    private @Nullable CustomTabOrientationController mCustomTabOrientationController;
    private @MonotonicNonNull CustomTabToolbarColorController mCustomTabToolbarColorController;
    private @Nullable SplashController mSplashController;
    private @MonotonicNonNull CustomTabCompositorContentInitializer
            mCustomTabCompositorContentInitializer;
    private @Nullable CustomTabBottomBarDelegate mCustomTabBottomBarDelegate;
    private @Nullable CustomTabTabPersistencePolicy mCustomTabTabPersistencePolicy;
    private @Nullable WebappDeferredStartupWithStorageHandler
            mWebappDeferredStartupWithStorageHandler;
    private @Nullable TrustedWebActivityModel mTrustedWebActivityModel;
    private @Nullable SharedActivityCoordinator mSharedActivityCoordinator;
    private @Nullable ImmersiveModeController mImmersiveModeController;
    private @Nullable WebappInsetsConsumer mWebappInsetsConsumer;
    private @Nullable TrustedWebActivityBrowserControlsVisibilityManager
            mBrowserControlsVisibilityManager;
    private @Nullable AppHeaderCoordinator mAppHeaderCoordinator;
    private @Nullable BrowserServicesThemeColorProvider mBrowserServicesThemeColorProvider;
    private @Nullable CustomTabAllTabObserver mCustomTabAllTabObserver;
    private @Nullable CustomTabSessionHandler mCustomTabSessionHandler;

    private @Nullable ActivityLifecycleDispatcher mLifecycleDispatcherForTesting;

    private static final class CustomTabAllTabObserver
            extends CustomTabActivityTabProvider.Observer {
        private @Nullable Tab mLastTab;

        @Override
        public void onInitialTabCreated(Tab tab, int mode) {
            AllTabObserver.addCustomTab(tab);
            mLastTab = tab;
        }

        @Override
        public void onTabSwapped(Tab tab) {
            removeLastTab();
            AllTabObserver.addCustomTab(tab);
            mLastTab = tab;
        }

        @Override
        public void onAllTabsClosed() {
            removeLastTab();
        }

        private void removeLastTab() {
            if (mLastTab == null) return;
            AllTabObserver.removeCustomTab(mLastTab);
            mLastTab = null;
        }
    }

    @IntDef({PictureInPictureMode.NONE, PictureInPictureMode.MINIMIZED_CUSTOM_TAB})
    @Retention(RetentionPolicy.SOURCE)
    protected @interface PictureInPictureMode {
        int NONE = 0;
        int MINIMIZED_CUSTOM_TAB = 1;
    }

    protected @PictureInPictureMode int mLastPipMode;

    protected FullscreenManager.Observer mFullscreenObserver =
            new Observer() {
                @Override
                public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
                    // We're certain here that the Custom Tab isn't minimized, so we can let PiP
                    // be handled for any other case, i.e. fullscreen video.
                    mLastPipMode = PictureInPictureMode.NONE;
                }
            };

    protected CustomTabMinimizeDelegate.Observer mMinimizationObserver =
            minimized -> {
                // We only handle the `minimized == true` case to update the last PiP mode to MCT.
                // This is because the order between this callback and the code in
                // Activity#onPictureInPictureModeChanged isn't guaranteed, so we might end up
                // resetting the last PiP mode prematurely.
                if (minimized) {
                    mLastPipMode = PictureInPictureMode.MINIMIZED_CUSTOM_TAB;
                }
            };

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    protected boolean mShouldOverridePackage;

    /**
     * Builds {@link BrowserServicesIntentDataProvider} for this {@link CustomTabActivity}.
     *
     * <p>{@link WebappActivity} necessitates the {@code @Nullable} return annotation.
     */
    protected @Nullable BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        if (AuthTabIntentDataProvider.isAuthTabIntent(intent)) {
            return new AuthTabIntentDataProvider(intent, this, colorScheme);
        } else if (IncognitoCustomTabIntentDataProvider.isValidIncognitoIntent(
                intent, /* recordMetrics= */ true)) {
            return new IncognitoCustomTabIntentDataProvider(intent, this, colorScheme);
        }
        return new CustomTabIntentDataProvider(intent, this, colorScheme);
    }

    /** Builds {@link BrowserServicesIntentDataProvider} for this {@link CustomTabActivity}. */
    protected @Nullable BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent,
            @CustomTabsIntent.ColorScheme int colorScheme,
            @Nullable CustomTabIntentDataHolder dataHolder) {
        if (dataHolder != null) {
            if (dataHolder.mActivityType == ActivityType.AUTH_TAB) {
                return new AuthTabIntentDataProvider(intent, this, colorScheme, dataHolder);
            } else if (dataHolder.mCustomTabMode == CustomTabProfileType.INCOGNITO) {
                return new IncognitoCustomTabIntentDataProvider(
                        intent, this, colorScheme, dataHolder);
            }
            return new CustomTabIntentDataProvider(intent, this, colorScheme, dataHolder);
        }

        return buildIntentDataProvider(intent, colorScheme);
    }

    /**
     * @return The {@link BrowserServicesIntentDataProvider} for this {@link CustomTabActivity}.
     */
    public @Nullable BrowserServicesIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }

    /**
     * @return Whether the activity window is initially translucent.
     */
    public static boolean isWindowInitiallyTranslucent(Activity activity) {
        return activity instanceof TranslucentCustomTabActivity
                || (activity instanceof SameTaskWebApkActivity
                        && Build.VERSION.SDK_INT < Build.VERSION_CODES.S);
    }

    @Override
    protected NightModeStateProvider createNightModeStateProvider() {
        return getCustomTabNightModeStateController();
    }

    public CustomTabNightModeStateController getCustomTabNightModeStateController() {
        if (mNightModeStateController == null) {
            mNightModeStateController =
                    new CustomTabNightModeStateController(getLifecycleDispatcher());
        }
        return mNightModeStateController;
    }

    @Override
    protected void initializeNightModeStateProvider() {
        assumeNonNull(mNightModeStateController);
        mNightModeStateController.initialize(getDelegate(), getIntent());
    }

    @Override
    protected boolean wrapContentWithEdgeToEdgeLayout() {
        // TODO(crbug.com/392774038): Enable for e2e everywhere for PCCT.
        assumeNonNull(mIntentDataProvider);
        return super.wrapContentWithEdgeToEdgeLayout() && !mIntentDataProvider.isPartialCustomTab();
    }

    @Override
    public void onNewIntent(Intent intent) {
        // Drop the cleaner intent since it's created in order to clear up the OS share sheet.
        if (ShareHelper.isCleanerIntent(intent)) {
            return;
        }

        Intent originalIntent = getIntent();
        super.onNewIntent(intent);
        // Currently we can't handle arbitrary updates of intent parameters, so make sure
        // getIntent() returns the same intent as before.
        setIntent(originalIntent);

        // Color scheme doesn't matter here: currently we don't support updating UI using Intents.
        BrowserServicesIntentDataProvider dataProvider =
                assertNonNull(buildIntentDataProvider(intent, COLOR_SCHEME_LIGHT));

        assumeNonNull(mCustomTabIntentHandler);
        mCustomTabIntentHandler.onNewIntent(dataProvider);
    }

    @Override
    public void setContentView(int layoutResID) {
        assert mIntentDataProvider != null;
        if (WebAppHeaderUtils.isWebAppHeaderEnabled(mIntentDataProvider)) {
            final View rootLayout =
                    getLayoutInflater().inflate(WebAppHeaderUtils.getWebAppHeaderLayoutId(), null);
            final LinearLayout linearLayout =
                    rootLayout.findViewById(WebAppHeaderUtils.getWebAppHeaderContentId());
            getLayoutInflater().inflate(layoutResID, linearLayout, true);
            super.setContentView(rootLayout);
        } else if (DesktopPopupHeaderUtils.isDesktopPopupHeaderEnabled(mIntentDataProvider)) {
            final View rootLayout =
                    getLayoutInflater().inflate(DesktopPopupHeaderUtils.getMainLayoutId(), null);
            final FrameLayout contentLayout =
                    rootLayout.findViewById(DesktopPopupHeaderUtils.getContentViewId());
            getLayoutInflater().inflate(layoutResID, contentLayout, true);
            super.setContentView(rootLayout);
        } else {
            super.setContentView(layoutResID);
        }
    }

    @Override
    public void setContentView(View view) {
        assert mIntentDataProvider != null;
        if (WebAppHeaderUtils.isWebAppHeaderEnabled(mIntentDataProvider)) {
            final View rootLayout =
                    getLayoutInflater().inflate(WebAppHeaderUtils.getWebAppHeaderLayoutId(), null);
            final LinearLayout linearLayout =
                    rootLayout.findViewById(WebAppHeaderUtils.getWebAppHeaderContentId());
            linearLayout.addView(view, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            super.setContentView(rootLayout);
        } else if (DesktopPopupHeaderUtils.isDesktopPopupHeaderEnabled(mIntentDataProvider)) {
            final View rootLayout =
                    getLayoutInflater().inflate(DesktopPopupHeaderUtils.getMainLayoutId(), null);
            final FrameLayout contentLayout =
                    rootLayout.findViewById(DesktopPopupHeaderUtils.getContentViewId());
            contentLayout.addView(view, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            super.setContentView(rootLayout);
        } else {
            super.setContentView(view);
        }
    }

    @Override
    public void setContentView(View view, ViewGroup.LayoutParams params) {
        assert mIntentDataProvider != null;
        if (WebAppHeaderUtils.isWebAppHeaderEnabled(mIntentDataProvider)) {
            final View rootLayout =
                    getLayoutInflater().inflate(WebAppHeaderUtils.getWebAppHeaderLayoutId(), null);
            rootLayout.setLayoutParams(params);

            final LinearLayout linearLayout =
                    rootLayout.findViewById(WebAppHeaderUtils.getWebAppHeaderContentId());
            linearLayout.addView(view, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);

            super.setContentView(rootLayout);
        } else if (DesktopPopupHeaderUtils.isDesktopPopupHeaderEnabled(mIntentDataProvider)) {
            final View rootLayout =
                    getLayoutInflater().inflate(DesktopPopupHeaderUtils.getMainLayoutId(), null);
            rootLayout.setLayoutParams(params);

            final FrameLayout contentLayout =
                    rootLayout.findViewById(DesktopPopupHeaderUtils.getContentViewId());
            contentLayout.addView(view, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);

            super.setContentView(rootLayout);
        } else {
            super.setContentView(view, params);
        }
    }

    @Override
    protected RootUiCoordinator createRootUiCoordinator() {
        var windowAndroid = assertNonNull(getWindowAndroid());
        var edgeToEdgeManager = assertNonNull(getEdgeToEdgeManager());
        mBaseCustomTabRootUiCoordinator =
                new BaseCustomTabRootUiCoordinator(
                        this,
                        getShareDelegateSupplier(),
                        getActivityTabProvider(),
                        getCustomTabActivityTabProvider(),
                        mTabModelProfileSupplier,
                        mBookmarkModelSupplier,
                        mTabBookmarkerSupplier,
                        getTabModelSelectorSupplier(),
                        getBrowserControlsManager(),
                        windowAndroid,
                        getActivityResultTracker(),
                        getChromeAndroidTaskSupplier(),
                        getLifecycleDispatcher(),
                        getMultiWindowModeStateDispatcher(),
                        getLayoutManagerSupplier(),
                        /* menuOrKeyboardActionController= */ this,
                        this::getActivityThemeColor,
                        getModalDialogManagerSupplier().asNonNull(),
                        /* appMenuBlocker= */ this,
                        this::supportsAppMenu,
                        getTabCreatorManagerSupplier(),
                        getFullscreenManager(),
                        getCompositorViewHolderSupplier(),
                        getTabContentManagerSupplier(),
                        getSnackbarManagerSupplier(),
                        mEdgeToEdgeControllerSupplier,
                        getActivityType(),
                        this::isInOverviewMode,
                        /* appMenuDelegate= */ this,
                        /* statusBarColorProvider= */ this,
                        getEphemeralTabCoordinatorSupplier(),
                        getIntentRequestTracker(),
                        this::getCustomTabToolbarCoordinator,
                        () -> assertNonNull(mIntentDataProvider),
                        mBackPressManager,
                        this::getCustomTabActivityTabController,
                        () -> getCustomTabMinimizationManagerHolder().getMinimizationManager(),
                        () -> getCustomTabActivityNavigationController().openCurrentUrlInBrowser(),
                        edgeToEdgeManager,
                        getAppHeaderCoordinator(),
                        this::getBrowserServicesThemeColorProvider,
                        getClientPackageNameProvider().get());
        return mBaseCustomTabRootUiCoordinator;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        return new ActivityProfileProvider(getLifecycleDispatcher()) {

            @Override
            protected @Nullable OtrProfileId createOffTheRecordProfileId() {
                assumeNonNull(mIntentDataProvider);
                switch (mIntentDataProvider.getCustomTabMode()) {
                    case CustomTabProfileType.INCOGNITO:
                        // If an incognito popup is being created by Chrome, this should use
                        // the same primary OTR profile associated with the requester's profile.
                        if (mIntentDataProvider.getUiType() == CustomTabsUiType.POPUP) {
                            if (!mIntentDataProvider.isOpenedByChrome()) {
                                throw new IllegalStateException(
                                        "Incognito CCTs should have unique OTR profiles unless"
                                                + " Chrome opened them as a popup window.");
                            }
                            return null;
                        }
                        return OtrProfileId.createUniqueIncognitoCctId();
                    case CustomTabProfileType.EPHEMERAL:
                        return OtrProfileId.createUnique("CCT:Ephemeral");
                    default:
                        throw new IllegalStateException(
                                "Attempting to create an OTR profile in a non-OTR session");
                }
            }
        };
    }

    @Override
    public boolean shouldAllocateChildConnection() {
        return getCustomTabActivityTabController().shouldAllocateChildConnection();
    }

    @Override
    protected boolean shouldPreferLightweightFre(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(
                intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
    }

    private void initializeForWebappOrWebApk() {
        assert mIntentDataProvider != null;
        mWebappActivityCoordinator =
                new WebappActivityCoordinator(
                        mIntentDataProvider,
                        this,
                        getWebappDeferredStartupWithStorageHandler(),
                        getLifecycleDispatcher());
        // Classes manage their own lifecycles and just need to be initialized.
        new WebappActionsNotificationManager(
                getCustomTabActivityTabProvider(), mIntentDataProvider, getLifecycleDispatcher());
        new WebappSplashController(
                this, getSplashController(), getTabObserverRegistrar(), mIntentDataProvider);
        getSharedActivityCoordinator();

        if (mIntentDataProvider.isWebApkActivity()) initializeForWebApk();
    }

    private void initializeForWebApk() {
        assert mIntentDataProvider != null;
        // Classes manage their own lifecycles and just need to be initialized.
        new WebApkActivityCoordinator(
                mIntentDataProvider,
                this::createWebApkUpdateManager,
                getWebappDeferredStartupWithStorageHandler(),
                getLifecycleDispatcher());
        createDisclosureSnackbar();
        new WebApkActivityLifecycleUmaTracker(
                this,
                mIntentDataProvider,
                getSplashControllerSupplier(),
                getStartupMetricsTracker(),
                this::getSavedInstanceState,
                getWebappDeferredStartupWithStorageHandler(),
                getLifecycleDispatcher());
        new WebappDisclosureController(
                getTrustedWebActivityModel(),
                getLifecycleDispatcher(),
                getCurrentPageVerifier(),
                mIntentDataProvider,
                getWebappDeferredStartupWithStorageHandler());
    }

    private void initializeForTwa() {
        assert mIntentDataProvider != null;
        // Classes manage their own lifecycles and just need to be initialized.
        mTwaCoordinator =
                new TrustedWebActivityCoordinator(
                        this,
                        getSharedActivityCoordinator(),
                        getCurrentPageVerifier(),
                        getClientPackageNameProvider(),
                        getSplashControllerSupplier(),
                        mIntentDataProvider);
        new DisclosureUiPicker(
                this::createDisclosurePersistentSnackbar,
                this::createDisclosureSnackbar,
                this::createDisclosureNotification,
                mIntentDataProvider,
                getLifecycleDispatcher());
        var windowAndroid = assertNonNull(getWindowAndroid());
        new TrustedWebActivityDisclosureController(
                windowAndroid,
                getTrustedWebActivityModel(),
                getLifecycleDispatcher(),
                getCurrentPageVerifier(),
                getClientPackageNameProvider());
    }

    /**
     * Return true when the activity has been launched in a separate task. The default behavior is
     * to reuse the same task and put the activity on top of the previous one (i.e hiding it). A
     * separate task creates a new entry in the Android recent screen.
     */
    protected boolean useSeparateTask() {
        final int separateTaskFlags =
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        return (getIntent().getFlags() & separateTaskFlags) != 0;
    }

    /**
     * Whether a web app with {@code displayMode} uses the short-edges cutout mode, where the
     * display cutout controller owns the window's edge-to-edge state. Fullscreen web apps are
     * covered by the feature itself; standalone web apps additionally require the {@code
     * enable_standalone} parameter, since the experiment targets fullscreen web apps. Other display
     * modes keep the pre-feature behavior.
     */
    public static boolean isShortEdgesCutoutModeEnabledForDisplayMode(
            @DisplayMode.EnumType int displayMode) {
        if (!ChromeFeatureList.sWebAppShortEdgesCutoutMode.isEnabled()) return false;
        if (displayMode == DisplayMode.FULLSCREEN) return true;
        return displayMode == DisplayMode.STANDALONE
                && ChromeFeatureList.sWebAppShortEdgesCutoutModeStandalone.getValue();
    }

    @Override
    public void performPreInflationStartup() {
        // This must be requested before adding content.
        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);

        // Parse the data from the Intent before calling super to allow the Intent to customize
        // the Activity parameters, including the background of the page.
        // Note that color scheme is fixed for the lifetime of Activity: if the system setting
        // changes, we recreate the activity.
        Bundle savedInstanceState = getSavedInstanceState();
        CustomTabIntentDataHolder dataHolder =
                savedInstanceState != null
                        ? IntentUtils.safeGetParcelable(
                                savedInstanceState, KEY_CUSTOM_TAB_INTENT_DATA_HOLDER)
                        : null;
        var intentDataProvider = buildIntentDataProvider(getIntent(), getColorScheme(), dataHolder);

        if (intentDataProvider == null) {
            // |mIntentDataProvider| is null if the WebAPK server vended an invalid WebAPK (WebAPK
            // correctly signed, mandatory <meta-data> missing).
            this.finishAndRemoveTask();
            return;
        }
        mIntentDataProvider = intentDataProvider;

        InstalledWebappDataRegister.prefetchPreferences();

        if (mIntentDataProvider.isWebappOrWebApkActivity()
                && isShortEdgesCutoutModeEnabledForDisplayMode(
                        mIntentDataProvider.getResolvedDisplayMode())) {
            // The window's edge-to-edge state is owned by token holders (display cutout
            // controller, immersive mode). While any token is held, withhold system bar and
            // display cutout insets from the edge-to-edge root layout so the web app content
            // draws under zero-height bars; releasing the last token restores the fitted
            // layout, e.g. with the CCT toolbar below the status bar after an off-origin
            // navigation.
            mWebappInsetsConsumer = new WebappInsetsConsumer(getInsetObserver());
            assumeNonNull(getEdgeToEdgeManager())
                    .getEdgeToEdgeStateProvider()
                    .getSupplier()
                    .addSyncObserverAndCallIfNonNull(mWebappInsetsConsumer::drawEdgeToEdge);
        }

        mClientPackageNameProvider =
                new ClientPackageNameProvider(
                        getLifecycleDispatcher(), mIntentDataProvider, getSavedInstanceState());

        // Hidden tabs shouldn't be used in incognito/ephemeral CCT, since they are always
        // created with regular profile. Also restrict usage if restoring a tab state.
        mResumeManager = maybeCreateResumeManager();
        boolean shouldRestore = mResumeManager != null && mResumeManager.isTabResumptionRequested();
        HiddenTab hiddenTab =
                (mIntentDataProvider.isOffTheRecord() || shouldRestore)
                        ? null
                        : CustomTabActivityTabController.takeHiddenTab(mIntentDataProvider);

        if (hiddenTab != null) {
            mTabProvider = new CustomTabActivityTabProvider(hiddenTab.url);
            mTabObserverRegistrar = hiddenTab.tabObserverRegistrar;
            mCustomTabObserver = hiddenTab.customTabObserver;
            mCustomTabNavigationEventObserver = hiddenTab.customTabNavigationEventObserver;
        } else {
            mTabProvider = new CustomTabActivityTabProvider(null);
            mTabObserverRegistrar = new TabObserverRegistrar();
            mCustomTabObserver =
                    new CustomTabObserver(
                            mIntentDataProvider.isOpenedByChrome(),
                            mIntentDataProvider.getSession());
            mCustomTabNavigationEventObserver =
                    new CustomTabNavigationEventObserver(
                            mIntentDataProvider.getSession(), /* forPrerender= */ false);
        }
        // Some information were not available when creating a hidden tab, now it is the time to
        // attach them.
        mCustomTabObserver.setTwaStartupMetadata(
                mIntentDataProvider.getAndroidBrowserHelperVersion(),
                mIntentDataProvider.getTwaStartupUptimeMillis());
        mTabObserverRegistrar.associateWithActivity(getLifecycleDispatcher(), mTabProvider);

        mCustomTabAllTabObserver = new CustomTabAllTabObserver();
        mTabProvider.addObserver(mCustomTabAllTabObserver);

        mCurrentPageVerifier =
                new CurrentPageVerifier(
                        getCustomTabActivityTabProvider(),
                        mIntentDataProvider,
                        getVerifier(),
                        getTabObserverRegistrar(),
                        getLifecycleDispatcher());

        if (mIntentDataProvider.isAuthTab()) {
            mAuthTabVerifier =
                    new AuthTabVerifier(
                            this,
                            getLifecycleDispatcher(),
                            mIntentDataProvider,
                            getCustomTabActivityTabProvider());
        }

        new CustomTabActivityClientConnectionKeeper(mIntentDataProvider, getLifecycleDispatcher());

        new CustomTabActivityLifecycleUmaTracker(
                this, mIntentDataProvider, this::getSavedInstanceState, getLifecycleDispatcher());

        super.performPreInflationStartup();

        mCustomTabToolbarColorController =
                new CustomTabToolbarColorController(
                        this,
                        getBrowserServicesThemeColorProvider(),
                        getAppHeaderCoordinator(),
                        mIntentDataProvider,
                        getLifecycleDispatcher());

        mCustomTabCompositorContentInitializer =
                new CustomTabCompositorContentInitializer(
                        this,
                        getCompositorViewHolderSupplier(),
                        getTabContentManagerSupplier(),
                        /* compositorViewHolderInitializer= */ this,
                        getToolbarThemeColorProvider(),
                        getLifecycleDispatcher());

        var windowAndroid = assertNonNull(getWindowAndroid());
        mCustomTabBottomBarDelegate =
                new CustomTabBottomBarDelegate(
                        this,
                        windowAndroid,
                        mIntentDataProvider,
                        getBrowserControlsManager(),
                        getCustomTabNightModeStateController(),
                        getCustomTabActivityTabProvider(),
                        getCustomTabCompositorContentInitializer());

        mTabController =
                new CustomTabActivityTabController(
                        this,
                        getProfileProviderSupplier(),
                        getCustomTabDelegateFactory(),
                        mIntentDataProvider,
                        getTabObserverRegistrar(),
                        getCompositorViewHolderSupplier(),
                        getCustomTabTabPersistencePolicy(),
                        getCustomTabActivityTabFactory(),
                        getCustomTabObserver(),
                        getCustomTabNavigationEventObserver(),
                        getActivityTabProvider(),
                        getCustomTabActivityTabProvider(),
                        this::getSavedInstanceState,
                        windowAndroid,
                        this,
                        getCipherFactory(),
                        getLifecycleDispatcher(),
                        mResumeManager);

        getCustomTabActivityTabFactory().setActivityType(getActivityType());
        // Finish reparenting as soon as possible as it may be blocking navigation.
        getCustomTabActivityTabController()
                .setUpInitialTab(hiddenTab != null ? hiddenTab.tab : null);

        mMinimizationManagerHolder =
                new CustomTabMinimizationManagerHolder(
                        this,
                        this::getCustomTabActivityNavigationController,
                        getActivityTabProvider(),
                        mIntentDataProvider,
                        this::getSavedInstanceState,
                        getLifecycleDispatcher());

        CloseButtonNavigator closeButtonNavigator =
                new CloseButtonNavigator(
                        getCustomTabActivityTabController(),
                        getCustomTabActivityTabProvider(),
                        mIntentDataProvider,
                        getCustomTabMinimizationManagerHolder());

        mNavigationController =
                new CustomTabActivityNavigationController(
                        getCustomTabActivityTabController(),
                        getCustomTabActivityTabProvider(),
                        mIntentDataProvider,
                        getCustomTabObserver(),
                        closeButtonNavigator,
                        this,
                        getLifecycleDispatcher());

        mToolbarCoordinator =
                new CustomTabToolbarCoordinator(
                        mIntentDataProvider,
                        getCustomTabActivityTabProvider(),
                        this,
                        windowAndroid,
                        getBrowserControlsManager(),
                        getCustomTabActivityNavigationController(),
                        getCloseButtonVisibilityManager(),
                        getCustomTabBrowserControlsVisibilityDelegate(),
                        getCustomTabToolbarColorController(),
                        getAppHeaderCoordinator(),
                        getCustomTabCompositorContentInitializer());

        CustomTabIntentHandlingStrategy customTabIntentHandlingStrategy =
                new DefaultCustomTabIntentHandlingStrategy(
                        getCustomTabActivityTabProvider(),
                        getCustomTabActivityNavigationController(),
                        getCustomTabObserver(),
                        getVerifier(),
                        getCurrentPageVerifier(),
                        this);
        if (getActivityType() == ActivityType.TRUSTED_WEB_ACTIVITY
                || getActivityType() == ActivityType.WEB_APK) {
            TwaSharingController controller =
                    new TwaSharingController(
                            getCustomTabActivityTabProvider(),
                            getCustomTabActivityNavigationController(),
                            getVerifier());
            customTabIntentHandlingStrategy =
                    new TwaIntentHandlingStrategy(customTabIntentHandlingStrategy, controller);
        }

        mCustomTabIntentHandler =
                new CustomTabIntentHandler(
                        getCustomTabActivityTabProvider(),
                        mIntentDataProvider,
                        customTabIntentHandlingStrategy,
                        this,
                        getCustomTabMinimizationManagerHolder());

        getCustomTabActivityNavigationController().setFinishHandler(this::handleFinishAndClose);

        mBackPressManager.setFallbackOnBackPressed(this::handleBackPressed);
        mBackPressManager.addHandler(
                getCustomTabActivityNavigationController(),
                BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB);
        if (CustomTabActivityNavigationController.supportsPredictiveBackGesture()) {
            mBackPressManager.addOnSystemNavigationObserver(
                    getCustomTabActivityNavigationController());
        }

        mCustomTabSessionHandler =
                new CustomTabSessionHandler(
                        mIntentDataProvider,
                        getCustomTabActivityTabProvider(),
                        this::getCustomTabToolbarCoordinator,
                        this::getCustomTabBottomBarDelegate,
                        mCustomTabIntentHandler,
                        this,
                        getLifecycleDispatcher());

        // We need the CustomTabIncognitoManager for all OffTheRecord profiles to ensure
        // that they are destroyed when a CCT session ends.
        if (intentDataProvider.isOffTheRecord()) {
            new CustomTabIncognitoManager(
                    this,
                    getCustomTabActivityNavigationController(),
                    mIntentDataProvider,
                    getProfileProviderSupplier(),
                    getLifecycleDispatcher());
        }

        if (intentDataProvider.isWebappOrWebApkActivity()) initializeForWebappOrWebApk();
        if (mIntentDataProvider.isTrustedWebActivity()) initializeForTwa();

        getCustomTabDelegateFactory()
                .setEphemeralTabCoordinatorSupplier(
                        mRootUiCoordinator.getEphemeralTabCoordinatorSupplier());

        new CustomTabDownloadObserver(this, getTabObserverRegistrar(), getLifecycleDispatcher());

        if (mIntentDataProvider.isTrustedWebActivity()) {
            new TrustedWebActivityOpenTimeRecorder(
                    getCurrentPageVerifier(), getActivityTabProvider(), getLifecycleDispatcher());
        }

        new CustomTabTaskDescriptionHelper(
                this,
                getCustomTabActivityTabProvider(),
                getTabObserverRegistrar(),
                mIntentDataProvider,
                getToolbarThemeColorProvider(),
                getLifecycleDispatcher());

        if (mIntentDataProvider.isPartialCustomTab()) {
            @AnimRes
            int startAnimResId =
                    PartialCustomTabDisplayManager.getStartAnimationOverride(
                            this, mIntentDataProvider, mIntentDataProvider.getAnimationEnterRes());
            overridePendingTransition(startAnimResId, R.anim.no_anim);
        }

        WebappExtras webappExtras = mIntentDataProvider.getWebappExtras();
        if (webappExtras != null) {
            // Set the title for web apps so that TalkBack says the web app's short name instead of
            // 'Chrome' or the activity's label ("Web app") when either launching the web app or
            // bringing it to the foreground via Android Recents.
            setTitle(webappExtras.shortName);
        }

        mFullscreenManager = getFullscreenManager();

        getCustomTabMinimizationManagerHolder()
                .maybeCreateMinimizationManager(mTabModelProfileSupplier);
        var minimizationManager = getCustomTabMinimizationManagerHolder().getMinimizationManager();
        if (minimizationManager != null) {
            getFullscreenManager().addObserver(mFullscreenObserver);
            minimizationManager.addObserver(mMinimizationObserver);
        }
    }

    @Override
    protected void onDestroyInternal() {
        if (mResumeManager != null) {
            mResumeManager.destroy();
            mResumeManager = null;
        }

        if (mCustomTabSessionHandler != null) {
            mCustomTabSessionHandler.onDestroy();
            mCustomTabSessionHandler = null;
        }

        if (mWebappInsetsConsumer != null) {
            mWebappInsetsConsumer.destroy();
            mWebappInsetsConsumer = null;
        }

        if (mFullscreenManager != null) {
            mFullscreenManager.removeObserver(mFullscreenObserver);
            mFullscreenManager = null;
        }
        if (mMinimizationManagerHolder != null) {
            var minimizationManager = mMinimizationManagerHolder.getMinimizationManager();
            if (minimizationManager != null) {
                minimizationManager.removeObserver(mMinimizationObserver);
            }
        }

        if (mWarmupOnDestroy) {
            RecordHistogram.recordBooleanHistogram("CustomTabs.SpareRenderer", true);
            Profile profile =
                    assumeNonNull(getProfileProviderSupplier().get()).getOriginalProfile();
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT, () -> CustomTabsConnection.createSpareTab(profile));
        } else {
            RecordHistogram.recordBooleanHistogram("CustomTabs.SpareRenderer", false);
        }

        if (mTabController != null) {
            mTabController.destroy();
        }

        if (mBrowserControlsVisibilityManager != null) {
            mBrowserControlsVisibilityManager.destroy();
        }

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM
                && mAppHeaderCoordinator != null) {
            mAppHeaderCoordinator.destroy();
            mAppHeaderCoordinator = null;
        }

        if (mBrowserServicesThemeColorProvider != null) {
            mBrowserServicesThemeColorProvider.destroy();
            mBrowserServicesThemeColorProvider = null;
        }

        if (mCustomTabBottomBarDelegate != null) {
            mCustomTabBottomBarDelegate.destroy();
            mCustomTabBottomBarDelegate = null;
        }

        if (mTabProvider != null && mTabProvider.getTab() != null) {
            AllTabObserver.removeCustomTab(mTabProvider.getTab());
        }
        if (mCustomTabAllTabObserver != null) {
            if (mTabProvider != null) {
                mTabProvider.removeObserver(mCustomTabAllTabObserver);
            }
            mCustomTabAllTabObserver = null;
        }

        super.onDestroyInternal();
    }

    private int getColorScheme() {
        if (mNightModeStateController != null) {
            return mNightModeStateController.isInNightMode()
                    ? COLOR_SCHEME_DARK
                    : COLOR_SCHEME_LIGHT;
        }
        assert false : "NightModeStateController should have been already created";
        return COLOR_SCHEME_LIGHT;
    }

    /**
     * @return {@link ThemeColorProvider} for the toolbar.
     */
    public ToolbarThemeColorProvider getToolbarThemeColorProvider() {
        return mRootUiCoordinator.getToolbarThemeColorProvider();
    }

    @Override
    public void initializeState() {
        super.initializeState();

        // TODO(pkotwicz): Determine whether finishing tab initialization in initializeState() has a
        // positive performance impact.
        assumeNonNull(mIntentDataProvider);
        if (mIntentDataProvider.isWebappOrWebApkActivity()) {
            getCustomTabActivityTabController().finishNativeInitialization();
        }
    }

    @Override
    public void finishNativeInitialization() {
        if (isTaskRoot()) {
            getProfileProviderSupplier()
                    .runSyncOrOnAvailable(
                            (ProfileProvider profileProvider) ->
                                    UsageStatsService.createPageViewObserverIfEnabled(
                                            this,
                                            getLifecycleDispatcher(),
                                            profileProvider.getOriginalProfile(),
                                            getActivityTabProvider(),
                                            getTabContentManagerSupplier().asNonNull()));
        }
        assumeNonNull(mIntentDataProvider);
        if (!mIntentDataProvider.isWebappOrWebApkActivity()) {
            getCustomTabActivityTabController().finishNativeInitialization();
        } else {
            // Webapp/WebAPK activities don't run CustomTabActivity#performPreInflationStartup,
            // which is where CCT applies its navigation bar color. Apply it here so installed web
            // apps color the navigation bar from the manifest theme color. This runs post-native
            // because the color depends on a feature flag.
            updateNavigationBarColor();
            // In edge-to-edge the navigation buttons render over page content; follow dynamic
            // page theme-color changes and edge-to-edge transitions so the buttons stay
            // readable, e.g. dark buttons over a page that switches its theme color to white.
            getBrowserServicesThemeColorProvider()
                    .addThemeColorObserver((color, shouldAnimate) -> updateNavigationBarColor());
            if (getEdgeToEdgeManager() != null) {
                getEdgeToEdgeManager()
                        .getEdgeToEdgeStateProvider()
                        .getSupplier()
                        .addSyncObserver(drawingEdgeToEdge -> updateNavigationBarColor());
            }
        }

        super.finishNativeInitialization();
    }

    /**
     * Applies the navigation bar color (and divider) for the current activity based on the intent /
     * manifest theme color and the current edge-to-edge state.
     */
    protected void updateNavigationBarColor() {
        // Webapp/WebAPK window tokens draw content behind the navigation bar only when the
        // webapp insets consumer prevents the root layout from padding it.
        boolean windowDrawsEdgeToEdge =
                mWebappInsetsConsumer != null
                        && getEdgeToEdgeManager() != null
                        && Boolean.TRUE.equals(
                                getEdgeToEdgeManager()
                                        .getEdgeToEdgeStateProvider()
                                        .getSupplier()
                                        .get());
        boolean drawEdgeToEdge =
                windowDrawsEdgeToEdge
                        || (mEdgeToEdgeControllerSupplier.get() != null
                                && mEdgeToEdgeControllerSupplier.get().isDrawingToEdge()
                                && mEdgeToEdgeControllerSupplier.get().isPageOptedIntoEdgeToEdge());
        var systemBarColorHelper =
                getEdgeToEdgeManager() != null
                        ? getEdgeToEdgeManager().getEdgeToEdgeSystemBarColorHelper()
                        : null;
        Integer edgeToEdgeContentColor =
                drawEdgeToEdge ? getBrowserServicesThemeColorProvider().getThemeColor() : null;
        CustomTabNavigationBarController.update(
                getWindow(),
                assertNonNull(getIntentDataProvider()),
                this,
                drawEdgeToEdge,
                systemBarColorHelper,
                edgeToEdgeContentColor);
    }

    @Override
    protected boolean maybeApplyCustomizedColors() {
        return false;
    }

    @Override
    protected TabModelOrchestrator createTabModelOrchestrator() {
        return getCustomTabActivityTabFactory().createTabModelOrchestrator();
    }

    @Override
    protected @TabDestroyStatus int destroyTabModels() {
        @TabDestroyStatus int status = TabDestroyStatus.NO_SHUTDOWN;
        if (mTabFactory != null) {
            status = mTabFactory.destroyTabModelOrchestrator();
        }

        final var tab = mTabProvider != null ? mTabProvider.getTab() : null;
        if (tab == null) {
            return status;
        }

        final var tabModelSelector = getTabModelSelectorSupplier().get();
        // Keep a tab alive when re-parenting.
        final var isReparenting =
                tabModelSelector != null
                        && tabModelSelector.isReparentingInProgress()
                        && AsyncTabParamsManagerSingleton.getInstance()
                                .hasParamsForTabId(tab.getId());
        // If tab models have not been initialized, any early created tabs would leak.
        if (!tab.isDestroyed() && !isReparenting) {
            @TabDestroyStatus int tabStatus = tab.destroy();
            if (tabStatus == TabDestroyStatus.SLOW_SHUTDOWN) {
                status = TabDestroyStatus.SLOW_SHUTDOWN;
            } else if (tabStatus == TabDestroyStatus.FAST_SHUTDOWN
                    && status != TabDestroyStatus.SLOW_SHUTDOWN) {
                status = TabDestroyStatus.FAST_SHUTDOWN;
            }
        }
        return status;
    }

    @Override
    protected void createTabModels() {
        getCustomTabActivityTabFactory().createTabModels();
    }

    @Override
    protected Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return getCustomTabActivityTabFactory().createTabCreators();
    }

    @Override
    public @ActivityType int getActivityType() {
        assumeNonNull(mIntentDataProvider);
        return mIntentDataProvider.getActivityType();
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        var tabModelOrchestrator = getCustomTabActivityTabFactory().getTabModelOrchestrator();
        tabModelOrchestrator.onNativeLibraryReady(getTabContentManager());
        // This ensures that an off-the-record TabModel is the current model before it is needed.
        assumeNonNull(mIntentDataProvider);
        boolean isOffTheRecord = mIntentDataProvider.isOffTheRecord();
        var tabModelSelector = assumeNonNull(tabModelOrchestrator.getTabModelSelector());
        tabModelSelector.selectModel(isOffTheRecord);

        @BrowserWindowType Integer browserWindowType = getSupportedBrowserWindowType();
        if (browserWindowType != null) {
            // Custom tabs don't mix OTR and normal tabs in the same window, so it is fine to
            // not pass MIXED as the supported profile type even on non-desktop form factors.
            @SupportedProfileType
            int supportedProfileType =
                    isOffTheRecord
                            ? SupportedProfileType.OFF_THE_RECORD
                            : SupportedProfileType.REGULAR;
            initializeChromeAndroidTask(
                    browserWindowType,
                    tabModelSelector,
                    supportedProfileType,
                    /* multiInstanceManager= */ null);
        }
    }

    @Override
    public TabModelSelectorImpl getTabModelSelector() {
        return (TabModelSelectorImpl) super.getTabModelSelector();
    }

    @Override
    public @Nullable Tab getActivityTab() {
        assumeNonNull(mTabProvider);
        return mTabProvider.getTab();
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        // Menu icon is at the other side of the toolbar relative to the close button, so it will be
        // at the start when the close button is at the end.
        assumeNonNull(mIntentDataProvider);
        assumeNonNull(mBaseCustomTabRootUiCoordinator);
        boolean isMenuIconAtStart =
                mIntentDataProvider.getCloseButtonPosition() == CLOSE_BUTTON_POSITION_END;
        var toolbarManager = assertNonNull(getToolbarManager());
        return new CustomTabAppMenuPropertiesDelegate(
                this,
                getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(),
                getTabModelSelector(),
                toolbarManager,
                getWindow().getDecorView(),
                mBookmarkModelSupplier,
                getVerifier(),
                mIntentDataProvider.getUiType(),
                mIntentDataProvider.getMenuTitles(),
                mIntentDataProvider.isOpenedByChrome(),
                mIntentDataProvider.shouldShowShareMenuItem(),
                mIntentDataProvider.shouldShowStarButton(),
                mIntentDataProvider.shouldShowDownloadButton(),
                mIntentDataProvider.getCustomTabMode() == CustomTabProfileType.INCOGNITO,
                mIntentDataProvider.isOffTheRecord(),
                isMenuIconAtStart,
                mBaseCustomTabRootUiCoordinator.getReadAloudControllerSupplier(),
                mBaseCustomTabRootUiCoordinator::getContextualPageActionController,
                mIntentDataProvider.getClientPackageNameIdentitySharing() != null,
                mBaseCustomTabRootUiCoordinator.getPageZoomManager(),
                mBaseCustomTabRootUiCoordinator.getOpenInAppMenuItemProvider(),
                mBaseCustomTabRootUiCoordinator::getWebAppHeaderLayoutCoordinator);
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getToolbarLayoutId() {
        return R.layout.new_custom_tab_toolbar;
    }

    @Override
    protected int getToolbarLayoutHeightResId() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    public boolean shouldPostDeferredStartupForReparentedTab() {
        if (!super.shouldPostDeferredStartupForReparentedTab()) return false;

        // Check {@link CustomTabActivityTabProvider#getInitialTabCreationMode()} because the
        // tab has not yet started loading in the common case due to ordering of
        // {@link ChromeActivity#onStartWithNative()} and
        // {@link CustomTabActivityTabController#onFinishNativeInitialization()}.
        assumeNonNull(mTabProvider);
        @TabCreationMode int mode = mTabProvider.getInitialTabCreationMode();
        return (mode == TabCreationMode.HIDDEN || mode == TabCreationMode.EARLY);
    }

    protected boolean handleBackPressed() {
        return getCustomTabActivityNavigationController()
                .navigateOnBack(FinishReason.USER_NAVIGATION);
    }

    @Override
    public void finish() {
        super.finish();

        // Calling #overridePendingTransition() is known to cause the device to freeze on
        // automotive. See crbug.com/445873259.
        if (DeviceInfo.isAutomotive()) return;

        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (intentDataProvider != null && intentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            // |mShouldOverridePackage| is used in #getPackageName for |overridePendingTransition|
            // to pick up the client package name regardless of custom tabs connection.
            overridePendingTransition(
                    intentDataProvider.getAnimationEnterRes(),
                    intentDataProvider.getAnimationExitRes());
            mShouldOverridePackage = false;
        } else if (intentDataProvider != null && intentDataProvider.isOpenedByChrome()) {
            overridePendingTransition(R.anim.no_anim, R.anim.activity_close_exit);
        }
    }

    /**
     * Internal implementation that finishes the activity and removes the references from Android
     * recents.
     */
    protected void handleFinishAndClose(@FinishReason int reason, boolean warmupOnFinish) {
        // Delay until we're destroyed to avoid jank in the transition animation when closing the
        // tab.
        mWarmupOnDestroy = warmupOnFinish;
        Runnable defaultBehavior =
                () -> {
                    if (useSeparateTask()) {
                        this.finishAndRemoveTask();
                    } else {
                        finish();
                    }
                };
        assumeNonNull(mIntentDataProvider);
        if (mIntentDataProvider.isTrustedWebActivity()
                || mIntentDataProvider.isWebappOrWebApkActivity()) {
            // TODO(pshmakov): extract all finishing logic from BaseCustomTabActivity.
            // In addition to TwaFinishHandler, create DefaultFinishHandler, PaymentsFinishHandler,
            // and SeparateTaskActivityFinishHandler, all implementing
            // CustomTabActivityNavigationController#FinishHandler. Pass the mode enum into
            // CustomTabActivityModule, so that it can provide the correct implementation.
            getTwaFinishHandler().onFinish(defaultBehavior);
        } else if (mIntentDataProvider.isPartialCustomTab()) {
            // WebContents is missing during the close animation due to android:windowIsTranslucent.
            // We let partial CCT handle the animation.
            assumeNonNull(mBaseCustomTabRootUiCoordinator);
            mBaseCustomTabRootUiCoordinator.handleCloseAnimation(defaultBehavior);
        } else if (reason != HANDLED_BY_OS) {
            // Back events handled by the OS, such as predictive gesture, are removed by the OS.
            // There is no need in overriding their transitions.
            defaultBehavior.run();
        }
    }

    @Override
    public boolean canShowAppMenu() {
        assumeNonNull(mToolbarCoordinator);
        if (getActivityTab() == null || !mToolbarCoordinator.toolbarIsInitialized()) return false;

        return super.canShowAppMenu();
    }

    @Override
    public int getActivityThemeColor() {
        assumeNonNull(mIntentDataProvider);
        if (mIntentDataProvider.getColorProvider().hasCustomToolbarColor()) {
            return mIntentDataProvider.getColorProvider().getToolbarColor();
        }
        return TabState.UNSPECIFIED_THEME_COLOR;
    }

    @Override
    public int getBaseStatusBarColor(@Nullable Tab tab) {
        // TODO(crbug.com/465719853): Pass the CCT Top Bar Color in AGSA intent after Google
        // Bottom Bar is launched.
        assert mIntentDataProvider != null;
        if (GoogleBottomBarCoordinator.isFeatureEnabled()
                && CustomTabsConnection.getInstance()
                        .shouldEnableGoogleBottomBarForIntent(mIntentDataProvider)) {
            return getWindow().getContext().getColor(R.color.google_bottom_bar_background_color);
        }
        return getCustomTabStatusBarColorProvider().getBaseStatusBarColor(tab);
    }

    @Override
    public void initDeferredStartupForActivity() {
        if (mWebappActivityCoordinator != null) {
            mWebappActivityCoordinator.initDeferredStartupForActivity();
        }
        DeferredStartupHandler.getInstance().addDeferredTask(this::onDeferredStartup);
        super.initDeferredStartupForActivity();
    }

    protected void onDeferredStartup() {
        if (isActivityFinishingOrDestroyed()) return;

        assumeNonNull(mBaseCustomTabRootUiCoordinator);
        mBaseCustomTabRootUiCoordinator.onDeferredStartup();
    }

    /**
     * Discards touch events that arrive while this activity is not RESUMED, i.e. events which may
     * be trickling down from an overlay activity above. See crbug.com/40063907.
     *
     * <p>This lives on {@link BaseCustomTabActivity} rather than on an individual subclass so that
     * every activity in this family hosting web content is covered - notably the PWA/WebAPK windows
     * ({@code WebappActivity}, {@code SameTaskWebApkActivity}), which render without a URL bar and
     * typically host a logged-in session.
     */
    @Override
    public boolean dispatchTouchEvent(MotionEvent ev) {
        if (sPreventTouches && shouldPreventTouch()) {
            return true;
        }
        return super.dispatchTouchEvent(ev);
    }

    @VisibleForTesting
    public boolean shouldPreventTouch() {
        if (ApplicationStatus.getStateForActivity(this) == ActivityState.RESUMED) return false;
        return true;
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        assumeNonNull(mToolbarCoordinator);
        Boolean result =
                KeyboardShortcuts.dispatchKeyEvent(
                        event,
                        mToolbarCoordinator.toolbarIsInitialized(),
                        getFullscreenManager(),
                        /* menuOrKeyboardActionController= */ this,
                        this);
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordCustomTimesHistogram(
                "MobileStartup.IntentToCreationTime2.CustomTabs",
                timeMs,
                1,
                DateUtils.MINUTE_IN_MILLIS,
                50);
        @ActivityType int activityType = getActivityType();
        if (activityType == ActivityType.WEBAPP || activityType == ActivityType.WEB_APK) {
            RecordHistogram.recordCustomTimesHistogram(
                    "MobileStartup.IntentToCreationTime2.Webapp",
                    timeMs,
                    1,
                    DateUtils.MINUTE_IN_MILLIS,
                    50);
        }
        if (activityType == ActivityType.WEB_APK) {
            RecordHistogram.recordCustomTimesHistogram(
                    "MobileStartup.IntentToCreationTime2.WebApk",
                    timeMs,
                    1,
                    DateUtils.MINUTE_IN_MILLIS,
                    50);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        assumeNonNull(mToolbarCoordinator);
        if (!mToolbarCoordinator.toolbarIsInitialized()) {
            return super.onKeyDown(keyCode, event);
        }
        boolean keyboardShortcutHandled =
                KeyboardShortcuts.onKeyDown(
                        event,
                        true,
                        false,
                        getTabModelSelector(),
                        /* menuOrKeyboardActionController= */ this,
                        getToolbarManager());
        if (keyboardShortcutHandled) {
            RecordUserAction.record("MobileKeyboardShortcutUsed");
        }
        return keyboardShortcutHandled || super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onMenuOrKeyboardAction(
            int id,
            boolean fromMenu,
            @Nullable Bundle menuItemData,
            @Nullable MotionEventInfo triggeringMotion) {
        // Disable creating new tabs, bookmark, print, help, focus_url, etc.
        if (id == R.id.focus_url_bar
                || id == R.id.all_bookmarks_menu_id
                || id == R.id.help_id
                || id == R.id.recent_tabs_menu_id
                || id == R.id.new_incognito_tab_menu_id
                || id == R.id.new_tab_menu_id) {
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu, menuItemData, triggeringMotion);
    }

    public WebContentsDelegateAndroid getWebContentsDelegate() {
        return assertNonNull(getCustomTabDelegateFactory().getWebContentsDelegate());
    }

    /**
     * @return Whether the app is running in the "Trusted Web Activity" mode, where the TWA-specific
     *     UI is shown.
     */
    public boolean isInTwaMode() {
        return mTwaCoordinator == null ? false : mTwaCoordinator.shouldUseAppModeUi();
    }

    /**
     * @return The package name of the Trusted Web Activity, if the activity is a TWA; null
     *     otherwise.
     */
    public @Nullable String getTwaPackage() {
        return mTwaCoordinator == null ? null : mTwaCoordinator.getTwaPackage();
    }

    @Override
    public void maybePreconnect() {
        // The ids need to be set early on, this way prewarming will pick them up.
        assumeNonNull(mIntentDataProvider);
        int[] experimentIds = mIntentDataProvider.getGsaExperimentIds();
        if (experimentIds != null) {
            // When ids are set through the intent, we don't want them to override the existing ids.
            boolean override = false;
            UmaSessionStats.registerExternalExperiment(experimentIds, override);
        }
        super.maybePreconnect();
    }

    @Override
    public boolean supportsAppMenu() {
        assumeNonNull(mIntentDataProvider);
        if (mIntentDataProvider.shouldSuppressAppMenu()) return false;

        return super.supportsAppMenu();
    }

    @Override
    protected boolean shouldShowTabOnActivityShown() {
        // Hidden tabs from speculation will be shown and added to the tab model in
        // CustomTabActivityTabController#finalizeCreatingTab.
        assumeNonNull(mTabProvider);
        return didFinishNativeInitialization()
                || mTabProvider.getInitialTabCreationMode() != TabCreationMode.HIDDEN;
    }

    @Override
    protected boolean wasInPictureInPictureForMinimizedCustomTabs() {
        if (!MinimizedFeatureUtils.isMinimizedCustomTabAvailable(this)) {
            return false;
        }
        return mLastPipMode == PictureInPictureMode.MINIMIZED_CUSTOM_TAB;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mCipherFactory.saveToBundle(outState);
        if (mIntentDataProvider != null) {
            CustomTabIntentDataHolder dataHolder =
                    mIntentDataProvider.getCustomTabIntentDataHolder();
            if (dataHolder != null) {
                outState.putParcelable(KEY_CUSTOM_TAB_INTENT_DATA_HOLDER, dataHolder);
            }
        }
    }

    public TabObserverRegistrar getTabObserverRegistrar() {
        return assertNonNull(mTabObserverRegistrar);
    }

    public CustomTabActivityTabProvider getCustomTabActivityTabProvider() {
        return assertNonNull(mTabProvider);
    }

    private CustomTabObserver getCustomTabObserver() {
        return assertNonNull(mCustomTabObserver);
    }

    private CustomTabNavigationEventObserver getCustomTabNavigationEventObserver() {
        return assertNonNull(mCustomTabNavigationEventObserver);
    }

    private Verifier createVerifier() {
        assert mIntentDataProvider != null;
        return switch (getActivityType()) {
            case ActivityType.WEB_APK -> new WebApkVerifier(mIntentDataProvider);
            case ActivityType.WEBAPP -> new AddToHomescreenVerifier(mIntentDataProvider);
            case ActivityType.TRUSTED_WEB_ACTIVITY ->
                    new TwaVerifier(
                            getLifecycleDispatcher(),
                            mIntentDataProvider,
                            getClientPackageNameProvider(),
                            getCustomTabActivityTabProvider());
            default -> new EmptyVerifier();
        };
    }

    private Verifier getVerifier() {
        if (mVerifier == null) mVerifier = createVerifier();
        return mVerifier;
    }

    private ClientPackageNameProvider getClientPackageNameProvider() {
        return assertNonNull(mClientPackageNameProvider);
    }

    private CipherFactory getCipherFactory() {
        return mCipherFactory;
    }

    private TwaFinishHandler getTwaFinishHandler() {
        if (mTwaFinishHandler == null) {
            assert mIntentDataProvider != null;
            mTwaFinishHandler = new TwaFinishHandler(this, mIntentDataProvider);
        }
        return mTwaFinishHandler;
    }

    private CloseButtonVisibilityManager getCloseButtonVisibilityManager() {
        if (mCloseButtonVisibilityManager == null) {
            assert mIntentDataProvider != null;
            mCloseButtonVisibilityManager = new CloseButtonVisibilityManager(mIntentDataProvider);
        }
        return mCloseButtonVisibilityManager;
    }

    private CustomTabBrowserControlsVisibilityDelegate
            getCustomTabBrowserControlsVisibilityDelegate() {
        if (mCustomTabBrowserControlsVisibilityDelegate == null) {
            mCustomTabBrowserControlsVisibilityDelegate =
                    new CustomTabBrowserControlsVisibilityDelegate(this::getBrowserControlsManager);

            // TODO(crbug.com/470432106): Move this to root ui coordinator.
            getBaseCustomTabRootUiCoordinator()
                    .getAppBrowserControlsVisibilityDelegate()
                    .addDelegate(getCustomTabBrowserControlsVisibilityDelegate());
        }
        return mCustomTabBrowserControlsVisibilityDelegate;
    }

    public Supplier<@Nullable BottomSheetController> getBottomSheetController() {
        return mRootUiCoordinator.getBottomSheetControllerSupplier();
    }

    @Override
    public ActivityLifecycleDispatcher getLifecycleDispatcher() {
        if (mLifecycleDispatcherForTesting != null) return mLifecycleDispatcherForTesting;
        return super.getLifecycleDispatcher();
    }

    public void setLifecycleDispatcherForTesting(ActivityLifecycleDispatcher dispatcher) {
        mLifecycleDispatcherForTesting = dispatcher;
    }

    public CurrentPageVerifier getCurrentPageVerifier() {
        return assertNonNull(mCurrentPageVerifier);
    }

    private @Nullable AuthTabVerifier getAuthTabVerifier() {
        return mAuthTabVerifier;
    }

    private CustomTabOrientationController getCustomTabOrientationController() {
        if (mCustomTabOrientationController == null) {
            var windowAndroid = assertNonNull(getWindowAndroid());
            assert mIntentDataProvider != null;
            mCustomTabOrientationController =
                    new CustomTabOrientationController(windowAndroid, mIntentDataProvider);
        }
        return mCustomTabOrientationController;
    }

    private ImmersiveModeController getImmersiveModeController() {
        if (mImmersiveModeController == null) {
            var windowAndroid = assertNonNull(getWindowAndroid());
            mImmersiveModeController =
                    new ImmersiveModeController(
                            this,
                            windowAndroid,
                            assumeNonNull(getEdgeToEdgeManager()).getEdgeToEdgeStateProvider(),
                            getLifecycleDispatcher());
        }
        return mImmersiveModeController;
    }

    private CustomTabToolbarColorController getCustomTabToolbarColorController() {
        return assertNonNull(mCustomTabToolbarColorController);
    }

    private CustomTabStatusBarColorProvider getCustomTabStatusBarColorProvider() {
        if (mStatusBarColorProvider == null) {
            assert mIntentDataProvider != null;
            mStatusBarColorProvider =
                    new CustomTabStatusBarColorProvider(
                            mIntentDataProvider, mRootUiCoordinator.getStatusBarColorController());
        }
        return mStatusBarColorProvider;
    }

    private SplashController getSplashController() {
        if (mSplashController == null) {
            mSplashController =
                    new SplashController(
                            this,
                            getLifecycleDispatcher(),
                            getTabObserverRegistrar(),
                            getTwaFinishHandler(),
                            getCustomTabActivityTabProvider(),
                            getCompositorViewHolderSupplier(),
                            getCustomTabOrientationController());
        }
        return mSplashController;
    }

    public Supplier<SplashController> getSplashControllerSupplier() {
        return this::getSplashController;
    }

    private CustomTabCompositorContentInitializer getCustomTabCompositorContentInitializer() {
        return assertNonNull(mCustomTabCompositorContentInitializer);
    }

    protected CustomTabBottomBarDelegate getCustomTabBottomBarDelegate() {
        return assertNonNull(mCustomTabBottomBarDelegate);
    }

    private CustomTabDelegateFactory getCustomTabDelegateFactory() {
        if (mDelegateFactory == null) {
            assert mIntentDataProvider != null;
            mDelegateFactory =
                    new CustomTabDelegateFactory(
                            this,
                            mIntentDataProvider,
                            getCustomTabBrowserControlsVisibilityDelegate(),
                            getVerifier(),
                            this,
                            getBrowserControlsManager(),
                            getFullscreenManager(),
                            this,
                            getTabModelSelectorSupplier(),
                            getCompositorViewHolderSupplier(),
                            getModalDialogManagerSupplier(),
                            this::getSnackbarManager,
                            getShareDelegateSupplier(),
                            getActivityType(),
                            getBottomSheetController(),
                            getAuthTabVerifier(),
                            getBrowserControlsManager(),
                            this::isShowingWebAppHeaderButtons,
                            this::isShowingHeaderAsOverlay,
                            mRootUiCoordinator.getExclusiveAccessManager(),
                            mRootUiCoordinator.getDesktopWindowStateManager());
        }
        return mDelegateFactory;
    }

    public CustomTabTabPersistencePolicy getCustomTabTabPersistencePolicy() {
        if (mCustomTabTabPersistencePolicy == null) {
            mCustomTabTabPersistencePolicy =
                    new CustomTabTabPersistencePolicy(this, getSavedInstanceState());
        }
        return mCustomTabTabPersistencePolicy;
    }

    private WebApkUpdateManager createWebApkUpdateManager() {
        return new WebApkUpdateManager(this, getActivityTabProvider(), getLifecycleDispatcher());
    }

    private CustomTabActivityTabFactory getCustomTabActivityTabFactory() {
        if (mTabFactory == null) {
            var windowAndroid = assertNonNull(getWindowAndroid());
            assert mIntentDataProvider != null;
            mTabFactory =
                    new CustomTabActivityTabFactory(
                            this,
                            getCustomTabTabPersistencePolicy(),
                            windowAndroid,
                            getProfileProviderSupplier(),
                            getCustomTabDelegateFactory(),
                            mIntentDataProvider,
                            this,
                            getTabModelSelectorSupplier(),
                            getCompositorViewHolderSupplier(),
                            getCipherFactory());
        }
        return mTabFactory;
    }

    private CustomTabActivityTabController getCustomTabActivityTabController() {
        return assertNonNull(mTabController);
    }

    private WebappDeferredStartupWithStorageHandler getWebappDeferredStartupWithStorageHandler() {
        if (mWebappDeferredStartupWithStorageHandler == null) {
            assert mIntentDataProvider != null;
            mWebappDeferredStartupWithStorageHandler =
                    new WebappDeferredStartupWithStorageHandler(this, mIntentDataProvider);
        }
        return mWebappDeferredStartupWithStorageHandler;
    }

    private TrustedWebActivityModel getTrustedWebActivityModel() {
        if (mTrustedWebActivityModel == null) {
            mTrustedWebActivityModel = new TrustedWebActivityModel();
        }
        return mTrustedWebActivityModel;
    }

    public CustomTabActivityNavigationController getCustomTabActivityNavigationController() {
        return assertNonNull(mNavigationController);
    }

    private CustomTabMinimizationManagerHolder getCustomTabMinimizationManagerHolder() {
        return assertNonNull(mMinimizationManagerHolder);
    }

    private DisclosurePersistentSnackbar createDisclosurePersistentSnackbar() {
        return new DisclosurePersistentSnackbar(
                getResources(),
                this::getSnackbarManager,
                getTrustedWebActivityModel(),
                getLifecycleDispatcher());
    }

    private DisclosureSnackbar createDisclosureSnackbar() {
        return new DisclosureSnackbar(
                getResources(),
                this::getSnackbarManager,
                getTrustedWebActivityModel(),
                getLifecycleDispatcher());
    }

    private DisclosureNotification createDisclosureNotification() {
        return new DisclosureNotification(
                getResources(), getTrustedWebActivityModel(), getLifecycleDispatcher());
    }

    public CustomTabToolbarCoordinator getCustomTabToolbarCoordinator() {
        return assertNonNull(mToolbarCoordinator);
    }

    private TrustedWebActivityBrowserControlsVisibilityManager
            createTrustedWebActivityBrowserControlsVisibilityManager() {
        if (mBrowserControlsVisibilityManager != null) {
            return mBrowserControlsVisibilityManager;
        }

        assert mIntentDataProvider != null;
        mBrowserControlsVisibilityManager =
                new TrustedWebActivityBrowserControlsVisibilityManager(
                        getTabObserverRegistrar(),
                        getCustomTabActivityTabProvider(),
                        getCustomTabToolbarCoordinator(),
                        getCloseButtonVisibilityManager(),
                        getAppHeaderCoordinator(),
                        mIntentDataProvider,
                        getFullscreenManager());
        return mBrowserControlsVisibilityManager;
    }

    private SharedActivityCoordinator getSharedActivityCoordinator() {
        if (mSharedActivityCoordinator == null) {
            TrustedWebActivityBrowserControlsVisibilityManager controlsVisibilityManager =
                    createTrustedWebActivityBrowserControlsVisibilityManager();
            assert mIntentDataProvider != null;
            mSharedActivityCoordinator =
                    new SharedActivityCoordinator(
                            getCurrentPageVerifier(),
                            controlsVisibilityManager,
                            getCustomTabStatusBarColorProvider(),
                            this::getImmersiveModeController,
                            mIntentDataProvider,
                            getCustomTabOrientationController(),
                            getCustomTabActivityNavigationController(),
                            getVerifier(),
                            getBrowserServicesThemeColorProvider(),
                            getLifecycleDispatcher());
        }
        return mSharedActivityCoordinator;
    }

    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    private @Nullable AppHeaderCoordinator getAppHeaderCoordinator() {
        assert mIntentDataProvider != null;
        if (!WebAppHeaderUtils.isWebAppHeaderEnabled(mIntentDataProvider)
                && !DesktopPopupHeaderUtils.isDesktopPopupHeaderEnabled(mIntentDataProvider)) {
            return null;
        }
        if (mAppHeaderCoordinator != null) return mAppHeaderCoordinator;

        mAppHeaderCoordinator =
                new AppHeaderCoordinator(
                        this,
                        getWindow().getDecorView().getRootView(),
                        getBrowserControlsManager().getBrowserVisibilityDelegate(),
                        getInsetObserver(),
                        getLifecycleDispatcher(),
                        getSavedInstanceState(),
                        null,
                        assumeNonNull(getEdgeToEdgeManager()).getEdgeToEdgeStateProvider(),
                        null);

        return mAppHeaderCoordinator;
    }

    private BrowserServicesThemeColorProvider getBrowserServicesThemeColorProvider() {
        if (mBrowserServicesThemeColorProvider != null) return mBrowserServicesThemeColorProvider;

        assert mIntentDataProvider != null;
        mBrowserServicesThemeColorProvider =
                new BrowserServicesThemeColorProvider(
                        this,
                        mIntentDataProvider,
                        getToolbarThemeColorProvider(),
                        getCustomTabActivityTabProvider(),
                        getTabObserverRegistrar(),
                        getLifecycleDispatcher(),
                        getAppHeaderCoordinator());
        return mBrowserServicesThemeColorProvider;
    }

    protected @Nullable WebappActivityCoordinator getWebappActivityCoordinator() {
        return mWebappActivityCoordinator;
    }

    protected BaseCustomTabRootUiCoordinator getBaseCustomTabRootUiCoordinator() {
        return assertNonNull(mBaseCustomTabRootUiCoordinator);
    }

    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    private boolean isShowingWebAppHeaderButtons() {
        assert mIntentDataProvider != null;
        if (!WebAppHeaderUtils.isMinimalUiEnabled(mIntentDataProvider)) return false;

        assumeNonNull(mBaseCustomTabRootUiCoordinator);
        WebAppHeaderLayoutCoordinator webAppHeaderLayoutCoordinator =
                mBaseCustomTabRootUiCoordinator.getWebAppHeaderLayoutCoordinator();
        if (webAppHeaderLayoutCoordinator == null) {
            return false;
        }
        return webAppHeaderLayoutCoordinator.isShowingButtons();
    }

    private boolean isShowingHeaderAsOverlay() {
        assert mIntentDataProvider != null;
        if (!WebAppHeaderUtils.isWindowControlsOverlayEnabled(mIntentDataProvider)) {
            return false;
        }

        assumeNonNull(mBaseCustomTabRootUiCoordinator);
        if (mBaseCustomTabRootUiCoordinator.getWebAppHeaderLayoutCoordinator() == null) {
            return false;
        }

        return mBaseCustomTabRootUiCoordinator.isShowingHeaderAsOverlay();
    }

    /**
     * Returns the native browser window type supported by this {@code Activity}.
     *
     * <p>The native browser window types are defined in the {@code BrowserWindowInterface::Type}
     * enum.
     */
    @BrowserWindowType
    @Nullable Integer getSupportedBrowserWindowType() {
        final boolean browserWindowInterfaceEnabled =
                ChromeFeatureList.sEnableBrowserWindowInterfaceForCustomTabActivity.isEnabled();
        assumeNonNull(mIntentDataProvider);
        // Progressive web apps.
        if (browserWindowInterfaceEnabled
                && mIntentDataProvider.getActivityType() == ActivityType.WEBAPP) {
            return BrowserWindowType.APP;
        }
        @CustomTabsUiType int type = mIntentDataProvider.getUiType();
        switch (type) {
            case CustomTabsUiType.DEFAULT:
                if (browserWindowInterfaceEnabled) {
                    return BrowserWindowType.CUSTOM_TAB;
                }
                break;
            // Popups.
            case CustomTabsUiType.POPUP:
                return BrowserWindowType.POPUP;
            // Progressive web apps.
            case CustomTabsUiType.MINIMAL_UI_WEBAPP:
            /* Fallthrough */
            case CustomTabsUiType.TRUSTED_WEB_ACTIVITY:
                return browserWindowInterfaceEnabled ? BrowserWindowType.APP : null;
            default:
                break;
        }

        return null;
    }

    private @Nullable CustomTabResumeManager maybeCreateResumeManager() {
        if (!ChromeFeatureList.sCctTabResumption.isEnabled()) return null;

        assert mIntentDataProvider != null;
        if (!CustomTabResumeManager.shouldCreateTabResumeManager(mIntentDataProvider)) {
            return null;
        }

        return new CustomTabResumeManager(mIntentDataProvider, getCipherFactory());
    }
}
