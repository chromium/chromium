// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_DARK;
import static androidx.browser.customtabs.CustomTabsIntent.COLOR_SCHEME_LIGHT;

import static org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController.FinishReason.USER_NAVIGATION;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Bundle;
import android.provider.Browser;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.KeyboardShortcuts;
import org.chromium.chrome.browser.LaunchIntentDispatcher;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantFacade;
import org.chromium.chrome.browser.browserservices.BrowserServicesIntentDataProvider.CustomTabsUiType;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityNavigationController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabController;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabFactory;
import org.chromium.chrome.browser.customtabs.content.CustomTabActivityTabProvider;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler;
import org.chromium.chrome.browser.customtabs.content.CustomTabIntentHandler.IntentIgnoringCriterion;
import org.chromium.chrome.browser.customtabs.content.TabCreationMode;
import org.chromium.chrome.browser.customtabs.dependency_injection.CustomTabActivityComponent;
import org.chromium.chrome.browser.customtabs.dependency_injection.CustomTabActivityModule;
import org.chromium.chrome.browser.customtabs.dynamicmodule.DynamicModuleCoordinator;
import org.chromium.chrome.browser.customtabs.features.CustomTabNavigationBarController;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbarCoordinator;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityCommonsModule;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.night_mode.NightModeStateProvider;
import org.chromium.chrome.browser.night_mode.NightModeUtils;
import org.chromium.chrome.browser.night_mode.PowerSavingModeMonitor;
import org.chromium.chrome.browser.night_mode.SystemNightModeMonitor;
import org.chromium.chrome.browser.page_info.PageInfoController;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/**
 * The activity for custom tabs. It will be launched on top of a client's task.
 */
public class CustomTabActivity extends ChromeActivity<CustomTabActivityComponent> {
    private CustomTabIntentDataProvider mIntentDataProvider;
    private CustomTabsSessionToken mSession;
    private CustomTabActivityTabController mTabController;
    private CustomTabActivityTabProvider mTabProvider;
    private CustomTabActivityTabFactory mTabFactory;
    private CustomTabActivityNavigationController mNavigationController;
    private CustomTabStatusBarColorProvider mCustomTabStatusBarColorProvider;
    private CustomTabToolbarCoordinator mToolbarCoordinator;
    private CustomTabIntentHandler mCustomTabIntentHandler;

    // This is to give the right package name while using the client's resources during an
    // overridePendingTransition call.
    // TODO(ianwen, yusufo): Figure out a solution to extract external resources without having to
    // change the package name.
    private boolean mShouldOverridePackage;

    private final CustomTabsConnection mConnection = CustomTabsConnection.getInstance();

    @Nullable
    private DynamicModuleCoordinator mDynamicModuleCoordinator;

    private CustomTabNightModeStateController mNightModeStateController;

    /**
     * Return true when the activity has been launched in a separate task. The default behavior is
     * to reuse the same task and put the activity on top of the previous one (i.e hiding it). A
     * separate task creates a new entry in the Android recent screen.
     **/
    private boolean useSeparateTask() {
        final int separateTaskFlags =
                Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT;
        return (getIntent().getFlags() & separateTaskFlags) != 0;
    }

    private CustomTabActivityTabProvider.Observer mTabChangeObserver =
            new CustomTabActivityTabProvider.Observer() {
        @Override
        public void onInitialTabCreated(@NonNull Tab tab, int mode) {
            resetPostMessageHandlersForCurrentSession();
        }

        @Override
        public void onTabSwapped(@NonNull Tab tab) {
            resetPostMessageHandlersForCurrentSession();
        }

        @Override
        public void onAllTabsClosed() {
            resetPostMessageHandlersForCurrentSession();
        }
    };

    @Override
    protected RootUiCoordinator createRootUiCoordinator() {
        // TODO(https://crbug.com/1020324): Move this logic into a CustomTabRootUICoordinator that
        // can synchronously orchestrate the creation of child coordinators.
        return new RootUiCoordinator(this, (toolbarManager) -> {
            mToolbarCoordinator.onToolbarInitialized(toolbarManager);
            mNavigationController.onToolbarInitialized(toolbarManager);
        }, null, getShareDelegate());
    }

    @Override
    protected Drawable getBackgroundDrawable() {
        int initialBackgroundColor = mIntentDataProvider.getInitialBackgroundColor();
        if (mIntentDataProvider.isTrustedIntent() && initialBackgroundColor != Color.TRANSPARENT) {
            return new ColorDrawable(initialBackgroundColor);
        } else {
            return super.getBackgroundDrawable();
        }
    }

    @Override
    public @ActivityType int getActivityType() {
        return ActivityType.CUSTOM_TAB;
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);

        RecordHistogram.recordTimesHistogram(
                "MobileStartup.IntentToCreationTime.CustomTabs", timeMs);
    }

    @Override
    public void performPreInflationStartup() {
        // Parse the data from the Intent before calling super to allow the Intent to customize
        // the Activity parameters, including the background of the page.
        // Note that color scheme is fixed for the lifetime of Activity: if the system setting
        // changes, we recreate the activity.
        mIntentDataProvider = new CustomTabIntentDataProvider(getIntent(), this, getColorScheme());

        super.performPreInflationStartup();
        mTabProvider.addObserver(mTabChangeObserver);
        // We might have missed an onInitialTabCreated event.
        resetPostMessageHandlersForCurrentSession();

        mSession = mIntentDataProvider.getSession();

        CustomTabNavigationBarController.updateNavigationBarColor(this, mIntentDataProvider);
    }

    private int getColorScheme() {
        if (mNightModeStateController != null) {
            return mNightModeStateController.isInNightMode() ? COLOR_SCHEME_DARK :
                    COLOR_SCHEME_LIGHT;
        }
        assert false : "NightModeStateController should have been already created";
        return COLOR_SCHEME_LIGHT;
    }

    @Override
    public boolean shouldAllocateChildConnection() {
        return mTabController.shouldAllocateChildConnection();
    }

    @Override
    public void performPostInflationStartup() {
        super.performPostInflationStartup();
        getStatusBarColorController().updateStatusBarColor(mTabProvider.getTab());

        // Properly attach tab's InfoBarContainer to the view hierarchy if the tab is already
        // attached to a ChromeActivity, as the main tab might have been initialized prior to
        // inflation.
        if (mTabProvider.getTab() != null) {
            ViewGroup bottomContainer = (ViewGroup) findViewById(R.id.bottom_container);
            InfoBarContainer.get(mTabProvider.getTab()).setParentView(bottomContainer);
        }

        // Setting task title and icon to be null will preserve the client app's title and icon.
        ApiCompatibilityUtils.setTaskDescription(this, null, null,
                mIntentDataProvider.getToolbarColor());
        getComponent().resolveBottomBarDelegate().showBottomBarIfNecessary();
    }

    @Override
    protected TabModelSelector createTabModelSelector() {
        return mTabFactory.createTabModelSelector();
    }

    @Override
    protected Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return mTabFactory.createTabCreators();
    }

    @Override
    protected NightModeStateProvider createNightModeStateProvider() {
        // This is called before Dagger component is created, so using getInstance() directly.
        mNightModeStateController = new CustomTabNightModeStateController(getLifecycleDispatcher(),
                SystemNightModeMonitor.getInstance(),
                PowerSavingModeMonitor.getInstance());
        return mNightModeStateController;
    }

    @Override
    protected void initializeNightModeStateProvider() {
        mNightModeStateController.initialize(getDelegate(), getIntent());
    }

    @Override
    public void finishNativeInitialization() {
        if (!mIntentDataProvider.isInfoPage()) FirstRunSignInProcessor.start(this);

        // Try to initialize dynamic module early to enqueue navigation events
        // @see DynamicModuleNavigationEventObserver
        if (mIntentDataProvider.isDynamicModuleEnabled()) {
            mDynamicModuleCoordinator = getComponent().resolveDynamicModuleCoordinator();
        }

        mConnection.showSignInToastIfNecessary(mSession, getIntent());

        if (isTaskRoot() && UsageStatsService.isEnabled()) {
            UsageStatsService.getInstance().createPageViewObserver(getTabModelSelector(), this);
        }

        super.finishNativeInitialization();

        // We start the Autofill Assistant after the call to super.finishNativeInitialization() as
        // this will initialize the BottomSheet that is used to embed the Autofill Assistant bottom
        // bar.
        if (isAutofillAssistantEnabled()) {
            AutofillAssistantFacade.start(this);
        }
    }

    @Override
    public void onNewIntent(Intent intent) {
        Intent originalIntent = getIntent();
        super.onNewIntent(intent);
        // Currently we can't handle arbitrary updates of intent parameters, so make sure
        // getIntent() returns the same intent as before.
        setIntent(originalIntent);

        // Color scheme doesn't matter here: currently we don't support updating UI using Intents.
        CustomTabIntentDataProvider dataProvider = new CustomTabIntentDataProvider(intent, this,
                CustomTabsIntent.COLOR_SCHEME_LIGHT);

        mCustomTabIntentHandler.onNewIntent(dataProvider);
    }

    private void resetPostMessageHandlersForCurrentSession() {
        Tab tab = mTabProvider.getTab();
        WebContents webContents = tab == null ? null : tab.getWebContents();
        mConnection.resetPostMessageHandlerForSession(
                mIntentDataProvider.getSession(), webContents);

        if (mDynamicModuleCoordinator != null) {
            mDynamicModuleCoordinator.resetPostMessageHandlersForCurrentSession(null);
        }
    }

    @Override
    public void initializeCompositor() {
        super.initializeCompositor();
        getTabModelSelector().onNativeLibraryReady(getTabContentManager());
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        @TabCreationMode int mode = mTabProvider.getInitialTabCreationMode();
        boolean earlyCreatedTabIsReady =
                (mode == TabCreationMode.HIDDEN || mode == TabCreationMode.EARLY)
                && !mTabProvider.getTab().isLoading();
        if (earlyCreatedTabIsReady) postDeferredStartupIfNeeded();
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        if (getActivityTab() == null) return;
        getActivityTab().loadUrl(new LoadUrlParams(searchUrl));
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
        return new CustomTabAppMenuPropertiesDelegate(this, getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(), getTabModelSelector(), getToolbarManager(),
                getWindow().getDecorView(), getToolbarManager().getBookmarkBridgeSupplier(),
                mIntentDataProvider.getUiType(), mIntentDataProvider.getMenuTitles(),
                mIntentDataProvider.isOpenedByChrome(),
                mIntentDataProvider.shouldShowShareMenuItem(),
                mIntentDataProvider.shouldShowStarButton(),
                mIntentDataProvider.shouldShowDownloadButton(), mIntentDataProvider.isIncognito());
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
    public String getPackageName() {
        if (mShouldOverridePackage) return mIntentDataProvider.getClientPackageName();
        return super.getPackageName();
    }

    @Override
    public void finish() {
        super.finish();
        if (mIntentDataProvider != null && mIntentDataProvider.shouldAnimateOnFinish()) {
            mShouldOverridePackage = true;
            overridePendingTransition(mIntentDataProvider.getAnimationEnterRes(),
                    mIntentDataProvider.getAnimationExitRes());
            mShouldOverridePackage = false;
        } else if (mIntentDataProvider != null && mIntentDataProvider.isOpenedByChrome()) {
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
                ApiCompatibilityUtils.finishAndRemoveTask(this);
            } else {
                finish();
            }
        };
        if (mIntentDataProvider.isTrustedWebActivity()) {
            // TODO(pshmakov): extract all finishing logic from CustomTabActivity.
            // In addition to TwaFinishHandler, create DefaultFinishHandler, PaymentsFinishHandler,
            // and SeparateTaskActivityFinishHandler, all implementing
            // CustomTabActivityNavigationController#FinishHandler. Pass the mode enum into
            // CustomTabActivityModule, so that it can provide the correct implementation.
            getComponent().resolveTwaFinishHandler().onFinish(defaultBehavior);
        } else {
            defaultBehavior.run();
        }
    }

    @Override
    protected boolean handleBackPressed() {
        return mNavigationController.navigateOnBack();
    }

    @Override
    public boolean canShowAppMenu() {
        if (getActivityTab() == null || !mToolbarCoordinator.toolbarIsInitialized()) return false;

        return super.canShowAppMenu();
    }

    @Override
    public boolean onOptionsItemSelected(int itemId, @Nullable Bundle menuItemData) {
        int menuIndex =
                CustomTabAppMenuPropertiesDelegate.getIndexOfMenuItemFromBundle(menuItemData);
        if (menuIndex >= 0) {
            mIntentDataProvider.clickMenuItemWithUrlAndTitle(
                    this, menuIndex, getActivityTab().getUrl(), getActivityTab().getTitle());
            RecordUserAction.record("CustomTabsMenuCustomMenuItem");
            return true;
        }

        return super.onOptionsItemSelected(itemId, menuItemData);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(event, this,
                mToolbarCoordinator.toolbarIsInitialized());
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!mToolbarCoordinator.toolbarIsInitialized()) {
            return super.onKeyDown(keyCode, event);
        }
        return KeyboardShortcuts.onKeyDown(event, this, true, false)
                || super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        // Disable creating new tabs, bookmark, history, print, help, focus_url, etc.
        if (id == R.id.focus_url_bar || id == R.id.all_bookmarks_menu_id
                || id == R.id.help_id || id == R.id.recent_tabs_menu_id
                || id == R.id.new_incognito_tab_menu_id || id == R.id.new_tab_menu_id
                || id == R.id.open_history_menu_id) {
            return true;
        } else if (id == R.id.bookmark_this_page_id) {
            addOrEditBookmark(getActivityTab());
            RecordUserAction.record("MobileMenuAddToBookmarks");
            return true;
        } else if (id == R.id.open_in_browser_id) {
            if (mNavigationController.openCurrentUrlInBrowser(false)) {
                RecordUserAction.record("CustomTabsMenuOpenInChrome");
                mConnection.notifyOpenInBrowser(mSession);
            }
            return true;
        } else if (id == R.id.info_menu_id) {
            if (getTabModelSelector().getCurrentTab() == null) return false;
            PageInfoController.show(this, getTabModelSelector().getCurrentTab(),
                    getToolbarManager().getContentPublisher(),
                    PageInfoController.OpenedFromSource.MENU);
            return true;
        }
        return super.onMenuOrKeyboardAction(id, fromMenu);
    }

    @Override
    public void registerDirectActions() {
        super.registerDirectActions();
        mDirectActionInitializer.allowMenuActions(
                this, getTabModelSelector(), R.id.bookmark_this_page_id, R.id.preferences_id);
    }

    @Override
    public int getActivityThemeColor() {
        if (mIntentDataProvider.isOpenedByChrome()) {
            return TabState.UNSPECIFIED_THEME_COLOR;
        }

        return mIntentDataProvider.getToolbarColor();
    }

    @Override
    public int getBaseStatusBarColor() {
        return mCustomTabStatusBarColorProvider
                .getBaseStatusBarColor(super.getBaseStatusBarColor());
    }

    @Override
    public boolean isStatusBarDefaultThemeColor() {
        return mCustomTabStatusBarColorProvider
                .isStatusBarDefaultThemeColor(super.isStatusBarDefaultThemeColor());
    }

    @Override
    public void onUpdateStateChanged() {}

    /**
     * @return The {@link CustomTabIntentDataProvider} for this {@link CustomTabActivity}.
     */
    @VisibleForTesting
    public CustomTabIntentDataProvider getIntentDataProvider() {
        return mIntentDataProvider;
    }

    @Override
    public boolean supportsAppMenu() {
        // The media viewer has no default menu items, so if there are also no custom items, we
        // should disable the menu altogether.
        if (mIntentDataProvider.isMediaViewer() && mIntentDataProvider.getMenuTitles().isEmpty()) {
            return false;
        }
        return super.supportsAppMenu();
    }

    /**
     * Show the web page with CustomTabActivity, without any navigation control.
     * Used in showing the terms of services page or help pages for Chrome.
     * @param context The current activity context.
     * @param url The url of the web page.
     */
    public static void showInfoPage(Context context, String url) {
        // TODO(xingliu): The title text will be the html document title, figure out if we want to
        // use Chrome strings here as EmbedContentViewActivity does.
        CustomTabsIntent customTabIntent =
                new CustomTabsIntent.Builder()
                        .setShowTitle(true)
                        .setColorScheme(NightModeUtils.isInNightMode(context) ? COLOR_SCHEME_DARK
                                                                              : COLOR_SCHEME_LIGHT)
                        .build();
        customTabIntent.intent.setData(Uri.parse(url));

        Intent intent = LaunchIntentDispatcher.createCustomTabActivityIntent(
                context, customTabIntent.intent);
        intent.setPackage(context.getPackageName());
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_UI_TYPE, CustomTabsUiType.INFO_PAGE);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, context.getPackageName());
        if (!(context instanceof Activity)) intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        IntentHandler.addTrustedIntentExtras(intent);

        context.startActivity(intent);
    }

    @Override
    protected boolean requiresFirstRunToBeCompleted(Intent intent) {
        // Custom Tabs can be used to open Chrome help pages before the ToS has been accepted.
        if (IntentHandler.notSecureIsIntentChromeOrFirstParty(intent)
                && IntentUtils.safeGetIntExtra(intent, CustomTabIntentDataProvider.EXTRA_UI_TYPE,
                           CustomTabIntentDataProvider.CustomTabsUiType.DEFAULT)
                        == CustomTabIntentDataProvider.CustomTabsUiType.INFO_PAGE) {
            return false;
        }

        return super.requiresFirstRunToBeCompleted(intent);
    }

    @Override
    public boolean canShowTrustedCdnPublisherUrl() {
        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.SHOW_TRUSTED_PUBLISHER_URL)) {
            return false;
        }

        Tab tab = mTabProvider.getTab();
        if (tab != null && tab.isPreview()) {
            return false;
        }

        String publisherUrlPackage = mConnection.getTrustedCdnPublisherUrlPackage();
        return publisherUrlPackage != null
                && publisherUrlPackage.equals(mConnection.getClientPackageNameForSession(mSession));
    }

    @Override
    protected CustomTabActivityComponent createComponent(
            ChromeActivityCommonsModule commonsModule) {
        // mIntentHandler comes from the base class.
        IntentIgnoringCriterion intentIgnoringCriterion =
                (intent) -> IntentHandler.shouldIgnoreIntent(intent);

        CustomTabActivityModule customTabsModule = new CustomTabActivityModule(mIntentDataProvider,
                mNightModeStateController, intentIgnoringCriterion, getStartupTabPreloader());
        CustomTabActivityComponent component =
                ChromeApplication.getComponent().createCustomTabActivityComponent(
                        commonsModule, customTabsModule);

        mCustomTabStatusBarColorProvider = component.resolveCustomTabStatusBarColorProvider();
        mTabController = component.resolveTabController();
        mTabProvider = component.resolveTabProvider();
        mTabFactory = component.resolveTabFactory();
        mToolbarCoordinator = component.resolveToolbarCoordinator();
        component.resolveUmaTracker();
        CustomTabActivityClientConnectionKeeper connectionKeeper =
                component.resolveConnectionKeeper();
        mNavigationController = component.resolveNavigationController();
        mNavigationController.setFinishHandler((reason) -> {
            if (reason == USER_NAVIGATION) connectionKeeper.recordClientConnectionStatus();
            handleFinishAndClose();
        });
        mCustomTabIntentHandler = component.resolveIntentHandler();
        component.resolveCompositorContentInitializer();
        component.resolveSessionHandler();
        component.resolveToolbarColorController();
        component.resolveTaskDescriptionHelper();

        if (mIntentDataProvider.isTrustedWebActivity()) {
            component.resolveTrustedWebActivityCoordinator();
        }
        if (mConnection.shouldHideTopBarOnModuleManagedUrlsForSession(
                    mIntentDataProvider.getSession())) {
            component.resolveDynamicModuleToolbarController();
        }

        return component;
    }

    private boolean isAutofillAssistantEnabled() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.AUTOFILL_ASSISTANT)
                && AutofillAssistantFacade.isConfigured(getInitialIntent().getExtras());
    }
}
