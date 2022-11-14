// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.USER_NAVIGATION;

import android.app.Activity;
import android.content.Intent;
import android.util.Pair;
import android.view.KeyEvent;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.TrustedWebUtils;

import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.browserservices.ui.controller.Verifier;
import org.chromium.chrome.browser.browserservices.ui.trustedwebactivity.TrustedWebActivityCoordinator;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.BaseCustomTabActivityModule;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.dependency_injection.ModuleFactoryOverrides;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.metrics.UmaSessionStats;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.PowerSavingModeMonitor;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.theme.TopUiThemeColorProvider;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.webapps.SameTaskWebApkActivity;
import org.chromium.chrome.browser.webapps.WebappActivityCoordinator;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;

/**
 * Contains functionality which is shared between {@link WebappActivity} and
 * {@link CustomTabActivity}. Purpose of the class is to simplify merging {@link WebappActivity}
 * and {@link CustomTabActivity}.
 */
public abstract class BaseCustomTabActivity extends ChromeActivity<BaseCustomTabActivityComponent> {
    protected static Integer sOverrideCoreCountForTesting;

    // Fallback study name used for experiments ids.
    public static final String GSA_FALLBACK_STUDY_NAME = "GsaExperiments";

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
    protected Verifier mVerifier;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    protected boolean mShouldOverridePackage;

    @VisibleForTesting
    public static void setOverrideCoreCount(int coreCount) {
        sOverrideCoreCountForTesting = coreCount;
    }

    /**
     * Builds {@link BrowserServicesIntentDataProvider} for this {@link CustomTabActivity}.
     */
    protected abstract BrowserServicesIntentDataProvider buildIntentDataProvider(
            Intent intent, @CustomTabsIntent.ColorScheme int colorScheme);

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
        mNightModeStateController = new CustomTabNightModeStateController(getLifecycleDispatcher(),
                SystemNightModeMonitor.getInstance(), PowerSavingModeMonitor.getInstance());
        return mNightModeStateController;
    }

    @Override
    protected void initializeNightModeStateProvider() {
        mNightModeStateController.initialize(getDelegate(), getIntent());
    }

    @Override
    public void onNewIntent(Intent intent) {
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
        // clang-format off
        mBaseCustomTabRootUiCoordinator = new BaseCustomTabRootUiCoordinator(this,
                getShareDelegateSupplier(), getActivityTabProvider(), mTabModelProfileSupplier,
                mBookmarkModelSupplier, mTabBookmarkerSupplier,
                getContextualSearchManagerSupplier(), getTabModelSelectorSupplier(),
                getBrowserControlsManager(), getWindowAndroid(), getLifecycleDispatcher(),
                getLayoutManagerSupplier(),
                /* menuOrKeyboardActionController= */ this, this::getActivityThemeColor,
                getModalDialogManagerSupplier(), /* appMenuBlocker= */ this, this::supportsAppMenu,
                this::supportsFindInPage, getTabCreatorManagerSupplier(), getFullscreenManager(),
                getCompositorViewHolderSupplier(), getTabContentManagerSupplier(),
                this::getSnackbarManager, getActivityType(),
                this::isInOverviewMode, this::isWarmOnResume, /* appMenuDelegate= */ this,
                /* statusBarColorProvider= */ this, getIntentRequestTracker(),
                () -> mToolbarCoordinator, () -> mNavigationController, () -> mIntentDataProvider,
                () -> mDelegateFactory.getEphemeralTabCoordinator(), mBackPressManager,
                () -> mTabController);
        // clang-format on
        return mBaseCustomTabRootUiCoordinator;
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
        IntentIgnoringCriterion intentIgnoringCriterion = (intent)
                -> mIntentHandler.shouldIgnoreIntent(
                        intent, /*startedActivity=*/true, isCustomTab());

        BaseCustomTabActivityModule baseCustomTabsModule = overridenBaseCustomTabFactory != null
                ? overridenBaseCustomTabFactory.create(mIntentDataProvider,
                        mNightModeStateController, intentIgnoringCriterion,
                        getTopUiThemeColorProvider(), new DefaultBrowserProviderImpl())
                : new BaseCustomTabActivityModule(mIntentDataProvider, mNightModeStateController,
                        intentIgnoringCriterion, getTopUiThemeColorProvider(),
                        new DefaultBrowserProviderImpl());
        BaseCustomTabActivityComponent component =
                ChromeApplicationImpl.getComponent().createBaseCustomTabActivityComponent(
                        commonsModule, baseCustomTabsModule);

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
        mNavigationController.setFinishHandler((reason) -> {
            if (reason == USER_NAVIGATION) connectionKeeper.recordClientConnectionStatus();
            handleFinishAndClose();
        });
        component.resolveSessionHandler();

        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();

        if (intentDataProvider.isIncognito()) {
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

        if (mIntentDataProvider.isPartialHeightCustomTab()) {
            overridePendingTransition(R.anim.slide_in_up, R.anim.no_anim);
        }

        WebappExtras webappExtras = getIntentDataProvider().getWebappExtras();
        if (webappExtras != null) {
            // Set the title for web apps so that TalkBack says the web app's short name instead of
            // 'Chrome' or the activity's label ("Web app") when either launching the web app or
            // bringing it to the foreground via Android Recents.
            setTitle(webappExtras.shortName);
        }
    }

    private int getColorScheme() {
        if (mNightModeStateController != null) {
            return mNightModeStateController.isInNightMode() ? COLOR_SCHEME_DARK
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
            UsageStatsService.createPageViewObserverIfEnabled(
                    this, getActivityTabProvider(), getTabContentManagerSupplier());
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
    @ActivityType
    public int getActivityType() {
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
    @Nullable
    public Tab getActivityTab() {
        return mTabProvider.getTab();
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        // Menu icon is at the other side of the toolbar relative to the close button, so it will be
        // at the start when the close button is at the end.
        boolean isMenuIconAtStart = mIntentDataProvider.getCloseButtonPosition()
                == BrowserServicesIntentDataProvider.CLOSE_BUTTON_POSITION_END;
        return new CustomTabAppMenuPropertiesDelegate(this, getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(), getTabModelSelector(), getToolbarManager(),
                getWindow().getDecorView(), mBookmarkModelSupplier, mVerifier,
                mIntentDataProvider.getUiType(), mIntentDataProvider.getMenuTitles(),
                mIntentDataProvider.isOpenedByChrome(),
                mIntentDataProvider.shouldShowShareMenuItem(),
                mIntentDataProvider.shouldShowStarButton(),
                mIntentDataProvider.shouldShowDownloadButton(), mIntentDataProvider.isIncognito(),
                isMenuIconAtStart);
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
    public int getControlContainerHeightResource() {
        return R.dimen.custom_tabs_control_container_height;
    }

    @Override
    public boolean shouldPostDeferredStartupForReparentedTab() {
        if (!super.shouldPostDeferredStartupForReparentedTab()) return false;

        // Check {@link CustomTabActivityTabProvider#getInitialTabCreationMode()} because the
        // tab has not yet started loading in the common case due to ordering of
        // {@link ChromeActivity#onStartWithNative()} and
        // {@link CustomTabActivityTabController#onFinishNativeInitialization()}.
        @TabCreationMode
        int mode = mTabProvider.getInitialTabCreationMode();
        return (mode == TabCreationMode.HIDDEN || mode == TabCreationMode.EARLY);
    }

    @Override
    protected boolean handleBackPressed() {
        return mNavigationController.navigateOnBack();
    }

    @Override
    protected void initializeBackPressHandling() {
        super.initializeBackPressHandling();
        if (!BackPressManager.isEnabled()) return;
        getOnBackPressedDispatcher().addCallback(new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                handleBackPressed();
            }
        });
    }

    @Override
    public void finish() {
        super.finish();
        BrowserServicesIntentDataProvider intentDataProvider = getIntentDataProvider();
        if (intentDataProvider != null && intentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            overridePendingTransition(intentDataProvider.getAnimationEnterRes(),
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
    protected void handleFinishAndClose() {
        Runnable defaultBehavior = () -> {
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
        } else if (intentDataProvider.isPartialHeightCustomTab()
                && intentDataProvider.shouldAnimateOnFinish()) {
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
        return mStatusBarColorProvider.getBaseStatusBarColor(tab);
    }

    @Override
    public void initDeferredStartupForActivity() {
        if (mWebappActivityCoordinator != null) {
            mWebappActivityCoordinator.initDeferredStartupForActivity();
        }
        DeferredStartupHandler.getInstance().addDeferredTask(
                () -> { mBaseCustomTabRootUiCoordinator.onDeferredStartup(); });
        super.initDeferredStartupForActivity();
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(event,
                mToolbarCoordinator.toolbarIsInitialized(), getFullscreenManager(),
                /* menuOrKeyboardActionController= */ this);
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime.CustomTabs", timeMs);
        @ActivityType
        int activityType = getActivityType();
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
        return KeyboardShortcuts.onKeyDown(event, true, false, getTabModelSelector(),
                       /* menuOrKeyboardActionController= */ this, getToolbarManager())
                || super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating new tabs, bookmark, history, print, help, focus_url, etc.
        if (id == R.id.focus_url_bar || id == R.id.all_bookmarks_menu_id
                || id == R.id.add_to_reading_list_menu_id || id == R.id.help_id
                || id == R.id.recent_tabs_menu_id || id == R.id.new_incognito_tab_menu_id
                || id == R.id.new_tab_menu_id || id == R.id.open_history_menu_id) {
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
    @Nullable
    public String getTwaPackage() {
        return mTwaCoordinator == null ? null : mTwaCoordinator.getTwaPackage();
    }

    @Override
    public void maybePreconnect() {
        // The ids need to be set early on, this way prewarming will pick them up.
        int[] experimentIds = mIntentDataProvider.getGsaExperimentIds();
        if (experimentIds != null) {
            // When ids are set through the intent, we don't want them to override the existing ids.
            boolean override = false;
            UmaSessionStats.registerExternalExperiment(
                    GSA_FALLBACK_STUDY_NAME, experimentIds, override);
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
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.CCT_PREFETCH_DELAY_SHOW_ON_START)) {
            return true;
        }
        // Hidden tabs from speculation will be shown and added to the tab model in
        // CustomTabActivityTabController#finalizeCreatingTab.
        return didFinishNativeInitialization()
                || mTabProvider.getInitialTabCreationMode() != TabCreationMode.HIDDEN;
    }
}
