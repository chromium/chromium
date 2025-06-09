// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.HANDLED_BY_OS;
import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.USER_NAVIGATION;

import android.app.Activity;
import android.content.Intent;
import android.os.Build;
import android.os.Bundle;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.View;
import android.view.ViewGroup;
import android.view.Window;
import android.widget.LinearLayout;
import android.widget.LinearLayout.LayoutParams;

import androidx.annotation.AnimRes;
import androidx.annotation.ChecksSdkIntAtLeast;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;

import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.browserservices.InstalledWebappDataRegister;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType;
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
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureInfobar;
import org.chromium.chrome.browser.browserservices.ui.view.DisclosureNotification;
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
import org.chromium.chrome.browser.customtabs.features.ImmersiveModeController;
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
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager.Observer;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.PowerSavingModeMonitor;
import org.chromium.chrome.browser.profiles.OtrProfileId;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.theme.ThemeColorProvider;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.desktop_windowing.AppHeaderCoordinator;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
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

/**
 * Contains functionality which is shared between {@link WebappActivity} and {@link
 * CustomTabActivity}. Purpose of the class is to simplify merging {@link WebappActivity} and {@link
 * CustomTabActivity}.
 */
public abstract class BaseCustomTabActivity extends ChromeActivity {
    private static Integer sOverrideCoreCountForTesting;

    private final CipherFactory mCipherFactory = new CipherFactory();

    private BaseCustomTabRootUiCoordinator mBaseCustomTabRootUiCoordinator;
    private BrowserServicesIntentDataProvider mIntentDataProvider;
    private CustomTabDelegateFactory mDelegateFactory;
    private CustomTabToolbarCoordinator mToolbarCoordinator;
    private CustomTabActivityNavigationController mNavigationController;
    private CustomTabActivityTabController mTabController;
    private CustomTabActivityTabProvider mTabProvider;
    private CustomTabStatusBarColorProvider mStatusBarColorProvider;
    private CustomTabActivityTabFactory mTabFactory;
    private CustomTabIntentHandler mCustomTabIntentHandler;
    private CustomTabNightModeStateController mNightModeStateController;
    private @Nullable WebappActivityCoordinator mWebappActivityCoordinator;
    private @Nullable TrustedWebActivityCoordinator mTwaCoordinator;
    private @Nullable AuthTabVerifier mAuthTabVerifier;
    private Verifier mVerifier;
    private FullscreenManager mFullscreenManager;
    private CustomTabMinimizationManagerHolder mMinimizationManagerHolder;
    private CustomTabFeatureOverridesManager mCustomTabFeatureOverridesManager;
    private boolean mWarmupOnDestroy;
    private TabObserverRegistrar mTabObserverRegistrar;
    private CustomTabObserver mCustomTabObserver;
    private CustomTabNavigationEventObserver mCustomTabNavigationEventObserver;
    private ClientPackageNameProvider mClientPackageNameProvider;
    private TwaFinishHandler mTwaFinishHandler;
    private CloseButtonVisibilityManager mCloseButtonVisibilityManager;
    private CustomTabBrowserControlsVisibilityDelegate mCustomTabBrowserControlsVisibilityDelegate;
    private CurrentPageVerifier mCurrentPageVerifier;
    private CustomTabActivityClientConnectionKeeper mCustomTabActivityClientConnectionKeeper;
    private CustomTabOrientationController mCustomTabOrientationController;
    private CustomTabToolbarColorController mCustomTabToolbarColorController;
    private SplashController mSplashController;
    private CustomTabCompositorContentInitializer mCustomTabCompositorContentInitializer;
    private CustomTabBottomBarDelegate mCustomTabBottomBarDelegate;
    private CustomTabTabPersistencePolicy mCustomTabTabPersistencePolicy;
    private WebappDeferredStartupWithStorageHandler mWebappDeferredStartupWithStorageHandler;
    private TrustedWebActivityModel mTrustedWebActivityModel;
    private SharedActivityCoordinator mSharedActivityCoordinator;
    private TrustedWebActivityBrowserControlsVisibilityManager mBrowserControlsVisibilityManager;
    private @Nullable AppHeaderCoordinator mAppHeaderCoordinator;
    private @Nullable BrowserServicesThemeColorProvider mBrowserServicesThemeColorProvider;

    private ActivityLifecycleDispatcher mLifecycleDispatcherForTesting;

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

    public static void setOverrideCoreCountForTesting(int coreCount) {
        sOverrideCoreCountForTesting = coreCount;
        ResettersForTesting.register(() -> sOverrideCoreCountForTesting = null);
    }

    /** Builds {@link BrowserServicesIntentDataProvider} for this {@link CustomTabActivity}. */
    protected BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme) {
        if (AuthTabIntentDataProvider.isAuthTabIntent(intent)) {
            return new AuthTabIntentDataProvider(intent, this, colorScheme);
        } else if (IncognitoCustomTabIntentDataProvider.isValidIncognitoIntent(
                intent, /* recordMetrics= */ true)) {
            return new IncognitoCustomTabIntentDataProvider(intent, this, colorScheme);
        } else if (EphemeralCustomTabIntentDataProvider.isValidEphemeralTabIntent(intent)) {
            return new EphemeralCustomTabIntentDataProvider(intent, this, colorScheme);
        }
        return new CustomTabIntentDataProvider(intent, this, colorScheme);
    }

    /**
     * @return The {@link BrowserServicesIntentDataProvider} for this {@link CustomTabActivity}.
     */
    public BrowserServicesIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }

    /**
     * @return Whether the activity window is initially translucent.
     */
    public static boolean isWindowInitiallyTranslucent(Activity activity) {
        return activity instanceof TranslucentCustomTabActivity
                || activity instanceof SameTaskWebApkActivity;
    }

    @Override
    protected NightModeStateProvider createNightModeStateProvider() {
        return getCustomTabNightModeStateController();
    }

    public CustomTabNightModeStateController getCustomTabNightModeStateController() {
        if (mNightModeStateController == null) {
            mNightModeStateController =
                    new CustomTabNightModeStateController(
                            getLifecycleDispatcher(), PowerSavingModeMonitor.getInstance());
        }
        return mNightModeStateController;
    }

    @Override
    protected void initializeNightModeStateProvider() {
        mNightModeStateController.initialize(getDelegate(), getIntent());
    }

    @Override
    protected boolean wrapContentWithEdgeToEdgeLayout() {
        // TODO(crbug.com/392774038): Enable for e2e everywhere for PCCT.
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
                buildIntentDataProvider(intent, COLOR_SCHEME_LIGHT);

        mCustomTabIntentHandler.onNewIntent(dataProvider);
    }

    @Override
    public void setContentView(int layoutResID) {
        if (WebAppHeaderUtils.isMinimalUiEnabled(mIntentDataProvider)) {
            final LinearLayout linearLayout =
                    (LinearLayout)
                            getLayoutInflater()
                                    .inflate(WebAppHeaderUtils.getWebAppHeaderLayoutId(), null);
            getLayoutInflater().inflate(layoutResID, linearLayout, true);
            super.setContentView(linearLayout);
        } else {
            super.setContentView(layoutResID);
        }
    }

    @Override
    public void setContentView(View view) {
        if (WebAppHeaderUtils.isMinimalUiEnabled(mIntentDataProvider)) {
            final LinearLayout linearLayout =
                    (LinearLayout)
                            getLayoutInflater()
                                    .inflate(WebAppHeaderUtils.getWebAppHeaderLayoutId(), null);
            linearLayout.addView(view, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            super.setContentView(linearLayout);
        } else {
            super.setContentView(view);
        }
    }

    @Override
    public void setContentView(View view, ViewGroup.LayoutParams params) {
        if (WebAppHeaderUtils.isMinimalUiEnabled(mIntentDataProvider)) {
            final LinearLayout linearLayout =
                    (LinearLayout)
                            getLayoutInflater()
                                    .inflate(WebAppHeaderUtils.getWebAppHeaderLayoutId(), null);
            linearLayout.setLayoutParams(params);
            linearLayout.addView(view, LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            super.setContentView(linearLayout);
        } else {
            super.setContentView(view, params);
        }
    }

    @Override
    protected RootUiCoordinator createRootUiCoordinator() {
        mBaseCustomTabRootUiCoordinator =
                new BaseCustomTabRootUiCoordinator(
                        this,
                        getShareDelegateSupplier(),
                        getActivityTabProvider(),
                        mTabModelProfileSupplier,
                        mBookmarkModelSupplier,
                        mTabBookmarkerSupplier,
                        getTabModelSelectorSupplier(),
                        getBrowserControlsManager(),
                        getWindowAndroid(),
                        getLifecycleDispatcher(),
                        getLayoutManagerSupplier(),
                        /* menuOrKeyboardActionController= */ this,
                        this::getActivityThemeColor,
                        getModalDialogManagerSupplier(),
                        /* appMenuBlocker= */ this,
                        this::supportsAppMenu,
                        this::supportsFindInPage,
                        getTabCreatorManagerSupplier(),
                        getFullscreenManager(),
                        getCompositorViewHolderSupplier(),
                        getTabContentManagerSupplier(),
                        this::getSnackbarManager,
                        mEdgeToEdgeControllerSupplier,
                        getActivityType(),
                        this::isInOverviewMode,
                        /* appMenuDelegate= */ this,
                        /* statusBarColorProvider= */ this,
                        getIntentRequestTracker(),
                        () -> mToolbarCoordinator,
                        () -> mIntentDataProvider,
                        mBackPressManager,
                        () -> getCustomTabActivityTabController(),
                        () -> getCustomTabMinimizationManagerHolder().getMinimizationManager(),
                        () -> getCustomTabFeatureOverridesManager(),
                        () -> getCustomTabActivityNavigationController().openCurrentUrlInBrowser(),
                        getEdgeToEdgeManager(),
                        getAppHeaderCoordinator(),
                        this::getBrowserServicesThemeColorProvider);
        return mBaseCustomTabRootUiCoordinator;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        return new ActivityProfileProvider(getLifecycleDispatcher()) {
            @Nullable
            @Override
            protected OtrProfileId createOffTheRecordProfileId() {
                switch (getIntentDataProvider().getCustomTabMode()) {
                    case CustomTabProfileType.INCOGNITO:
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
        mWebappActivityCoordinator =
                new WebappActivityCoordinator(
                        getIntentDataProvider(),
                        this,
                        getWebappDeferredStartupWithStorageHandler(),
                        getLifecycleDispatcher());
        // Classes manage their own lifecycles and just need to be initialized.
        new WebappActionsNotificationManager(
                getCustomTabActivityTabProvider(),
                getIntentDataProvider(),
                getLifecycleDispatcher());
        new WebappSplashController(
                this, getSplashController(), getTabObserverRegistrar(), getIntentDataProvider());
        getSharedActivityCoordinator();

        if (getIntentDataProvider().isWebApkActivity()) initializeForWebApk();
    }

    private void initializeForWebApk() {
        // Classes manage their own lifecycles and just need to be initialized.
        new WebApkActivityCoordinator(
                getIntentDataProvider(),
                this::createWebApkUpdateManager,
                getWebappDeferredStartupWithStorageHandler(),
                getLifecycleDispatcher());
        createDisclosureInfobar();
        new WebApkActivityLifecycleUmaTracker(
                this,
                getIntentDataProvider(),
                getSplashControllerSupplier(),
                getLegacyTabStartupMetricsTracker(),
                getStartupMetricsTracker(),
                this::getSavedInstanceState,
                getWebappDeferredStartupWithStorageHandler(),
                getLifecycleDispatcher());
        new WebappDisclosureController(
                getTrustedWebActivityModel(),
                getLifecycleDispatcher(),
                getCurrentPageVerifier(),
                getIntentDataProvider(),
                getWebappDeferredStartupWithStorageHandler());
    }

    private void initializeForTwa() {
        // Classes manage their own lifecycles and just need to be initialized.
        mTwaCoordinator =
                new TrustedWebActivityCoordinator(
                        this,
                        getSharedActivityCoordinator(),
                        getCurrentPageVerifier(),
                        getClientPackageNameProvider(),
                        getSplashControllerSupplier(),
                        getIntentDataProvider());
        new DisclosureUiPicker(
                this::createDisclosureInfobar,
                this::createDisclosureSnackbar,
                this::createDisclosureNotification,
                getIntentDataProvider(),
                getLifecycleDispatcher());
        new TrustedWebActivityDisclosureController(
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

    @Override
    public void performPreInflationStartup() {
        // This must be requested before adding content.
        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);

        // Parse the data from the Intent before calling super to allow the Intent to customize
        // the Activity parameters, including the background of the page.
        // Note that color scheme is fixed for the lifetime of Activity: if the system setting
        // changes, we recreate the activity.
        mIntentDataProvider = buildIntentDataProvider(getIntent(), getColorScheme());

        if (mIntentDataProvider == null) {
            // |mIntentDataProvider| is null if the WebAPK server vended an invalid WebAPK (WebAPK
            // correctly signed, mandatory <meta-data> missing).
            this.finishAndRemoveTask();
            return;
        }

        InstalledWebappDataRegister.prefetchPreferences();

        mClientPackageNameProvider =
                new ClientPackageNameProvider(
                        getLifecycleDispatcher(), mIntentDataProvider, getSavedInstanceState());

        // Hidden tabs shouldn't be used in incognito/ephemeral CCT, since they are always
        // created with regular profile.
        HiddenTab hiddenTab =
                mIntentDataProvider.isOffTheRecord()
                        ? null
                        : CustomTabActivityTabController.getHiddenTab(mIntentDataProvider);

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
                            mIntentDataProvider.getSession(),
                            mIntentDataProvider.getTwaStartupUptimeMillis());
            mCustomTabNavigationEventObserver =
                    new CustomTabNavigationEventObserver(
                            mIntentDataProvider.getSession(), /* forPrerender= */ false);
        }
        mTabObserverRegistrar.associateWithActivity(getLifecycleDispatcher(), mTabProvider);

        mCurrentPageVerifier =
                new CurrentPageVerifier(
                        getCustomTabActivityTabProvider(),
                        getIntentDataProvider(),
                        getVerifier(),
                        getTabObserverRegistrar(),
                        getLifecycleDispatcher());

        if (mIntentDataProvider.isAuthTab()) {
            mAuthTabVerifier =
                    new AuthTabVerifier(
                            this,
                            getLifecycleDispatcher(),
                            getIntentDataProvider(),
                            getCustomTabActivityTabProvider());
        }

        mCustomTabActivityClientConnectionKeeper =
                new CustomTabActivityClientConnectionKeeper(
                        getIntentDataProvider(),
                        getCustomTabActivityTabProvider(),
                        getLifecycleDispatcher());

        new CustomTabActivityLifecycleUmaTracker(
                this,
                getIntentDataProvider(),
                this::getSavedInstanceState,
                getLifecycleDispatcher());

        super.performPreInflationStartup();

        mCustomTabToolbarColorController =
                new CustomTabToolbarColorController(
                        this,
                        getBrowserServicesThemeColorProvider(),
                        getAppHeaderCoordinator(),
                        getIntentDataProvider());

        mCustomTabCompositorContentInitializer =
                new CustomTabCompositorContentInitializer(
                        this,
                        getCompositorViewHolderSupplier(),
                        getTabContentManagerSupplier(),
                        /* compositorViewHolderInitializer= */ this,
                        getTopUiThemeColorProvider(),
                        getLifecycleDispatcher());

        mCustomTabBottomBarDelegate =
                new CustomTabBottomBarDelegate(
                        this,
                        getWindowAndroid(),
                        getIntentDataProvider(),
                        getBrowserControlsManager(),
                        getCustomTabNightModeStateController(),
                        getCustomTabActivityTabProvider(),
                        getCustomTabCompositorContentInitializer());

        mTabController =
                new CustomTabActivityTabController(
                        this,
                        getProfileProviderSupplier(),
                        getCustomTabDelegateFactory(),
                        getIntentDataProvider(),
                        getTabObserverRegistrar(),
                        getCompositorViewHolderSupplier(),
                        getCustomTabTabPersistencePolicy(),
                        getCustomTabActivityTabFactory(),
                        getCustomTabObserver(),
                        getCustomTabNavigationEventObserver(),
                        getActivityTabProvider(),
                        getCustomTabActivityTabProvider(),
                        this::getSavedInstanceState,
                        getWindowAndroid(),
                        this,
                        getCipherFactory(),
                        getLifecycleDispatcher());

        // Finish reparenting as soon as possible as it may be blocking navigation.
        getCustomTabActivityTabController()
                .setUpInitialTab(hiddenTab != null ? hiddenTab.tab : null);

        mMinimizationManagerHolder =
                new CustomTabMinimizationManagerHolder(
                        this,
                        this::getCustomTabActivityNavigationController,
                        getActivityTabProvider(),
                        getIntentDataProvider(),
                        this::getSavedInstanceState,
                        getLifecycleDispatcher(),
                        getCustomTabFeatureOverridesManager());

        CloseButtonNavigator closeButtonNavigator =
                new CloseButtonNavigator(
                        getCustomTabActivityTabController(), getCustomTabActivityTabProvider(),
                        getIntentDataProvider(), getCustomTabMinimizationManagerHolder());

        mNavigationController =
                new CustomTabActivityNavigationController(
                        getCustomTabActivityTabController(),
                        getCustomTabActivityTabProvider(),
                        getIntentDataProvider(),
                        getCustomTabObserver(),
                        closeButtonNavigator,
                        this,
                        getLifecycleDispatcher());

        mToolbarCoordinator =
                new CustomTabToolbarCoordinator(
                        getIntentDataProvider(),
                        getCustomTabActivityTabProvider(),
                        this,
                        getWindowAndroid(),
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
                        getIntentDataProvider(),
                        customTabIntentHandlingStrategy,
                        this,
                        getCustomTabMinimizationManagerHolder());

        getCustomTabActivityNavigationController()
                .setFinishHandler(
                        (reason, warmupOnFinish) -> {
                            if (reason == USER_NAVIGATION || reason == HANDLED_BY_OS) {
                                getCustomTabActivityClientConnectionKeeper()
                                        .recordClientConnectionStatus();
                            }
                            handleFinishAndClose(reason, warmupOnFinish);
                        });

        mBackPressManager.setFallbackOnBackPressed(this::handleBackPressed);
        mBackPressManager.addHandler(
                getCustomTabActivityNavigationController(),
                BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB);
        if (CustomTabActivityNavigationController.supportsPredictiveBackGesture()) {
            mBackPressManager.addOnSystemNavigationObserver(
                    getCustomTabActivityNavigationController());
        }

        new CustomTabSessionHandler(
                getIntentDataProvider(),
                getCustomTabActivityTabProvider(),
                this::getCustomTabToolbarCoordinator,
                this::getCustomTabBottomBarDelegate,
                mCustomTabIntentHandler,
                this,
                getLifecycleDispatcher());

        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();

        // We need the CustomTabIncognitoManager for all OffTheRecord profiles to ensure
        // that they are destroyed when a CCT session ends.
        if (intentDataProvider.isOffTheRecord()) {
            new CustomTabIncognitoManager(
                    this,
                    getCustomTabActivityNavigationController(),
                    getIntentDataProvider(),
                    getProfileProviderSupplier(),
                    getLifecycleDispatcher());
        }

        if (intentDataProvider.isWebappOrWebApkActivity()) initializeForWebappOrWebApk();
        if (mIntentDataProvider.isTrustedWebActivity()) initializeForTwa();

        getCustomTabActivityTabFactory().setActivityType(getActivityType());
        getCustomTabDelegateFactory()
                .setEphemeralTabCoordinatorSupplier(
                        mRootUiCoordinator.getEphemeralTabCoordinatorSupplier());

        new CustomTabDownloadObserver(this, getTabObserverRegistrar());

        if (mIntentDataProvider.isTrustedWebActivity()) {
            new TrustedWebActivityOpenTimeRecorder(
                    getCurrentPageVerifier(), getActivityTabProvider(), getLifecycleDispatcher());
        }

        new CustomTabTaskDescriptionHelper(
                this,
                getCustomTabActivityTabProvider(),
                getTabObserverRegistrar(),
                getIntentDataProvider(),
                getTopUiThemeColorProvider(),
                getLifecycleDispatcher());

        if (mIntentDataProvider.isPartialCustomTab()) {
            @AnimRes
            int startAnimResId =
                    PartialCustomTabDisplayManager.getStartAnimationOverride(
                            this,
                            getIntentDataProvider(),
                            getIntentDataProvider().getAnimationEnterRes());
            overridePendingTransition(startAnimResId, R.anim.no_anim);
        }

        WebappExtras webappExtras = getIntentDataProvider().getWebappExtras();
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

        Integer androidBrowserHelperVersion = mIntentDataProvider.getAndroidBrowserHelperVersion();
        if (androidBrowserHelperVersion != null) {
            RecordHistogram.recordSparseHistogram(
                    "CustomTabs.AndroidBrowserHelper.Version",
                    androidBrowserHelperVersion.intValue());
        }
    }

    @Override
    protected void onDestroyInternal() {
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
            Profile profile = getProfileProviderSupplier().get().getOriginalProfile();
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> CustomTabsConnection.createSpareWebContents(profile));
        } else {
            RecordHistogram.recordBooleanHistogram("CustomTabs.SpareRenderer", false);
        }

        if (getCustomTabActivityTabController() != null) {
            getCustomTabActivityTabController().destroy();
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
     * @return {@link ThemeColorProvider} for top UI.
     */
    public TopUiThemeColorProvider getTopUiThemeColorProvider() {
        return mRootUiCoordinator.getTopUiThemeColorProvider();
    }

    @Override
    public void initializeState() {
        super.initializeState();

        // TODO(pkotwicz): Determine whether finishing tab initialization in initializeState() has a
        // positive performance impact.
        if (getIntentDataProvider().isWebappOrWebApkActivity()) {
            getCustomTabActivityTabController().finishNativeInitialization();
        }
    }

    @Override
    public void finishNativeInitialization() {
        if (isTaskRoot()) {
            getProfileProviderSupplier()
                    .runSyncOrOnAvailable(
                            (profileProvider) -> {
                                UsageStatsService.createPageViewObserverIfEnabled(
                                        this,
                                        profileProvider.getOriginalProfile(),
                                        getActivityTabProvider(),
                                        getTabContentManagerSupplier());
                            });
        }
        if (!getIntentDataProvider().isWebappOrWebApkActivity()) {
            getCustomTabActivityTabController().finishNativeInitialization();
        }

        super.finishNativeInitialization();
    }

    @Override
    protected TabModelOrchestrator createTabModelOrchestrator() {
        return getCustomTabActivityTabFactory().createTabModelOrchestrator();
    }

    @Override
    protected void destroyTabModels() {
        if (mTabFactory != null) {
            mTabFactory.destroyTabModelOrchestrator();
        }

        // If tab models have not been initialized, any early created tabs would leak.
        if (mTabProvider != null
                && mTabProvider.getTab() != null
                && !mTabProvider.getTab().isDestroyed()) {
            mTabProvider.getTab().destroy();
        }
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
        return getIntentDataProvider().getActivityType();
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        getCustomTabActivityTabFactory()
                .getTabModelOrchestrator()
                .onNativeLibraryReady(getTabContentManager());
    }

    @Override
    public TabModelSelectorImpl getTabModelSelector() {
        return (TabModelSelectorImpl) super.getTabModelSelector();
    }

    @Override
    public @Nullable Tab getActivityTab() {
        return mTabProvider.getTab();
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        // Menu icon is at the other side of the toolbar relative to the close button, so it will be
        // at the start when the close button is at the end.
        boolean isMenuIconAtStart =
                mIntentDataProvider.getCloseButtonPosition() == CLOSE_BUTTON_POSITION_END;
        return new CustomTabAppMenuPropertiesDelegate(
                this,
                getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(),
                getTabModelSelector(),
                getToolbarManager(),
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
                mIntentDataProvider.getClientPackageNameIdentitySharing() != null);
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getToolbarLayoutId() {
        return ChromeFeatureList.sCctToolbarRefactor.isEnabled()
                ? R.layout.new_custom_tab_toolbar
                : R.layout.custom_tabs_toolbar;
    }

    @Override
    public boolean shouldPostDeferredStartupForReparentedTab() {
        if (!super.shouldPostDeferredStartupForReparentedTab()) return false;

        // Check {@link CustomTabActivityTabProvider#getInitialTabCreationMode()} because the
        // tab has not yet started loading in the common case due to ordering of
        // {@link ChromeActivity#onStartWithNative()} and
        // {@link CustomTabActivityTabController#onFinishNativeInitialization()}.
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
        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (intentDataProvider.isTrustedWebActivity()
                || intentDataProvider.isWebappOrWebApkActivity()) {
            // TODO(pshmakov): extract all finishing logic from BaseCustomTabActivity.
            // In addition to TwaFinishHandler, create DefaultFinishHandler, PaymentsFinishHandler,
            // and SeparateTaskActivityFinishHandler, all implementing
            // CustomTabActivityNavigationController#FinishHandler. Pass the mode enum into
            // CustomTabActivityModule, so that it can provide the correct implementation.
            getTwaFinishHandler().onFinish(defaultBehavior);
        } else if (intentDataProvider.isPartialCustomTab()) {
            // WebContents is missing during the close animation due to android:windowIsTranslucent.
            // We let partial CCT handle the animation.
            mBaseCustomTabRootUiCoordinator.handleCloseAnimation(defaultBehavior);
        } else if (reason != HANDLED_BY_OS) {
            // Back events handled by the OS, such as predictive gesture, are removed by the OS.
            // There is no need in overriding their transitions.
            defaultBehavior.run();
        }
    }

    @Override
    public boolean canShowAppMenu() {
        if (getActivityTab() == null || !mToolbarCoordinator.toolbarIsInitialized()) return false;

        return super.canShowAppMenu();
    }

    @Override
    public int getActivityThemeColor() {
        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (intentDataProvider.getColorProvider().hasCustomToolbarColor()) {
            return intentDataProvider.getColorProvider().getToolbarColor();
        }
        return TabState.UNSPECIFIED_THEME_COLOR;
    }

    @Override
    public int getBaseStatusBarColor(Tab tab) {
        // TODO(b/300419189): Pass the CCT Top Bar Color in AGSA intent after Google Bottom Bar is
        // launched
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

        mBaseCustomTabRootUiCoordinator.onDeferredStartup();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
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

        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime.CustomTabs", timeMs);
        @ActivityType int activityType = getActivityType();
        if (activityType == ActivityType.WEBAPP || activityType == ActivityType.WEB_APK) {
            RecordHistogram.recordTimesHistogram(
                    "MobileStartup.IntentToCreationTime.Webapp", timeMs);
        }
        if (activityType == ActivityType.WEB_APK) {
            RecordHistogram.recordTimesHistogram(
                    "MobileStartup.IntentToCreationTime.WebApk", timeMs);
        }
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
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
            int id, boolean fromMenu, @Nullable MotionEventInfo triggeringMotion) {
        // Disable creating new tabs, bookmark, print, help, focus_url, etc.
        if (id == R.id.focus_url_bar
                || id == R.id.all_bookmarks_menu_id
                || id == R.id.help_id
                || id == R.id.recent_tabs_menu_id
                || id == R.id.new_incognito_tab_menu_id
                || id == R.id.new_tab_menu_id) {
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu, triggeringMotion);
    }

    public WebContentsDelegateAndroid getWebContentsDelegate() {
        assert getCustomTabDelegateFactory() != null;
        return getCustomTabDelegateFactory().getWebContentsDelegate();
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
     * otherwise.
     */
    public @Nullable String getTwaPackage() {
        return mTwaCoordinator == null ? null : mTwaCoordinator.getTwaPackage();
    }

    @Override
    public void maybePreconnect() {
        // The ids need to be set early on, this way prewarming will pick them up.
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
        if (mIntentDataProvider.shouldSuppressAppMenu()) return false;

        return super.supportsAppMenu();
    }

    @Override
    protected boolean shouldShowTabOnActivityShown() {
        // Hidden tabs from speculation will be shown and added to the tab model in
        // CustomTabActivityTabController#finalizeCreatingTab.
        return didFinishNativeInitialization()
                || mTabProvider.getInitialTabCreationMode() != TabCreationMode.HIDDEN;
    }

    @Override
    protected boolean wasInPictureInPictureForMinimizedCustomTabs() {
        if (!MinimizedFeatureUtils.isMinimizedCustomTabAvailable(
                this, getCustomTabFeatureOverridesManager())) {
            return false;
        }
        return mLastPipMode == PictureInPictureMode.MINIMIZED_CUSTOM_TAB;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mCipherFactory.saveToBundle(outState);
    }

    public TabObserverRegistrar getTabObserverRegistrar() {
        return mTabObserverRegistrar;
    }

    public CustomTabActivityTabProvider getCustomTabActivityTabProvider() {
        return mTabProvider;
    }

    private CustomTabObserver getCustomTabObserver() {
        return mCustomTabObserver;
    }

    private CustomTabNavigationEventObserver getCustomTabNavigationEventObserver() {
        return mCustomTabNavigationEventObserver;
    }

    private Verifier createVerifier() {
        return switch (getActivityType()) {
            case ActivityType.WEB_APK -> new WebApkVerifier(mIntentDataProvider);
            case ActivityType.WEBAPP -> new AddToHomescreenVerifier(mIntentDataProvider);
            case ActivityType.TRUSTED_WEB_ACTIVITY -> new TwaVerifier(
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
        return mClientPackageNameProvider;
    }

    private CipherFactory getCipherFactory() {
        return mCipherFactory;
    }

    private TwaFinishHandler getTwaFinishHandler() {
        if (mTwaFinishHandler == null) {
            mTwaFinishHandler = new TwaFinishHandler(this, mIntentDataProvider);
        }
        return mTwaFinishHandler;
    }

    private CloseButtonVisibilityManager getCloseButtonVisibilityManager() {
        if (mCloseButtonVisibilityManager == null) {
            mCloseButtonVisibilityManager =
                    new CloseButtonVisibilityManager(getIntentDataProvider());
        }
        return mCloseButtonVisibilityManager;
    }

    private CustomTabBrowserControlsVisibilityDelegate
            getCustomTabBrowserControlsVisibilityDelegate() {
        if (mCustomTabBrowserControlsVisibilityDelegate == null) {
            mCustomTabBrowserControlsVisibilityDelegate =
                    new CustomTabBrowserControlsVisibilityDelegate(this::getBrowserControlsManager);
        }
        return mCustomTabBrowserControlsVisibilityDelegate;
    }

    public Supplier<BottomSheetController> getBottomSheetController() {
        return mRootUiCoordinator::getBottomSheetController;
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
        return mCurrentPageVerifier;
    }

    private @Nullable AuthTabVerifier getAuthTabVerifier() {
        return mAuthTabVerifier;
    }

    private CustomTabActivityClientConnectionKeeper getCustomTabActivityClientConnectionKeeper() {
        return mCustomTabActivityClientConnectionKeeper;
    }

    private CustomTabFeatureOverridesManager getCustomTabFeatureOverridesManager() {
        if (mCustomTabFeatureOverridesManager == null) {
            mCustomTabFeatureOverridesManager =
                    new CustomTabFeatureOverridesManager(getIntentDataProvider());
        }
        return mCustomTabFeatureOverridesManager;
    }

    private CustomTabOrientationController getCustomTabOrientationController() {
        if (mCustomTabOrientationController == null) {
            mCustomTabOrientationController =
                    new CustomTabOrientationController(getWindowAndroid(), getIntentDataProvider());
        }
        return mCustomTabOrientationController;
    }

    private ImmersiveModeController createImmersiveModeController() {
        return new ImmersiveModeController(this, getWindowAndroid(), getLifecycleDispatcher());
    }

    private CustomTabToolbarColorController getCustomTabToolbarColorController() {
        return mCustomTabToolbarColorController;
    }

    private CustomTabStatusBarColorProvider getCustomTabStatusBarColorProvider() {
        if (mStatusBarColorProvider == null) {
            mStatusBarColorProvider =
                    new CustomTabStatusBarColorProvider(
                            getIntentDataProvider(),
                            mRootUiCoordinator.getStatusBarColorController());
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
        return mCustomTabCompositorContentInitializer;
    }

    protected CustomTabBottomBarDelegate getCustomTabBottomBarDelegate() {
        return mCustomTabBottomBarDelegate;
    }

    private CustomTabDelegateFactory getCustomTabDelegateFactory() {
        if (mDelegateFactory == null) {
            mDelegateFactory =
                    new CustomTabDelegateFactory(
                            this,
                            getIntentDataProvider(),
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
                            getBrowserControlsManager());
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
            mTabFactory =
                    new CustomTabActivityTabFactory(
                            this,
                            getCustomTabTabPersistencePolicy(),
                            getWindowAndroid(),
                            getProfileProviderSupplier(),
                            getCustomTabDelegateFactory(),
                            getIntentDataProvider(),
                            this,
                            getTabModelSelectorSupplier(),
                            getCompositorViewHolderSupplier(),
                            getCipherFactory());
        }
        return mTabFactory;
    }

    private CustomTabActivityTabController getCustomTabActivityTabController() {
        return mTabController;
    }

    private WebappDeferredStartupWithStorageHandler getWebappDeferredStartupWithStorageHandler() {
        if (mWebappDeferredStartupWithStorageHandler == null) {
            mWebappDeferredStartupWithStorageHandler =
                    new WebappDeferredStartupWithStorageHandler(this, getIntentDataProvider());
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
        return mNavigationController;
    }

    private CustomTabMinimizationManagerHolder getCustomTabMinimizationManagerHolder() {
        return mMinimizationManagerHolder;
    }

    private DisclosureInfobar createDisclosureInfobar() {
        return new DisclosureInfobar(
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
        return mToolbarCoordinator;
    }

    private TrustedWebActivityBrowserControlsVisibilityManager
            createTrustedWebActivityBrowserControlsVisibilityManager() {
        if (mBrowserControlsVisibilityManager != null) {
            return mBrowserControlsVisibilityManager;
        }

        mBrowserControlsVisibilityManager =
                new TrustedWebActivityBrowserControlsVisibilityManager(
                        getTabObserverRegistrar(),
                        getCustomTabActivityTabProvider(),
                        getCustomTabToolbarCoordinator(),
                        getCloseButtonVisibilityManager(),
                        getIntentDataProvider());
        return mBrowserControlsVisibilityManager;
    }

    private SharedActivityCoordinator getSharedActivityCoordinator() {
        if (mSharedActivityCoordinator == null) {
            TrustedWebActivityBrowserControlsVisibilityManager controlsVisibilityManager =
                    createTrustedWebActivityBrowserControlsVisibilityManager();
            mSharedActivityCoordinator =
                    new SharedActivityCoordinator(
                            getCurrentPageVerifier(),
                            controlsVisibilityManager,
                            getCustomTabStatusBarColorProvider(),
                            this::createImmersiveModeController,
                            getIntentDataProvider(),
                            getCustomTabOrientationController(),
                            getCustomTabActivityNavigationController(),
                            getVerifier(),
                            getBrowserServicesThemeColorProvider(),
                            getLifecycleDispatcher());
        }
        return mSharedActivityCoordinator;
    }

    @ChecksSdkIntAtLeast(api = Build.VERSION_CODES.VANILLA_ICE_CREAM)
    private AppHeaderCoordinator getAppHeaderCoordinator() {
        if (!WebAppHeaderUtils.isMinimalUiEnabled(getIntentDataProvider())) return null;
        if (mAppHeaderCoordinator != null) return mAppHeaderCoordinator;

        mAppHeaderCoordinator =
                new AppHeaderCoordinator(
                        this,
                        getWindow().getDecorView().getRootView(),
                        getBrowserControlsManager().getBrowserVisibilityDelegate(),
                        getInsetObserver(),
                        getLifecycleDispatcher(),
                        getSavedInstanceState(),
                        getEdgeToEdgeManager().getEdgeToEdgeStateProvider());

        return mAppHeaderCoordinator;
    }

    private BrowserServicesThemeColorProvider getBrowserServicesThemeColorProvider() {
        if (mBrowserServicesThemeColorProvider != null) return mBrowserServicesThemeColorProvider;

        mBrowserServicesThemeColorProvider =
                new BrowserServicesThemeColorProvider(
                        this,
                        getIntentDataProvider(),
                        getTopUiThemeColorProvider(),
                        getCustomTabActivityTabProvider(),
                        getTabObserverRegistrar());
        return mBrowserServicesThemeColorProvider;
    }

    protected @Nullable WebappActivityCoordinator getWebappActivityCoordinator() {
        return mWebappActivityCoordinator;
    }

    protected BaseCustomTabRootUiCoordinator getBaseCustomTabRootUiCoordinator() {
        return mBaseCustomTabRootUiCoordinator;
    }
}
