// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.CLOSE_BUTTON_POSITION_END;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.USER_NAVIGATION;

import android.app.Activity;
import android.content.Intent;
import android.os.Bundle;
import android.util.Pair;
import android.view.KeyEvent;

import androidx.annotation.AnimRes;
import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;

import org.chromium.base.IntentUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider.CustomTabProfileType;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.ui.controller.AuthTabVerifier;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizationManagerHolder;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.CustomTabMinimizeDelegate;
import org.chromium.chrome.browser.customtabs.features.minimizedcustomtab.MinimizedFeatureUtils;
import org.chromium.chrome.browser.customtabs.features.partialcustomtab.PartialCustomTabDisplayManager;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager.Observer;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.init.ActivityProfileProvider;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.PowerSavingModeMonitor;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.ui.google_bottom_bar.GoogleBottomBarCoordinator;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.webapps.SameTaskWebApkActivity;
import org.chromium.chrome.browser.webapps.WebappActivityCoordinator;
import org.chromium.components.browser_ui.share.ShareHelper;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;

/**
 * Contains functionality which is shared between {@link WebappActivity} and {@link
 * CustomTabActivity}. Purpose of the class is to simplify merging {@link WebappActivity} and {@link
 * CustomTabActivity}.
 */
public abstract class BaseCustomTabActivity extends ChromeActivity<BaseCustomTabActivityComponent> {
    protected static Integer sOverrideCoreCountForTesting;

    private final CipherFactory mCipherFactory = new CipherFactory();

    protected BaseCustomTabRootUiCoordinator mBaseCustomTabRootUiCoordinator;
    protected BrowserServicesIntentDataProvider mIntentDataProvider;
    protected CustomTabDelegateFactory mDelegateFactory;
    protected CustomTabToolbarCoordinator mToolbarCoordinator;
    protected CustomTabActivityNavigationController mNavigationController;
    protected CustomTabActivityTabController mTabController;
    protected CustomTabActivityTabProvider mTabProvider;
    protected CustomTabStatusBarColorProvider mStatusBarColorProvider;
    protected CustomTabActivityTabFactory mTabFactory;
    protected CustomTabIntentHandler mCustomTabIntentHandler;
    protected CustomTabNightModeStateController mNightModeStateController;
    protected @Nullable WebappActivityCoordinator mWebappActivityCoordinator;
    protected @Nullable TrustedWebActivityCoordinator mTwaCoordinator;
    protected @Nullable AuthTabVerifier mAuthTabVerifier;
    protected Verifier mVerifier;
    protected FullscreenManager mFullscreenManager;
    protected CustomTabMinimizationManagerHolder mMinimizationManagerHolder;
    protected CustomTabFeatureOverridesManager mFeatureOverridesManager;
    private boolean mWarmupOnDestroy;

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
            return new AuthTabIntentDataProvider(intent, this);
        } else if (IncognitoCustomTabIntentDataProvider.isValidIncognitoIntent(intent)) {
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
        // This is called before Dagger component is created, so using getInstance() directly.
        mNightModeStateController =
                new CustomTabNightModeStateController(
                        getLifecycleDispatcher(),
                        SystemNightModeMonitor.getInstance(),
                        PowerSavingModeMonitor.getInstance());
        return mNightModeStateController;
    }

    @Override
    protected void initializeNightModeStateProvider() {
        mNightModeStateController.initialize(getDelegate(), getIntent());
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
                        this::getLastUserInteractionTime,
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
                        () -> mNavigationController,
                        () -> mIntentDataProvider,
                        mBackPressManager,
                        () -> mTabController,
                        () -> mMinimizationManagerHolder.getMinimizationManager(),
                        () -> mFeatureOverridesManager,
                        getBaseChromeLayout());
        return mBaseCustomTabRootUiCoordinator;
    }

    @Override
    protected OneshotSupplier<ProfileProvider> createProfileProvider() {
        return new ActivityProfileProvider(getLifecycleDispatcher()) {
            @Nullable
            @Override
            protected OTRProfileID createOffTheRecordProfileID() {
                switch (getIntentDataProvider().getCustomTabMode()) {
                    case CustomTabProfileType.INCOGNITO:
                        return OTRProfileID.createUniqueIncognitoCCTId();
                    case CustomTabProfileType.EPHEMERAL:
                        return OTRProfileID.createUnique("CCT:Ephemeral");
                    default:
                        throw new IllegalStateException(
                                "Attempting to create an OTR profile in a non-OTR session");
                }
            }
        };
    }

    @Override
    public boolean shouldAllocateChildConnection() {
        return mTabController.shouldAllocateChildConnection();
    }

    @Override
    protected boolean shouldPreferLightweightFre(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(
                intent, TrustedWebUtils.EXTRA_LAUNCH_AS_TRUSTED_WEB_ACTIVITY, false);
    }

    @Override
    protected BaseCustomTabActivityComponent createComponent(
            ChromeActivityCommonsModule commonsModule) {
        BaseCustomTabActivityModule.Factory overridenBaseCustomTabFactory =
                ModuleFactoryOverrides.getOverrideFor(BaseCustomTabActivityModule.Factory.class);

        // mIntentHandler comes from the base class.
        IntentIgnoringCriterion intentIgnoringCriterion =
                (intent) -> IntentHandler.shouldIgnoreIntent(intent, this, isCustomTab());

        BaseCustomTabActivityModule baseCustomTabsModule =
                overridenBaseCustomTabFactory != null
                        ? overridenBaseCustomTabFactory.create(
                                mIntentDataProvider,
                                mNightModeStateController,
                                intentIgnoringCriterion,
                                getTopUiThemeColorProvider(),
                                new DefaultBrowserProviderImpl(),
                                mCipherFactory)
                        : new BaseCustomTabActivityModule(
                                mIntentDataProvider,
                                mNightModeStateController,
                                intentIgnoringCriterion,
                                getTopUiThemeColorProvider(),
                                new DefaultBrowserProviderImpl(),
                                mCipherFactory);
        BaseCustomTabActivityComponent component =
                ChromeApplicationImpl.getComponent()
                        .createBaseCustomTabActivityComponent(commonsModule, baseCustomTabsModule);

        mDelegateFactory = component.resolveTabDelegateFactory();
        mToolbarCoordinator = component.resolveToolbarCoordinator();
        mNavigationController = component.resolveNavigationController();
        mTabController = component.resolveTabController();
        mTabProvider = component.resolveTabProvider();
        mStatusBarColorProvider = component.resolveCustomTabStatusBarColorProvider();
        mTabFactory = component.resolveTabFactory();
        mCustomTabIntentHandler = component.resolveIntentHandler();
        mVerifier = component.resolveVerifier();

        component.resolveCompositorContentInitializer();
        component.resolveTaskDescriptionHelper();
        component.resolveUmaTracker();
        component.resolveDownloadObserver();
        CustomTabActivityClientConnectionKeeper connectionKeeper =
                component.resolveConnectionKeeper();
        mNavigationController.setFinishHandler(
                (reason, warmupOnFinish) -> {
                    if (reason == USER_NAVIGATION) connectionKeeper.recordClientConnectionStatus();
                    handleFinishAndClose(warmupOnFinish);
                });
        if (BackPressManager.isEnabled()) {
            mBackPressManager.setFallbackOnBackPressed(this::handleBackPressed);
            mBackPressManager.addHandler(
                    mNavigationController, BackPressHandler.Type.MINIMIZE_APP_AND_CLOSE_TAB);
        }
        component.resolveSessionHandler();

        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();

        // We need the CustomTabIncognitoManager for all OffTheRecord profiles to ensure
        // that they are destroyed when a CCT session ends.
        if (intentDataProvider.isOffTheRecord()) {
            component.resolveCustomTabIncognitoManager();
        }

        if (intentDataProvider.isWebappOrWebApkActivity()) {
            mWebappActivityCoordinator = component.resolveWebappActivityCoordinator();
        }

        if (intentDataProvider.isWebApkActivity()) {
            component.resolveWebApkActivityCoordinator();
        }

        if (mIntentDataProvider.isTrustedWebActivity()) {
            mTwaCoordinator = component.resolveTrustedWebActivityCoordinator();
        }

        if (mIntentDataProvider.isAuthTab()) {
            mAuthTabVerifier = component.resolveAuthTabVerifier();
        }

        mMinimizationManagerHolder = component.resolveCustomTabMinimizationManagerHolder();
        mFeatureOverridesManager = component.resolveCustomTabFeatureOverridesManager();
        mTabFactory.setActivityType(getActivityType());
        mDelegateFactory.setEphemeralTabCoordinatorSupplier(
                mRootUiCoordinator.getEphemeralTabCoordinatorSupplier());
        return component;
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

        super.performPreInflationStartup();

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

        mMinimizationManagerHolder.maybeCreateMinimizationManager(mTabModelProfileSupplier);
        var minimizationManager = mMinimizationManagerHolder.getMinimizationManager();
        if (minimizationManager != null) {
            getFullscreenManager().addObserver(mFullscreenObserver);
            minimizationManager.addObserver(mMinimizationObserver);
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
            Profile profile = getProfileProviderSupplier().get().getOriginalProfile();
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> CustomTabsConnection.createSpareWebContents(profile));
        }

        if (mTabController != null) {
            mTabController.destroy();
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

    private static int getCoreCount() {
        if (sOverrideCoreCountForTesting != null) return sOverrideCoreCountForTesting;
        return Runtime.getRuntime().availableProcessors();
    }

    /**
     * @return {@link ThemeColorProvider} for top UI.
     */
    private TopUiThemeColorProvider getTopUiThemeColorProvider() {
        return mRootUiCoordinator.getTopUiThemeColorProvider();
    }

    @Override
    public void initializeState() {
        super.initializeState();

        // TODO(pkotwicz): Determine whether finishing tab initialization in initializeState() has a
        // positive performance impact.
        if (getIntentDataProvider().isWebappOrWebApkActivity()) {
            mTabController.finishNativeInitialization();
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
            mTabController.finishNativeInitialization();
        }

        super.finishNativeInitialization();
    }

    @Override
    protected TabModelOrchestrator createTabModelOrchestrator() {
        return mTabFactory.createTabModelOrchestrator();
    }

    @Override
    protected void destroyTabModels() {
        if (mTabFactory != null) {
            mTabFactory.destroyTabModelOrchestrator();
        }
    }

    @Override
    protected void createTabModels() {
        mTabFactory.createTabModels();
    }

    @Override
    protected Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return mTabFactory.createTabCreators();
    }

    @Override
    public @ActivityType int getActivityType() {
        return getIntentDataProvider().getActivityType();
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        mTabFactory.getTabModelOrchestrator().onNativeLibraryReady(getTabContentManager());
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
                mVerifier,
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
                mIntentDataProvider.getClientPackageNameIdentitySharing() != null);
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.custom_tabs_control_container;
    }

    @Override
    protected int getToolbarLayoutId() {
        return R.layout.custom_tabs_toolbar;
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

    @Override
    protected boolean handleBackPressed() {
        if (!BackPressManager.isEnabled()
                && getTabModalLifetimeHandler() != null
                && getTabModalLifetimeHandler().onBackPressed()) {
            BackPressManager.record(BackPressHandler.Type.TAB_MODAL_HANDLER);
            return true;
        }
        if (BackPressManager.correctTabNavigationOnFallback()) {
            if (getToolbarManager() != null && getToolbarManager().back()) {
                return true;
            }
        }
        return mNavigationController.navigateOnBack();
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
    protected void handleFinishAndClose(boolean warmupOnFinish) {
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
            getComponent().resolveTwaFinishHandler().onFinish(defaultBehavior);
        } else if (intentDataProvider.isPartialCustomTab()) {
            // WebContents is missing during the close animation due to android:windowIsTranslucent.
            // We let partial CCT handle the animation.
            mBaseCustomTabRootUiCoordinator.handleCloseAnimation(defaultBehavior);
        } else {
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
        return mStatusBarColorProvider.getBaseStatusBarColor(tab);
    }

    @Override
    public void initDeferredStartupForActivity() {
        if (mWebappActivityCoordinator != null) {
            mWebappActivityCoordinator.initDeferredStartupForActivity();
        }
        DeferredStartupHandler.getInstance()
                .addDeferredTask(
                        () -> {
                            if (isActivityFinishingOrDestroyed()) return;
                            mBaseCustomTabRootUiCoordinator.onDeferredStartup();
                        });
        super.initDeferredStartupForActivity();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result =
                KeyboardShortcuts.dispatchKeyEvent(
                        event,
                        mToolbarCoordinator.toolbarIsInitialized(),
                        getFullscreenManager(),
                        /* menuOrKeyboardActionController= */ this);
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
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating new tabs, bookmark, print, help, focus_url, etc.
        if (id == R.id.focus_url_bar
                || id == R.id.all_bookmarks_menu_id
                || id == R.id.help_id
                || id == R.id.recent_tabs_menu_id
                || id == R.id.new_incognito_tab_menu_id
                || id == R.id.new_tab_menu_id) {
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    public WebContentsDelegateAndroid getWebContentsDelegate() {
        assert mDelegateFactory != null;
        return mDelegateFactory.getWebContentsDelegate();
    }

    /**
     * @return Whether the app is running in the "Trusted Web Activity" mode, where the TWA-specific
     *         UI is shown.
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
        if (!MinimizedFeatureUtils.isMinimizedCustomTabAvailable(this, mFeatureOverridesManager)) {
            return false;
        }
        return mLastPipMode == PictureInPictureMode.MINIMIZED_CUSTOM_TAB;
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        mCipherFactory.saveToBundle(outState);
    }
}
