// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ShortcutManager;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.text.TextUtils;
import android.text.format.DateUtils;
import android.util.Pair;
import android.view.KeyEvent;
import android.view.KeyboardShortcutGroup;
import android.view.Menu;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleRegistry;

import com.ark.browser.core.utils.NavigationPredictorBridge;

import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.metrics.TabbedActivityLaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.ChromeNextTabPolicySupplier;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabbedModeTabModelOrchestrator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.download.DownloadNotificationService;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoNotificationManager;
import org.chromium.chrome.browser.incognito.IncognitoNotificationPresenceController;
import org.chromium.chrome.browser.incognito.IncognitoProfileDestroyer;
import org.chromium.chrome.browser.incognito.IncognitoStartup;
import org.chromium.chrome.browser.incognito.IncognitoTabLauncher;
import org.chromium.chrome.browser.incognito.IncognitoTabSnapshotController;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.modaldialog.TabModalLifetimeHandler;
import org.chromium.chrome.browser.multiwindow.MultiInstanceChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionController;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionRationaleDialogController;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tab.tab_restore.HistoricalTabModelObserver;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorBase;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import com.ark.browser.translate.TranslateIntentHandler;
import org.chromium.chrome.browser.ui.AppLaunchDrawBlocker;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.embedder_support.util.UrlUtilitiesJni;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.Toast;

import java.util.HashSet;
import java.util.List;

/**
 * This is the main activity for ChromeMobile when not running in document mode.  All the tabs
 * are accessible via a chrome specific tab switching UI.
 */
public class ChromeTabbedActivity extends ChromeActivity
        implements ChromeAccessibilityUtil.Observer {
    private static final String TAG = "ChromeTabbedActivity";

    private static final String HELP_URL_PREFIX = "https://support.google.com/chrome/";

    protected static final String WINDOW_INDEX = "window_index";

    private static final String IS_INCOGNITO_SELECTED = "is_incognito_selected";

    private static final int INVALID_WINDOW_ID = TabWindowManager.INVALID_WINDOW_INDEX;

    // How long to delay closing the current tab when our app is minimized.  Have to delay this
    // so that we don't show the contents of the next tab while minimizing.
    private static final long CLOSE_TAB_ON_MINIMIZE_DELAY_MS = 500;

    // Maximum delay for initial tab creation. This is for homepage and NTP, not previous tabs
    // restore. This is needed because we do not know when reading PartnerBrowserCustomizations
    // provider will be finished.
    private static final int INITIAL_TAB_CREATION_TIMEOUT_MS = 500;

    /**
     * Sending an intent with this action to Chrome will cause it to close all tabs
     * (iff the --enable-test-intents command line flag is set). If a URL is supplied in the
     * intent data, this will be loaded and unaffected by the close all action.
     */
    private static final String ACTION_CLOSE_TABS =
            "com.google.android.apps.chrome.ACTION_CLOSE_TABS";

    @VisibleForTesting
    public static final String STARTUP_UMA_HISTOGRAM_SUFFIX = ".Tabbed";

    // Name of the ChromeTabbedActivity alias that handles MAIN intents.
    public static final String MAIN_LAUNCHER_ACTIVITY_NAME = "com.google.android.apps.chrome.Main";

    public static final HashSet<String> TABBED_MODE_COMPONENT_NAMES = new HashSet<String>() {
        {
            add(ChromeTabbedActivity.class.getName());
            add(MultiInstanceChromeTabbedActivity.class.getName());
            add(ChromeTabbedActivity2.class.getName());
            add(MAIN_LAUNCHER_ACTIVITY_NAME);
        }
    };

    // Count histogram used to track number of tabs when we show the Overview on Return to Chrome.
    private static final String TAB_COUNT_ON_RETURN = "Tabs.TabCountOnStartScreenShown";

    private @Nullable MultiInstanceManager mMultiInstanceManager;

    private LayoutManagerChrome mLayoutManager;

    private ViewGroup mContentContainer;

    private TabbedModeTabModelOrchestrator mTabModelOrchestrator;
    private TabModelSelectorBase mTabModelSelector;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private HistoricalTabModelObserver mHistoricalTabModelObserver;

    private TabModalLifetimeHandler mTabModalHandler;

    private boolean mUIWithNativeInitialized;

    private Boolean mIsAccessibilityTabSwitcherEnabled;

    private LocaleManager mLocaleManager;

    private AppIndexingUtil mAppIndexingUtil;

    private Runnable mShowHistoryRunnable;

    private CompositorViewHolder mCompositorViewHolder;

    /**
     * Keeps track of whether or not a specific tab was created based on the startup intent.
     */
    private boolean mCreatedTabOnStartup;

    // Whether or not the initial tab is being created.
    private boolean mPendingInitialTabCreation;

    // Whether {@link setInitialOverviewState()} has been called within the current onStart/onStop
    // session.
    private boolean mHasDeterminedOverviewStateForCurrentSession;
    /**
     *  Keeps track of the pref for the last time since this activity was stopped.
     */
    private ChromeInactivityTracker mInactivityTracker;

    // This is the cached value of mIntentHandler#shouldIgnoreIntent and shouldn't be read directly.
    // Use #shouldIgnoreIntent instead.
    private Boolean mShouldIgnoreIntent;

    // Time at which an intent was received and handled.
    private long mIntentHandlingTimeMs;

    /**
     * Whether the StartSurface is shown when Chrome is launched.
     */
    private boolean mOverviewShownOnStart;

    private NextTabPolicySupplier mNextTabPolicySupplier;

    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderSupplier =
            new OneshotSupplierImpl<>();
    // TODO(crbug.com/1108496): Removed after all usages has been migrated to LayoutStateProvider.
    private final OneshotSupplierImpl<OverviewModeBehavior> mOverviewModeBehaviorSupplier =
            new OneshotSupplierImpl<>();
    private OverviewModeBehavior mOverviewModeController;

    private CallbackController mCallbackController = new CallbackController();
    private TabbedModeTabDelegateFactory mTabDelegateFactory;

    private final AppLaunchDrawBlocker mAppLaunchDrawBlocker;
    private NotificationPermissionController mNotificationPermissionController;

    // ID assigned to each ChromeTabbedActivity instance in Android S+ where multi-instance feature
    // is supported. This can be explicitly set in the incoming Intent or internally assigned.
    private int mWindowId;

    private final IncognitoTabHost mIncognitoTabHost = new IncognitoTabHost() {
        @Override
        public boolean hasIncognitoTabs() {
            return getTabModelSelector().getModel(true).getCount() > 0;
        }

        @Override
        public void closeAllIncognitoTabs() {
            if (isActivityFinishingOrDestroyed()) return;

            // If the tabbed activity has not yet initialized, then finish the activity to avoid
            // timing issues with clearing the incognito tab state in the background.
            if (!areTabModelsInitialized() || !didFinishNativeInitialization()) {
                finish();
                return;
            }

            getTabModelSelector().getModel(true).closeAllTabs();
        }

        @Override
        public boolean isActiveModel() {
            return getTabModelSelector().getModel(true).isActiveModel();
        }
    };

    /**
     * This class is used to warm up the chrome split ClassLoader. See SplitChromeApplication for
     * more info
     */
    @UsedByReflection("SplitChromeApplication.java")
    public static class Preload extends ChromeTabbedActivity {
        private LifecycleRegistry mLifecycleRegistry;

        @UsedByReflection("SplitChromeApplication.java")
        public Preload() {}

        @Override
        public Lifecycle getLifecycle() {
            if (mLifecycleRegistry == null) {
                // LifecycleRegistry normally enforces it is called on the main thread, but this
                // class will be preloaded in a background thread. The only method that gets called
                // in the activity constructor is addObserver(), so just override that.
                mLifecycleRegistry = new LifecycleRegistry(null) {
                    @Override
                    public void addObserver(LifecycleObserver observer) {}
                };
            }
            return mLifecycleRegistry;
        }
    }

    /**
     * Specify the proper non-.Main-aliased Chrome Activity for the given component.
     *
     * @param intent The intent to set the component for.
     * @param component The client generated component to be validated.
     */
    public static void setNonAliasedComponent(Intent intent, ComponentName component) {
        assert component != null;
        Context appContext = ContextUtils.getApplicationContext();
        if (!TextUtils.equals(component.getPackageName(), appContext.getPackageName())) {
            return;
        }
        if (component.getClassName() != null
                && TextUtils.equals(component.getClassName(),
                        ChromeTabbedActivity.MAIN_LAUNCHER_ACTIVITY_NAME)) {
            // Keep in sync with the activities that the .Main alias points to in
            // AndroidManifest.xml.
            intent.setClass(appContext, ChromeTabbedActivity.class);
        } else {
            intent.setComponent(component);
        }
    }

    /**
     * Constructs a ChromeTabbedActivity.
     */
    public ChromeTabbedActivity() {

        // AppLaunchDrawBlocker may block drawing the Activity content until the initial tab is
        // available.
        // clang-format off
        mAppLaunchDrawBlocker = new AppLaunchDrawBlocker(getLifecycleDispatcher(),
                () -> findViewById(android.R.id.content),
                this::getIntent, this::shouldIgnoreIntent, this::isTablet,
                this::shouldShowOverviewPageOnStart);
        // clang-format on
    }

    @Override
    protected void onPreCreate() {
        super.onPreCreate();
        mMultiInstanceManager = MultiInstanceManager.create(this, getTabModelOrchestratorSupplier(),
                getMultiWindowModeStateDispatcher(), getLifecycleDispatcher(),
                getModalDialogManagerSupplier(), this);
    }

    @Override
    protected @LaunchIntentDispatcher.Action int maybeDispatchLaunchIntent(
            Intent intent, Bundle savedInstanceState) {
        // Detect if incoming intent is a result of Chrome recreating itself. For now, restrict this
        // path to reparenting to ensure the launching logic isn't disrupted.
        // TODO(crbug.com/1065491): Unlock this codepath for all incoming intents once it's
        // confirmed working and stable.
        if (savedInstanceState != null
                && AsyncTabParamsManagerSingleton.getInstance().hasParamsWithTabToReparent()) {
            return LaunchIntentDispatcher.Action.CONTINUE;
        }

        if (getClass().equals(ChromeTabbedActivity.class)
                && Intent.ACTION_MAIN.equals(intent.getAction())) {
            // Call dispatchToTabbedActivity() for MAIN intents to activate proper multi-window
            // TabbedActivity (i.e. if CTA2 is currently running and Chrome is started, CTA2
            // should be brought to front). Don't call dispatchToTabbedActivity() for non-MAIN
            // intents to avoid breaking cases where CTA is started explicitly (e.g. to handle
            // 'Move to other window' command from CTA2).
            return LaunchIntentDispatcher.dispatchToTabbedActivity(this, intent);
        }
        return super.maybeDispatchLaunchIntent(intent, savedInstanceState);
    }

    @Override
    public void initializeCompositor() {
        try {
            TraceEvent.begin("ChromeTabbedActivity.initializeCompositor");
            super.initializeCompositor();

            // LocaleManager can only function after the native library is loaded.
            mLocaleManager = LocaleManager.getInstance();

            mTabModelOrchestrator.onNativeLibraryReady(getTabContentManager());

            // For saving non-incognito tab closures for Recent Tabs.
            mHistoricalTabModelObserver =
                    new HistoricalTabModelObserver(mTabModelSelector.getModel(false));

            mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelector) {
                @Override
                public void didCloseTab(Tab tab) {
                    closeIfNoTabsAndHomepageEnabled(false);
                }

                @Override
                public void tabPendingClosure(Tab tab) {
                    closeIfNoTabsAndHomepageEnabled(true);
                }

                @Override
                public void tabRemoved(Tab tab) {
                    closeIfNoTabsAndHomepageEnabled(false);
                }

                private void closeIfNoTabsAndHomepageEnabled(boolean isPendingClosure) {

                    boolean gridTabSwitcherEnabled = true;
                    boolean overviewVisible = mOverviewModeController.overviewVisible();
                    boolean hasNextTab = !(getTabModelSelector().getTotalTabCount() == 0
                            || (!getTabModelSelector().isIncognitoSelected()
                                    && getTabModelSelector().getModel(false).getCount() == 0));

                    boolean multiWindowActive =
                            MultiWindowUtils.getInstance().areMultipleChromeInstancesRunning(
                                    ChromeTabbedActivity.this)
                            && MultiWindowUtils.getVisibleTabbedTaskCount() > 1;
                    boolean tabletGtsPolish = false;
                    boolean useAccessibilityListSwitcher =
                            DeviceClassManager.enableAccessibilityLayout(ChromeTabbedActivity.this);

                    // TODO(crbug.com/1046541) : Remove this when the associated bug is fixed. This
                    //  is a band-aid fix for TabGrid and closing tabs with TabGroupUi.
                    //  If one of the following is true, then exit Chrome when TabGroupsAndroid is
                    //  enabled, and tab switcher is not shown:
                    //   1. If the very last tab is closed.
                    //   2. If normal tab model is selected and no normal tabs.
                    if (gridTabSwitcherEnabled && !overviewVisible && !hasNextTab && !isTablet()
                            && !multiWindowActive) {
                        finish();
                    }

                    // TODO(crbug.com/1272821): Investigate unexpected behavior when the last tab is
                    //  closed through grid tab switcher without this logic.
                    //  If one of the following is true, then hide the grid tab switcher for tablets
                    //  when TabGroupsAndroid is enabled, and tab switcher is shown:
                    //   1. If the very last tab is closed.
                    //   2. If normal tab model is selected and no normal tabs.
                    if (gridTabSwitcherEnabled && overviewVisible && !hasNextTab && isTablet()
                            && !tabletGtsPolish && !useAccessibilityListSwitcher) {
                        mLayoutManager.showLayout(LayoutType.BROWSING, true);
                    }
                }

                @Override
                public void didAddTab(
                        Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                    if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                            || type == TabLaunchType.FROM_RECENT_TABS
                                    && !DeviceClassManager.enableAnimations()) {
                        Toast.makeText(ChromeTabbedActivity.this, R.string.open_in_new_tab_toast,
                                     Toast.LENGTH_SHORT)
                                .show();
                    }
                }

                @Override
                public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                }
            };
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeCompositor");
        }
    }

    private void setupCompositorContentPreNativeForPhone() {
        if (isTablet()) return;

        try (TraceEvent e = TraceEvent.scoped(
                     "ChromeTabbedActivity.setupCompositorContentPreNativeForPhone")) {
            CompositorViewHolder compositorViewHolder = getCompositorViewHolderSupplier().get();

            // clang-format off
            mLayoutManager = new LayoutManagerChromePhone(compositorViewHolder, mContentContainer,
                    getTabContentManagerSupplier(),
                    mOverviewModeBehaviorSupplier);
            mLayoutStateProviderSupplier.set(mLayoutManager);
            // clang-format on
            mOverviewModeController = mLayoutManager;
        }
    }

    private void setupCompositorContentPostNative() {
        try (TraceEvent e = TraceEvent.scoped(
                     "ChromeTabbedActivity.setupCompositorContentPostNative")) {
            if (!isLayoutManagerCreated()) {
                setupCompositorContentPreNativeForPhone();
            }

            mLayoutManager.setEnableAnimations(DeviceClassManager.enableAnimations());

            initializeCompositorContent(mLayoutManager,
                    mContentContainer);
        }
    }

    private boolean isLayoutManagerCreated() {
        return mLayoutManager != null;
    }

    private void maybeCreateIncognitoTabSnapshotController() {
        try (TraceEvent e = TraceEvent.scoped(
                     "ChromeTabbedActivity.maybeCreateIncognitoTabSnapshotController")) {
            if (!CommandLine.getInstance().hasSwitch(
                        ChromeSwitches.ENABLE_INCOGNITO_SNAPSHOTS_IN_ANDROID_RECENTS)) {
                IncognitoTabSnapshotController.createIncognitoTabSnapshotController(
                        this, getWindow(), mLayoutManager, mTabModelSelector);
            }

            mUIWithNativeInitialized = true;
            onAccessibilityTabSwitcherModeChanged();

            // The dataset has already been created, we need to initialize our state.
            mTabModelSelector.notifyChanged();

            // Check for incognito tabs to handle the case where Chrome was swiped away in the
            // background.
            if (!IncognitoTabHostUtils.doIncognitoTabsExist()) {
                IncognitoNotificationManager.dismissIncognitoNotification();
                DownloadNotificationService.getInstance().cancelOffTheRecordDownloads();
            }
        }
    }

    @Override
    public void onNewIntent(Intent intent) {

        Intent intentForDispatching = new Intent(intent);
        intentForDispatching.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);

        mIntentHandlingTimeMs = SystemClock.uptimeMillis();
        super.onNewIntent(intent);
    }

    @Override
    public void startNativeInitialization() {
        try (TraceEvent e = TraceEvent.scoped("ChromeTabbedActivity.startNativeInitialization")) {
            // This is on the critical path so don't delay.
            setupCompositorContentPostNative();

            // All this initialization can be expensive so it's split into multiple tasks.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    mCallbackController.makeCancelable(
                            this::maybeCreateIncognitoTabSnapshotController));
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    mCallbackController.makeCancelable(
                            this::onAccessibilityTabSwitcherModeChanged));
            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    mCallbackController.makeCancelable(this::finishNativeInitialization));
            ChromeAccessibilityUtil.get().addObserver(this);
        }
    }

    @Override
    public void finishNativeInitialization() {
        try (TraceEvent te = TraceEvent.scoped("ChromeTabbedActivity.finishNativeInitialization")) {
            super.finishNativeInitialization();

            // TODO(jinsukkim): Let these classes handle the registration by themselves.
            mCompositorViewHolder = getCompositorViewHolderSupplier().get();

            ChromeAccessibilityUtil.get().addObserver(mLayoutManager);
            if (isTablet()) ChromeAccessibilityUtil.get().addObserver(mCompositorViewHolder);

            mNotificationPermissionController =
                    new NotificationPermissionController(getWindowAndroid(),
                            new NotificationPermissionRationaleDialogController(
                                    this, getModalDialogManager()));
            NotificationPermissionController.attach(
                    getWindowAndroid(), mNotificationPermissionController);
            mNotificationPermissionController.requestPermissionIfNeeded(false /* contextual */);
        }
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();

        IncognitoStartup.onResumeWithNative(
                getTabModelSelectorSupplier(), TABBED_MODE_COMPONENT_NAMES);

        mLocaleManager.setSnackbarManager(getSnackbarManager());
        mLocaleManager.startObservingPhoneChanges();

        if (isWarmOnResume()) {
            NavigationPredictorBridge.onActivityWarmResumed();
        } else {
            NavigationPredictorBridge.onColdStart();
        }
    }

    @Override
    public void onPauseWithNative() {
        mTabModelSelector.commitAllTabClosures();
        CookiesFetcher.persistCookies();

        mLocaleManager.setSnackbarManager(null);
        mLocaleManager.stopObservingPhoneChanges();

        NavigationPredictorBridge.onPause();

        super.onPauseWithNative();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();

        saveState();
        mHasDeterminedOverviewStateForCurrentSession = false;
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();

        // Don't call setInitialOverviewState if we're waiting for the tab's creation or we risk
        // showing a glimpse of the tab selector during start up.
        if (!mPendingInitialTabCreation) {
            setInitialOverviewState();
        }

        resetSavedInstanceState();
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        try {
            TraceEvent.begin("ChromeTabbedActivity.onNewIntentWithNative");

            super.onNewIntentWithNative(intent);

            if (CommandLine.getInstance().hasSwitch(ContentSwitches.ENABLE_TEST_INTENTS)) {
                handleDebugIntent(intent);
            }

        } finally {
            TraceEvent.end("ChromeTabbedActivity.onNewIntentWithNative");
        }
    }

    @Override
    public @ActivityType int getActivityType() {
        return ActivityType.TABBED;
    }

    @Override
    public ChromeTabCreator getTabCreator(boolean incognito) {
        return (ChromeTabCreator) super.getTabCreator(incognito);
    }

    @Override
    public ChromeTabCreator getCurrentTabCreator() {
        return (ChromeTabCreator) super.getCurrentTabCreator();
    }

    @Override
    public OverviewModeBehavior getOverviewModeBehavior() {
        return mOverviewModeController;
    }

    private void handleDebugIntent(Intent intent) {
        if (ACTION_CLOSE_TABS.equals(intent.getAction())) {
            getTabModelSelector().closeAllTabs();
        } else if (MemoryPressureListener.handleDebugIntent(
                           ChromeTabbedActivity.this, intent.getAction())) {
            // Handled.
        }
    }

    private void setInitialOverviewState() {
        if (mHasDeterminedOverviewStateForCurrentSession) return;

        mHasDeterminedOverviewStateForCurrentSession = true;
        boolean isOverviewVisible = mOverviewModeController.overviewVisible();

        if (shouldRefreshAndShowOverview(isOverviewVisible)) {
            if (getCurrentTabModel() != null) {
                RecordHistogram.recordCount1MHistogram(
                        TAB_COUNT_ON_RETURN, getCurrentTabModel().getCount());
            }
            mOverviewShownOnStart = true;
            showOverview();
            mAppLaunchDrawBlocker.onOverviewPageAvailable(mOverviewShownOnStart);
            return;
        }

        if (getActivityTab() == null && !isOverviewVisible) {
            mOverviewShownOnStart = true;
            showOverview();
        }

        if (IntentUtils.isMainIntentFromLauncher(getIntent())
                && mOverviewModeController.overviewVisible()) {
            RecordUserAction.record("MobileStartup.UserEnteredTabSwitcher");
        }
        mAppLaunchDrawBlocker.onOverviewPageAvailable(mOverviewShownOnStart);
    }

    private boolean shouldRefreshAndShowOverview(boolean isOverviewVisible) {
        // If StartSurfaceConfiguration.NEW_SURFACE_FROM_HOME_BUTTON is turned on, MV tiles and
        // carousels may be hidden before Chrome is brought to the background. If overview should be
        // shown, no matter overview was already visible or not, we should call
        // showOverview(StartSurfaceState.SHOWING_START) to show MV tiles and carousels again.
        return shouldShowOverviewPageOnStart()
                && !isOverviewVisible;
    }

    @VisibleForTesting
    public ChromeInactivityTracker getInactivityTrackerForTesting() {
        return mInactivityTracker;
    }

    @Override
    public void initializeState() {
        // This method goes through 3 steps:
        // 1. Load the saved tab state (but don't start restoring the tabs yet).
        // 2. Process the Intent that this activity received and if that should result in any
        //    new tabs, create them.  This is done after step 1 so that the new tab gets
        //    created after previous tab state was restored.
        // 3. If no tabs were created in any of the above steps, create an NTP, otherwise
        //    start asynchronous tab restore (loading the previously active tab synchronously
        //    if no new tabs created in step 2).

        // Only look at the original intent if this is not a "restoration" and we are allowed to
        // process intents. Any subsequent intents are carried through onNewIntent.
        try {
            TraceEvent.begin("ChromeTabbedActivity.initializeState");

            super.initializeState();
            Log.i(TAG, "#initializeState");
            Intent intent = getIntent();

            boolean hadCipherData =
                    CipherFactory.getInstance().restoreFromBundle(getSavedInstanceState());

            boolean noRestoreState =
                    CommandLine.getInstance().hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
            if (noRestoreState) {
                // Clear the state files because they are inconsistent and useless from now on.
                mTabModelOrchestrator.clearState();
            } else {
                // State should be clear when we start first run and hence we do not need to load
                // a previous state. This may change the current Model, watch out for initialization
                // based on the model.
                // Never attempt to restore incognito tabs when this activity was previously swiped
                // away in Recents. http://crbug.com/626629
                boolean ignoreIncognitoFiles = !hadCipherData;
                mTabModelOrchestrator.loadState(ignoreIncognitoFiles);
            }

            mInactivityTracker.register(this.getLifecycleDispatcher());
            boolean isIntentWithEffect = false;
            boolean isMainIntentFromLauncher = false;
            if (getSavedInstanceState() == null && intent != null) {
                if (!shouldIgnoreIntent()) {
                    isIntentWithEffect = mIntentHandler.onNewIntent(intent);
                }

                if (IntentUtils.isMainIntentFromLauncher(intent)) {
                    isMainIntentFromLauncher = true;
                }
            }

            // If we have tabs to reparent and getSavedInstanceState() is non-null, then the tabs
            // are coming from night mode tab reparenting. In this case, reparenting happens
            // synchronously along with tab restoration so there are no tabs waiting for
            // reparenting like there are for other tab reparenting operations.
            boolean hasTabWaitingForReparenting =
                    AsyncTabParamsManagerSingleton.getInstance().hasParamsWithTabToReparent()
                    && getSavedInstanceState() == null;
            mCreatedTabOnStartup = getCurrentTabModel().getCount() > 0
                    || mTabModelOrchestrator.getRestoredTabCount() > 0 || isIntentWithEffect
                    || hasTabWaitingForReparenting;

            // We always need to try to restore tabs. The set of tabs might be empty, but at least
            // it will trigger the notification that tab restore is complete which is needed by
            // other parts of Chrome such as sync.
            boolean activeTabBeingRestored = !isIntentWithEffect
                    || (shouldShowOverviewPageOnStart()
                            && !mTabModelSelector.isIncognitoSelected());

            mTabModelOrchestrator.restoreTabs(activeTabBeingRestored);

            // Only create an initial tab if no tabs were restored and no intent was handled.
            // Also, check whether the active tab was supposed to be restored and that the total
            // tab count is now non zero.  If this is not the case, tab restore failed and we need
            // to create a new tab as well.
            if (!mCreatedTabOnStartup
                    || (!hasTabWaitingForReparenting && activeTabBeingRestored
                            && getTabModelSelector().getTotalTabCount() == 0)) {
                // If homepage URI is not determined, due to PartnerBrowserCustomizations provider
                // async reading, then create a tab at the async reading finished. If it takes
                // too long, just create NTP.

                mPendingInitialTabCreation = true;
                PartnerBrowserCustomizations.getInstance().setOnInitializeAsyncFinished(() -> {
                    if (!isActivityFinishingOrDestroyed()) {
                        createInitialTab();
                    }
                }, INITIAL_TAB_CREATION_TIMEOUT_MS);
            }

            // If initial tab creation is pending, this will instead be handled when we create the
            // initial tab in #createInitialTab.
            if (!mPendingInitialTabCreation) {
                mAppLaunchDrawBlocker.onActiveTabAvailable(isTabRegularNtp(getActivityTab()));
            }
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeState");
        }
    }

    private boolean hasStartWithNativeBeenCalled() {
        int activity_state = getLifecycleDispatcher().getCurrentActivityState();
        return activity_state == ActivityLifecycleDispatcher.ActivityState.STARTED_WITH_NATIVE
                || activity_state == ActivityLifecycleDispatcher.ActivityState.RESUMED_WITH_NATIVE;
    }

    /**
     * Create an initial tab for cold start without restored tabs.
     */
    private void createInitialTab() {
        Log.i(TAG, "#createInitialTab executed.");
        mPendingInitialTabCreation = false;

        // If the start surface or grid tab switcher will be shown on start, do not create a new
        // tab.
        if (!shouldShowOverviewPageOnStart()) {
            String url = "https://www.baidu.com";

            getTabCreator(false).launchUrl(url, TabLaunchType.FROM_STARTUP);
        }

        // If we didn't call setInitialOverviewState() in onStartWithNative() because
        // mPendingInitialTabCreation was true then do so now.
        if (hasStartWithNativeBeenCalled()) {
            setInitialOverviewState();
        }

        mAppLaunchDrawBlocker.onActiveTabAvailable(isTabRegularNtp(getActivityTab()));
    }

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        onAccessibilityTabSwitcherModeChanged();
    }

    private void onAccessibilityTabSwitcherModeChanged() {
        if (!mUIWithNativeInitialized) return;

        boolean accessibilityTabSwitcherEnabled =
                DeviceClassManager.enableAccessibilityLayout(this);
        if (mOverviewModeController != null && mOverviewModeController.overviewVisible()
                && (mIsAccessibilityTabSwitcherEnabled == null
                        || mIsAccessibilityTabSwitcherEnabled
                                != DeviceClassManager.enableAccessibilityLayout(this))) {

            mLayoutManager.showLayout(LayoutType.BROWSING, true);
            if (getTabModelSelector().getCurrentModel().getCount() == 0) {
                getCurrentTabCreator().launchNTP();
            }
        }
        mIsAccessibilityTabSwitcherEnabled = accessibilityTabSwitcherEnabled;

        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
            RecordHistogram.recordBooleanHistogram(
                    "Accessibility.Android.TabSwitcherPreferenceEnabled",
                    mIsAccessibilityTabSwitcherEnabled);
        }
    }

    /**
     * Internal class which performs the intent handling operations delegated by IntentHandler.
     */
    private class InternalIntentDelegate implements IntentHandler.IntentHandlerDelegate {
        /**
         * Processes a url view intent.
         *
         * @param url The url from the intent.
         */
        @Override
        public void processUrlViewIntent(LoadUrlParams loadUrlParams, @TabOpenType int tabOpenType,
                String externalAppId, int tabIdToBringToFront, Intent intent) {
            if (isActivityFinishingOrDestroyed()) {
                return;
            }
            if (isFromChrome(intent, externalAppId)) {
                RecordUserAction.record("MobileTabbedModeViewIntentFromChrome");
            } else {
                RecordUserAction.record("MobileTabbedModeViewIntentFromApp");
            }

            final String url = loadUrlParams.getUrl();
            boolean fromLauncherShortcut = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false);
            boolean fromAppWidget = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, false);
            boolean focus = false;

            TabModel tabModel = getCurrentTabModel();
            switch (tabOpenType) {
                case TabOpenType.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB:
                    mTabModelOrchestrator.tryToRestoreTabStateForUrl(url);
                    int tabToBeClobberedIndex = TabModelUtils.getTabIndexByUrl(tabModel, url);
                    Tab tabToBeClobbered = tabModel.getTabAt(tabToBeClobberedIndex);
                    if (tabToBeClobbered != null) {
                        TabModelUtils.setIndex(tabModel, tabToBeClobberedIndex, false);
                        tabToBeClobbered.reload();
                    } else {
                        launchIntent(loadUrlParams, externalAppId, true, intent);
                    }
                    if (fromAppWidget && url.startsWith(UrlConstants.CHROME_DINO_URL)) {
                        RecordUserAction.record("QuickActionSearchWidget.StartDinoGame");
                    }
                    break;
                case TabOpenType.BRING_TAB_TO_FRONT:
                    mTabModelOrchestrator.tryToRestoreTabStateForId(tabIdToBringToFront);

                    int tabIndex = TabModelUtils.getTabIndexById(tabModel, tabIdToBringToFront);
                    if (tabIndex == TabModel.INVALID_TAB_INDEX) {
                        TabModel otherModel =
                                getTabModelSelector().getModel(!tabModel.isIncognito());
                        tabIndex = TabModelUtils.getTabIndexById(otherModel, tabIdToBringToFront);
                        if (tabIndex != TabModel.INVALID_TAB_INDEX) {
                            getTabModelSelector().selectModel(otherModel.isIncognito());
                            TabModelUtils.setIndex(otherModel, tabIndex, false);
                        } else {
                            Log.e(TAG, "Failed to bring tab to front because it doesn't exist.");
                            return;
                        }
                    } else {
                        TabModelUtils.setIndex(tabModel, tabIndex, false);
                    }
                    break;
                case TabOpenType.CLOBBER_CURRENT_TAB:
                    // The browser triggered the intent. This happens when clicking links which
                    // can be handled by other applications (e.g. www.youtube.com links).
                    Tab currentTab = getActivityTab();
                    if (currentTab != null) {
                        RedirectHandlerTabHelper.updateIntentInTab(currentTab, intent);
                        currentTab.loadUrl(loadUrlParams);
                    } else {
                        launchIntent(loadUrlParams, externalAppId, true, intent);
                    }
                    break;
                case TabOpenType.REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB:
                    launchIntent(loadUrlParams, externalAppId, false, intent);
                    break;
                case TabOpenType.REUSE_TAB_MATCHING_ID_ELSE_NEW_TAB:
                    int tabId = IntentUtils.safeGetIntExtra(
                            intent, TabOpenType.REUSE_TAB_MATCHING_ID_STRING, Tab.INVALID_TAB_ID);
                    if (tabId != Tab.INVALID_TAB_ID) {
                        mTabModelOrchestrator.tryToRestoreTabStateForId(tabId);
                        int matchingTabIndex = TabModelUtils.getTabIndexById(tabModel, tabId);
                        boolean loaded = false;
                        if (matchingTabIndex != TabModel.INVALID_TAB_INDEX) {
                            Tab tab = tabModel.getTabAt(matchingTabIndex);
                            if (tab.getUrl().getSpec().equals(url)
                                    || tab.getUrl().getSpec().equals(IntentUtils.safeGetStringExtra(
                                            intent, TabOpenType.REUSE_TAB_ORIGINAL_URL_STRING))) {
                                tabModel.setIndex(
                                        matchingTabIndex, TabSelectionType.FROM_USER, false);
                                tab.loadUrl(loadUrlParams);
                                loaded = true;
                            }
                        }
                        if (!loaded) {
                            launchIntent(loadUrlParams, externalAppId, false, intent);
                        }
                    }
                    break;
                case TabOpenType.OPEN_NEW_TAB:
                    if (fromLauncherShortcut) {
                        recordLauncherShortcutAction(false);
                        reportNewTabShortcutUsed(false);
                    }

                    launchIntent(loadUrlParams, externalAppId, true, intent);
                    break;
                case TabOpenType.OPEN_NEW_INCOGNITO_TAB:
                    if (!TextUtils.equals(externalAppId, getPackageName())) {
                        assert false : "Only Chrome is allowed to open incognito tabs";
                        Log.e(TAG, "Only Chrome is allowed to open incognito tabs");
                        return;
                    }

                    if (url == null || url.equals(UrlConstants.NTP_URL)) {
                        if (fromLauncherShortcut) {
                            getTabCreator(true).launchUrl(
                                    UrlConstants.NTP_URL, TabLaunchType.FROM_LAUNCHER_SHORTCUT);
                            recordLauncherShortcutAction(true);
                            reportNewTabShortcutUsed(true);
                        } else if (fromAppWidget) {
                            RecordUserAction.record("QuickActionSearchWidget.StartIncognito");
                            getTabCreator(true).launchUrl(
                                    UrlConstants.NTP_URL, TabLaunchType.FROM_APP_WIDGET);
                        } else if (IncognitoTabLauncher.didCreateIntent(intent)) {
                            IncognitoTabLauncher.recordUse();
                        } else {
                            // Used by the Account management screen to open a new incognito tab.
                            // Account management screen collects its metrics separately.
                            getTabCreator(true).launchUrl(UrlConstants.NTP_URL,
                                    TabLaunchType.FROM_CHROME_UI, intent, mIntentHandlingTimeMs);
                        }
                    } else {
                        @TabLaunchType
                        Integer launchType = IntentHandler.getTabLaunchType(intent);
                        if (launchType == null) launchType = TabLaunchType.FROM_LINK;
                        getTabCreator(true).launchUrl(
                                url, launchType, intent, mIntentHandlingTimeMs);
                    }
                    break;
                default:
                    assert false : "Unknown TabOpenType: " + tabOpenType;
                    break;
            }

            if (tabModel.getCount() > 0 && isInOverviewMode() && !isTablet()
                    && !shouldShowOverviewPageOnStart()) {
                mLayoutManager.showLayout(LayoutType.BROWSING, true);
            }
        }

        @Override
        public long getIntentHandlingTimeMs() {
            return mIntentHandlingTimeMs;
        }

        @Override
        public void processWebSearchIntent(String query) {
            assert false;
        }

        @Override
        public void processTranslateTabIntent(
                @Nullable String targetLanguageCode, @Nullable String expectedUrl) {
            TranslateIntentHandler.translateTab(getActivityTab(), targetLanguageCode, expectedUrl);
        }

        private boolean isFromChrome(Intent intent, String externalAppId) {
            // To determine if the processed intent is from Chrome, check for any of the following:
            // 1.) The authentication token that will be added to trusted intents.
            // 2.) The app ID matches Chrome.  This value can be spoofed by other applications, but
            //     in cases where we were not able to add the authentication token this is our only
            //     indication the intent was from Chrome.
            return IntentHandler.wasIntentSenderChrome(intent)
                    || TextUtils.equals(externalAppId, getPackageName());
        }
    }

    @Override
    public void performPreInflationStartup() {

        super.performPreInflationStartup();

        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);

        IncognitoTabHostRegistry.getInstance().register(mIncognitoTabHost);
    }

    @Override
    public void performPostInflationStartup() {
        super.performPostInflationStartup();

        TabModelSelector tabModelSelector = getTabModelSelector();
        IncognitoProfileDestroyer.observeTabModelSelector(tabModelSelector);
        IncognitoNotificationPresenceController.observeTabModelSelector(tabModelSelector);

        // Critical path for startup. Create the minimum objects needed
        // to allow a blank screen draw (without depending on any native code)
        // and then yield ASAP.
        if (isFinishing()) return;

        // Don't show the keyboard until user clicks in.
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        mContentContainer = (ViewGroup) findViewById(android.R.id.content);

        Supplier<Boolean> dialogVisibilitySupplier = () -> {
            return false;
        };

        mInactivityTracker = new ChromeInactivityTracker(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF);
        TabUsageTracker.initialize(this.getLifecycleDispatcher(), tabModelSelector);
    }

    private boolean shouldIgnoreIntent() {
        if (mShouldIgnoreIntent == null) {
            // We call this only once because mIntentHandler#shouldIgnoreIntent has side effects.
            mShouldIgnoreIntent =
                    mIntentHandler.shouldIgnoreIntent(getIntent(), /*startedActivity=*/true);
        }
        return mShouldIgnoreIntent;
    }

    @Override
    protected final void dispatchOnInflationComplete() {
        super.dispatchOnInflationComplete();
    }

    @Override
    protected TabModelOrchestrator createTabModelOrchestrator() {
        boolean tabMergingEnabled =
                mMultiInstanceManager != null && mMultiInstanceManager.isTabModelMergingEnabled();
        mTabModelOrchestrator = new TabbedModeTabModelOrchestrator(tabMergingEnabled);
        return mTabModelOrchestrator;
    }

    @Override
    protected void createTabModels() {
        assert mTabModelSelector == null;
        assert mWindowId != INVALID_WINDOW_ID;

        Bundle savedInstanceState = getSavedInstanceState();

        // We determine the model as soon as possible so every systems get initialized coherently.
        boolean startIncognito = savedInstanceState != null
                && savedInstanceState.getBoolean(IS_INCOGNITO_SELECTED, false);

        mNextTabPolicySupplier = new ChromeNextTabPolicySupplier(mLayoutStateProviderSupplier);

        boolean tabModelWasCreated = mTabModelOrchestrator.createTabModels(
                this, this, mNextTabPolicySupplier, mWindowId);
        if (!tabModelWasCreated) {
            finish();
            return;
        }

        if (mMultiInstanceManager != null) {
            int assignedIndex = TabWindowManagerSingleton.getInstance().getIndexForWindow(this);
            // The given index and the one computed by TabWindowManager should be one and the same.
            assert !MultiWindowUtils.isMultiInstanceApi31Enabled() || assignedIndex == mWindowId;
            mMultiInstanceManager.initialize(assignedIndex, getTaskId());
        }

        mTabModelSelector = mTabModelOrchestrator.getTabModelSelector();

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelector) {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {

            }
        };
        mAppIndexingUtil = new AppIndexingUtil(mTabModelSelector);

        if (startIncognito) mTabModelSelector.selectModel(true);
    }

    @Override
    protected LaunchCauseMetrics createLaunchCauseMetrics() {
        return new TabbedActivityLaunchCauseMetrics(this);
    }

    private TabDelegateFactory getTabDelegateFactory() {
        if (mTabDelegateFactory == null) {
            mTabDelegateFactory = new TabbedModeTabDelegateFactory(this,
                    /* ChromeActivityNativeDelegate */ this,
                    getBrowserControlsManager(), getFullscreenManager(),
                    /* TabCreatorManager */ this, getTabModelSelectorSupplier(),
                    getCompositorViewHolderSupplier(), getModalDialogManagerSupplier(),
                    this::getSnackbarManager);
        }
        return mTabDelegateFactory;
    }

    @Override
    protected Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        return Pair.create(
                new ChromeTabCreator(this, getWindowAndroid(), getStartupTabPreloader(),
                        this::getTabDelegateFactory, false,
                        AsyncTabParamsManagerSingleton.getInstance(), getTabModelSelectorSupplier(),
                        getCompositorViewHolderSupplier()),
                new ChromeTabCreator(this, getWindowAndroid(), getStartupTabPreloader(),
                        this::getTabDelegateFactory, true,
                        AsyncTabParamsManagerSingleton.getInstance(), getTabModelSelectorSupplier(),
                        getCompositorViewHolderSupplier()));
    }

    @Override
    protected void initDeferredStartupForActivity() {
        super.initDeferredStartupForActivity();
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            ActivityManager am = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
            RecordHistogram.recordSparseHistogram(
                    "MemoryAndroid.DeviceMemoryClass", am.getMemoryClass());

            LauncherShortcutActivity.updateIncognitoShortcut(ChromeTabbedActivity.this);
        });
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);
        RecordHistogram.recordCustomTimesHistogram("MobileStartup.IntentToCreationTime.TabbedMode",
                timeMs, 1, DateUtils.SECOND_IN_MILLIS * 30, 50);
    }

    @Override
    protected boolean isStartedUpCorrectly(Intent intent) {
        mWindowId = 0;
        Bundle savedInstanceState = getSavedInstanceState();
        int windowId = getExtraWindowIdFromIntent(intent);
        if (savedInstanceState != null && savedInstanceState.containsKey(WINDOW_INDEX)) {
            // Activity is recreated after destruction. |windowId| must not be valid in this case.
            assert windowId == INVALID_WINDOW_ID;
            mWindowId = savedInstanceState.getInt(WINDOW_INDEX, 0);
        } else if (mMultiInstanceManager != null) {
            // |allocInstanceId| doesn't do any disk I/O that would add a long-running task
            // to pre-inflation startup.
            boolean preferNew = getExtraPreferNewFromIntent(intent);
            mWindowId = mMultiInstanceManager.allocInstanceId(windowId, getTaskId(), preferNew);
        }
        if (mWindowId == INVALID_WINDOW_ID) {
            Log.i(TAG, "Window ID not allocated. Finishing the activity");
            Toast.makeText(this, R.string.max_number_of_windows, Toast.LENGTH_LONG).show();
            return false;
        }

        if (mMultiInstanceManager != null
                && !mMultiInstanceManager.isStartedUpCorrectly(getTaskId())) {
            return false;
        }

        return super.isStartedUpCorrectly(intent);
    }

    private static int getExtraWindowIdFromIntent(Intent intent) {
        int windowId = IntentUtils.safeGetIntExtra(
                intent, IntentHandler.EXTRA_WINDOW_ID, INVALID_WINDOW_ID);
        return IntentUtils.isTrustedIntentFromSelf(intent) ? windowId : INVALID_WINDOW_ID;
    }

    private static boolean getExtraPreferNewFromIntent(Intent intent) {
        return IntentUtils.safeGetBooleanExtra(intent, IntentHandler.EXTRA_PREFER_NEW, false);
    }

    @Override
    public void terminateIncognitoSession() {
        getTabModelSelector().getModel(true).closeAllTabs();
    }

    @Override
    public boolean onMenuOrKeyboardAction(final int id, boolean fromMenu) {
        return true;
    }

    private boolean isTabNtp(Tab tab) {
        return tab != null && UrlUtilities.isNTPUrl(tab.getUrl());
    }

    private boolean isTabRegularNtp(Tab tab) {
        return isTabNtp(tab) && !tab.isIncognito();
    }

    private void onOmniboxFocusChanged(boolean hasFocus) {
        mTabModalHandler.onOmniboxFocusChanged(hasFocus);
    }

    private void recordLauncherShortcutAction(boolean isIncognito) {
        if (isIncognito) {
            RecordUserAction.record("Android.LauncherShortcut.NewIncognitoTab");
        } else {
            RecordUserAction.record("Android.LauncherShortcut.NewTab");
        }
    }

    @Override
    public boolean handleBackPressed() {
        if (!mUIWithNativeInitialized) return false;

        if (getManualFillingComponent().handleBackPress()) return true;

        if (exitFullscreenIfShowing()) {
            return true;
        }

        if (mTabModalHandler.handleBackPress()) return true;

        final Tab currentTab = getActivityTab();
        if (currentTab == null) {
            moveTaskToBack(true);
            return true;
        }
        if (currentTab.canGoBack()) {
            currentTab.goBack();
            return true;
        }

        // If we are in the tab switcher mode (not in the Start surface homepage) and not a tablet,
        // then leave tab switcher mode on back.
        if (mOverviewModeController.overviewVisible() && !isTablet()) {
            mLayoutManager.showLayout(LayoutType.BROWSING, true);
            return true;
        }

        final WebContents webContents = currentTab.getWebContents();
        if (webContents != null) {
            RenderFrameHost focusedFrame = webContents.getFocusedFrame();
            if (focusedFrame != null && focusedFrame.signalCloseWatcherIfActive()) return true;
        }

        // If the current tab url is HELP_URL, then the back button should close the tab to
        // get back to the previous state. The reason for startsWith check is that the
        // actual redirected URL is a different system language based help url.
        final @TabLaunchType int type = currentTab.getLaunchType();

        final boolean helpUrl = currentTab.getUrl().getSpec().startsWith(HELP_URL_PREFIX);
        if (type == TabLaunchType.FROM_CHROME_UI && helpUrl) {
            getCurrentTabModel().closeTab(currentTab);
            return true;
        }

        final boolean shouldCloseTab = backShouldCloseTab(currentTab);

        // Minimize the app if either:
        // - we decided not to close the tab
        // - we decided to close the tab, but it was opened by an external app, so we will go
        //   exit Chrome on top of closing the tab
        final boolean minimizeApp =
                !shouldCloseTab || TabAssociatedApp.isOpenedFromExternalApp(currentTab);
        if (minimizeApp) {
            if (shouldCloseTab) {
                sendToBackground(currentTab);
                return true;
            } else {
                sendToBackground(null);
                return true;
            }
        } else if (shouldCloseTab) {
            if (webContents != null) webContents.dispatchBeforeUnload(false);
            return true;
        }

        assert false : "The back button should have already been handled by this point";
        return false;
    }

    /**
     * [true]: Reached the bottom of the back stack on a tab the user did not explicitly
     * create (i.e. it was created by an external app or opening a link in background, etc).
     * [false]: Reached the bottom of the back stack on a tab that the user explicitly
     * created (e.g. selecting "new tab" from menu).
     *
     * @return Whether pressing the back button on the provided Tab should close the Tab.
     */
    @Override
    public boolean backShouldCloseTab(Tab tab) {
        if (!tab.isInitialized()) {
            return false;
        }
        @TabLaunchType
        int type = tab.getLaunchType();

        return type == TabLaunchType.FROM_LINK || type == TabLaunchType.FROM_EXTERNAL_APP
                || type == TabLaunchType.FROM_READING_LIST
                || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || type == TabLaunchType.FROM_LONGPRESS_INCOGNITO
                || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                || type == TabLaunchType.FROM_RECENT_TABS
                || (type == TabLaunchType.FROM_RESTORE
                        && CriticalPersistedTabData.from(tab).getParentId() != Tab.INVALID_TAB_ID);
    }

    /**
     * Sends this Activity to the background.
     *
     * @param tabToClose Tab that will be closed once the app is not visible.
     */
    private void sendToBackground(@Nullable final Tab tabToClose) {
        Log.i(TAG, "sendToBackground(): " + tabToClose);
        moveTaskToBack(true);
        if (tabToClose != null) {
            // In the case of closing a tab upon minimization, don't allow the close action to
            // happen until after our app is minimized to make sure we don't get a brief glimpse of
            // the newly active tab before we exit Chrome.
            //
            // If the runnable doesn't run before the Activity dies, Chrome won't crash but the tab
            // won't be closed (crbug.com/587565).
            mHandler.postDelayed(() -> {
                boolean uponExit = ChromeFeatureList.isEnabled(
                        ChromeFeatureList.MOST_RECENT_TAB_ON_BACKGROUND_CLOSE_TAB);
                Tab nextTab = getCurrentTabModel().getNextTabIfClosed(
                        tabToClose.getId(), /*uponExit=*/uponExit);
                getCurrentTabModel().closeTab(tabToClose, nextTab, false, true, false);

                // If there is no next tab to open, enter overview mode.
                if (nextTab == null) showOverview();
            }, CLOSE_TAB_ON_MINIMIZE_DELAY_MS);
        }
    }

    @Override
    public boolean moveTaskToBack(boolean nonRoot) {
        try {
            return super.moveTaskToBack(nonRoot);
        } catch (NullPointerException e) {
            // Work around framework bug described in https://crbug.com/817567.
            finish();
            return true;
        }
    }

    /**
     * Launch a URL from an intent.
     *
     * @param url           The url from the intent.
     * @param referer       Optional referer URL to be used.
     * @param headers       Optional headers to be sent when opening the URL.
     * @param externalAppId External app id.
     * @param forceNewTab   Whether to force the URL to be launched in a new tab or to fall
     *                      back to the default behavior for making that determination.
     * @param isRendererInitiated Whether the intent is originally from browser renderer process.
     * @param initiatorOrigin Origin that initiates the intent.
     * @param intent        The original intent.
     */
    private Tab launchIntent(
            LoadUrlParams loadUrlParams, String externalAppId, boolean forceNewTab, Intent intent) {
        if (mUIWithNativeInitialized && !UrlUtilities.isNTPUrl(loadUrlParams.getUrl())) {
            getLayoutManager().showLayout(LayoutType.BROWSING, false);
        }
        if (IntentHandler.wasIntentSenderChrome(intent)) {
            // If the intent was launched by chrome, open the new tab in the appropriate model.
            boolean isIncognito = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
            @TabLaunchType
            Integer launchType = IntentHandler.getTabLaunchType(intent);
            if (launchType == null) {
                if (IntentUtils.safeGetBooleanExtra(
                            intent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false)) {
                    launchType = TabLaunchType.FROM_LAUNCHER_SHORTCUT;
                } else if (IntentUtils.safeGetBooleanExtra(
                                   intent, IntentHandler.EXTRA_INVOKED_FROM_APP_WIDGET, false)) {
                    launchType = TabLaunchType.FROM_APP_WIDGET;
                } else if (IncognitoTabLauncher.didCreateIntent(intent)) {
                    launchType = TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB;
                } else {
                    // Using FROM_LINK ensures the tab is parented to the current tab, which allows
                    // the back button to close these tabs and restore selection to the previous
                    // tab.
                    launchType = TabLaunchType.FROM_LINK;
                }
            }
            return getTabCreator(isIncognito).createNewTab(loadUrlParams, launchType, null, intent);
        }

        // Check if the tab is being created from a Reader Mode navigation.
        if (ReaderModeManager.isEnabled() && ReaderModeManager.isReaderModeCreatedIntent(intent)) {
            Bundle extras = intent.getExtras();
            int readerParentId = IntentUtils.safeGetInt(
                    extras, ReaderModeManager.EXTRA_READER_MODE_PARENT, Tab.INVALID_TAB_ID);
            extras.remove(ReaderModeManager.EXTRA_READER_MODE_PARENT);
            // Set the parent tab to the tab that Reader Mode started from.
            if (readerParentId != Tab.INVALID_TAB_ID && mTabModelSelector != null) {
                return getCurrentTabCreator().createNewTab(
                        new LoadUrlParams(loadUrlParams.getUrl(), PageTransition.LINK),
                        TabLaunchType.FROM_LINK, mTabModelSelector.getTabById(readerParentId));
            }
        }

        return getTabCreator(false).launchUrlFromExternalApp(
                loadUrlParams, externalAppId, forceNewTab, intent);
    }

    private void showOverview() {
        if (mIsAccessibilityTabSwitcherEnabled != null && mIsAccessibilityTabSwitcherEnabled
                && mOverviewModeController != null) {
            // TODO(1200727): This is a temporary fix that should be removed once grid tab switcher
            //                is completely launched. The "start surface" is now created regardless
            //                of the state of accessibility, so we check that mode first and try
            //                showing the overview list before going to the start surface.
            mLayoutManager.showLayout(LayoutType.TAB_SWITCHER, false);
        }

        if (mOverviewModeController == null) return;

        if (mOverviewModeController.overviewVisible()) {
            if (didFinishNativeInitialization()) {
                getCompositorViewHolderSupplier().get().hideKeyboard(() -> {});
            }
            return;
        }

        Tab currentTab = getActivityTab();
        // If we don't have a current tab, show the overview mode.
        if (currentTab == null) {
            mLayoutManager.showLayout(LayoutType.TAB_SWITCHER, false);
        } else {
            getCompositorViewHolderSupplier().get().hideKeyboard(
                    () -> mLayoutManager.showLayout(LayoutType.TAB_SWITCHER, true));
            updateAccessibilityState(false);
        }
    }

    private void hideOverview() {
        assert (mOverviewModeController.overviewVisible());
        if (getCurrentTabModel().getCount() != 0) {
            // Don't hide overview if current tab stack is empty()
            mLayoutManager.showLayout(LayoutType.BROWSING, false);
            updateAccessibilityState(true);
        }
    }

    private void updateAccessibilityState(boolean enabled) {
        Tab currentTab = getActivityTab();
        WebContents webContents = currentTab != null ? currentTab.getWebContents() : null;
        if (webContents != null) {
            WebContentsAccessibility.fromWebContents(webContents).setState(enabled);
        }
    }

    @Override
    public void onSaveInstanceState(Bundle outState) {
        super.onSaveInstanceState(outState);
        CipherFactory.getInstance().saveToBundle(outState);
        outState.putInt(
                WINDOW_INDEX, TabWindowManagerSingleton.getInstance().getIndexForWindow(this));
        Boolean is_incognito = getCurrentTabModel().isIncognito();
        outState.putBoolean(IS_INCOGNITO_SELECTED, is_incognito);
    }

    @Override
    public void onDestroyInternal() {
        if (mNotificationPermissionController != null) {
            NotificationPermissionController.detach(mNotificationPermissionController);
            mNotificationPermissionController = null;
        }

        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        if (mHistoricalTabModelObserver != null) mHistoricalTabModelObserver.destroy();

        if (mTabModelObserver != null) mTabModelObserver.destroy();

        if (mAppIndexingUtil != null) {
            mAppIndexingUtil.destroy();
            mAppIndexingUtil = null;
        }

        IncognitoTabHostRegistry.getInstance().unregister(mIncognitoTabHost);

        if (isTablet()) ChromeAccessibilityUtil.get().removeObserver(mCompositorViewHolder);
        ChromeAccessibilityUtil.get().removeObserver(this);
        ChromeAccessibilityUtil.get().removeObserver(mLayoutManager);

        if (mTabDelegateFactory != null) mTabDelegateFactory.destroy();

        mAppLaunchDrawBlocker.destroy();

        super.onDestroyInternal();
    }

    @Override
    protected void destroyTabModels() {
        if (mTabModelOrchestrator != null) {
            mTabModelOrchestrator.destroy();
        }
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(event, mUIWithNativeInitialized,
                getFullscreenManager(), /* menuOrKeyboardActionController= */ this);
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        return super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && !isTablet()) {
            mHandler.removeCallbacks(mShowHistoryRunnable);
            mShowHistoryRunnable = null;
            if (event.getEventTime() - event.getDownTime()
                            >= ViewConfiguration.getLongPressTimeout()) {
                // If tab history popup is showing, do not process the keyUp event
                // which will dismiss it immediately.
                return true;
            }
        }
        return super.onKeyUp(keyCode, event);
    }

    @VisibleForTesting
    public boolean hasPendingNavigationRunnableForTesting() {
        ThreadUtils.assertOnUiThread();
        return mShowHistoryRunnable != null;
    }

    @Override
    public void onProvideKeyboardShortcuts(
            List<KeyboardShortcutGroup> data, Menu menu, int deviceId) {
        data.addAll(KeyboardShortcuts.createShortcutGroup(this));
    }

    @VisibleForTesting
    public View getTabsView() {
        return getCompositorViewHolderSupplier().get();
    }

    @VisibleForTesting
    public LayoutManagerChrome getLayoutManager() {
        return (LayoutManagerChrome) getCompositorViewHolderSupplier().get().getLayoutManager();
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        ModalDialogManager manager = super.createModalDialogManager();
        // TODO(crbug.com/1157310): Transition this::method refs to dedicated suppliers.
        mTabModalHandler = new TabModalLifetimeHandler(this, getLifecycleDispatcher(), manager,
                getContextualSearchManagerSupplier(),
                getTabModelSelectorSupplier(), this::getBrowserControlsManager,
                this::getFullscreenManager);
        return manager;
    }

    @Override
    public boolean isInOverviewMode() {
        return mOverviewModeController != null && mOverviewModeController.overviewVisible();
    }

    @Override
    public boolean shouldShowOverviewPageOnStart() {
        return false;
    }

    @Override
    protected IntentHandlerDelegate createIntentHandlerDelegate() {
        return new InternalIntentDelegate();
    }

    @Override
    public void onSceneChange(Layout layout) {
        super.onSceneChange(layout);
        if (!layout.shouldDisplayContentOverlay()) mTabModelSelector.onTabsViewShown();
    }

    /**
     * Writes the tab state to disk.
     */
    @VisibleForTesting
    public void saveState() {
        mTabModelOrchestrator.saveState();

        // Save whether the current tab is a search result page into preferences.
        Tab currentStandardTab = TabModelUtils.getCurrentTab(mTabModelSelector.getModel(false));
        SharedPreferencesManager.getInstance().writeBoolean(
                ChromePreferenceKeys.IS_LAST_VISITED_TAB_SRP,
                currentStandardTab != null
                        && UrlUtilitiesJni.get().isGoogleSearchUrl(
                                currentStandardTab.getUrl().getSpec()));
    }

    @Override
    protected void applyThemeOverlays() {
        super.applyThemeOverlays();
    }

    @Override
    protected boolean supportsDynamicColors() {
        return false;
    }

    /**
     * Reports that a new tab launcher shortcut was selected or an action equivalent to a shortcut
     * was performed.
     * @param isIncognito Whether the shortcut or action created a new incognito tab.
     */
    @RequiresApi(Build.VERSION_CODES.N_MR1)
    private void reportNewTabShortcutUsed(boolean isIncognito) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.N_MR1) return;

        ShortcutManager shortcutManager = getSystemService(ShortcutManager.class);
        shortcutManager.reportShortcutUsed(
                isIncognito ? "new-incognito-tab-shortcut" : "new-tab-shortcut");
    }

    @VisibleForTesting
    public MultiInstanceManager getMultiInstanceMangerForTesting() {
        return mMultiInstanceManager;
    }

    @VisibleForTesting
    public ChromeNextTabPolicySupplier getNextTabPolicySupplier() {
        return (ChromeNextTabPolicySupplier) mNextTabPolicySupplier;
    }
}
