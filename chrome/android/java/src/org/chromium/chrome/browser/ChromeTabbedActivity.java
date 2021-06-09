// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static org.chromium.chrome.features.start_surface.StartSurfaceConfiguration.shouldIntentShowNewTabOmniboxFocused;

import android.annotation.TargetApi;
import android.app.ActivityManager;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
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
import android.view.View.OnClickListener;
import android.view.ViewConfiguration;
import android.view.ViewGroup;
import android.view.Window;
import android.view.WindowManager;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.lifecycle.Lifecycle;
import androidx.lifecycle.LifecycleObserver;
import androidx.lifecycle.LifecycleRegistry;

import org.chromium.base.CallbackController;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.IntentUtils;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.UsedByReflection;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneShotCallback;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.accessibility_tab_switcher.OverviewListLayout;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.metrics.TabbedActivityLaunchCauseMetrics;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.ChromeNextTabPolicySupplier;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.app.tabmodel.TabWindowManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabbedModeTabModelOrchestrator;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.bottombar.ephemeraltab.EphemeralTabCoordinator;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeController;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.dependency_injection.ChromeActivityComponent;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.download.DownloadOpenSource;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.v2.FeedStreamSurface;
import org.chromium.chrome.browser.feed.webfeed.WebFeedBridge;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fonts.FontPreloader;
import org.chromium.chrome.browser.gesturenav.NavigationSheet;
import org.chromium.chrome.browser.homepage.HomepageManager;
import org.chromium.chrome.browser.incognito.IncognitoNotificationManager;
import org.chromium.chrome.browser.incognito.IncognitoNotificationPresenceController;
import org.chromium.chrome.browser.incognito.IncognitoProfileDestroyer;
import org.chromium.chrome.browser.incognito.IncognitoTabLauncher;
import org.chromium.chrome.browser.incognito.IncognitoTabSnapshotController;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.infobar.DataReductionPromoInfoBar;
import org.chromium.chrome.browser.infobar.SyncErrorInfoBar;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.MainIntentBehaviorMetrics;
import org.chromium.chrome.browser.modaldialog.ChromeTabModalPresenter;
import org.chromium.chrome.browser.modaldialog.TabModalLifetimeHandler;
import org.chromium.chrome.browser.multiwindow.MultiInstanceChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager;
import org.chromium.chrome.browser.native_page.NativePageAssassin;
import org.chromium.chrome.browser.navigation_predictor.NavigationPredictorBridge;
import org.chromium.chrome.browser.night_mode.WebContentsDarkModeController;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.omnibox.OmniboxFocusReason;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelper;
import org.chromium.chrome.browser.paint_preview.StartupPaintPreviewHelperSupplier;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.reengagement.ReengagementNotificationController;
import org.chromium.chrome.browser.search_engines.SearchEngineChoiceNotification;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.survey.ChromeSurveyController;
import org.chromium.chrome.browser.tab.RedirectHandlerTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabAssociatedApp;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.state.CriticalPersistedTabData;
import org.chromium.chrome.browser.tabbed_mode.TabbedAppMenuPropertiesDelegate;
import org.chromium.chrome.browser.tabbed_mode.TabbedRootUiCoordinator;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHost;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.EngagementTimeUtil;
import org.chromium.chrome.browser.tasks.JourneyManager;
import org.chromium.chrome.browser.tasks.ReturnToChromeExperimentsUtil;
import org.chromium.chrome.browser.tasks.TasksUma;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabManagementModuleProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabUiFeatureUtilities;
import org.chromium.chrome.browser.toolbar.ToolbarButtonInProductHelpController;
import org.chromium.chrome.browser.toolbar.ToolbarIntentMetadata;
import org.chromium.chrome.browser.toolbar.top.ToolbarControlContainer;
import org.chromium.chrome.browser.translate.TranslateIntentHandler;
import org.chromium.chrome.browser.ui.AppLaunchDrawBlocker;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.undo_tab_close_snackbar.UndoBarController;
import org.chromium.chrome.browser.usage_stats.UsageStatsService;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.features.start_surface.StartSurface;
import org.chromium.chrome.features.start_surface.StartSurfaceConfiguration;
import org.chromium.chrome.features.start_surface.StartSurfaceState;
import org.chromium.chrome.features.start_surface.StartSurfaceUserData;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.components.browser_ui.util.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.embedder_support.util.UrlUtilities;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.profile_metrics.BrowserProfileType;
import org.chromium.components.webapps.ShortcutSource;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.widget.Toast;
import org.chromium.url.Origin;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Locale;

/**
 * This is the main activity for ChromeMobile when not running in document mode.  All the tabs
 * are accessible via a chrome specific tab switching UI.
 */
public class ChromeTabbedActivity extends ChromeActivity<ChromeActivityComponent>
        implements ChromeAccessibilityUtil.Observer {
    private static final String TAG = "ChromeTabbedActivity";

    private static final String HELP_URL_PREFIX = "https://support.google.com/chrome/";

    private static final String WINDOW_INDEX = "window_index";

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

    /**
     * Identifies a histogram to use in {@link #maybeDispatchExplicitMainViewIntent(Intent, int)}.
     */
    @IntDef({DispatchedBy.ON_CREATE, DispatchedBy.ON_NEW_INTENT})
    @Retention(RetentionPolicy.SOURCE)
    private @interface DispatchedBy {
        int ON_CREATE = 1;
        int ON_NEW_INTENT = 2;
    }

    // Count histogram used to track number of tabs when we show the Overview on Return to Chrome.
    private static final String TAB_COUNT_ON_RETURN = "Tabs.TabCountOnStartScreenShown";

    private final MainIntentBehaviorMetrics mMainIntentMetrics;
    private final @Nullable MultiInstanceManager mMultiInstanceManager;

    private UndoBarController mUndoBarPopupController;

    private LayoutManagerChrome mLayoutManager;

    private ViewGroup mContentContainer;

    private ToolbarControlContainer mControlContainer;

    private TabbedModeTabModelOrchestrator mTabModelOrchestrator;
    private TabModelSelectorImpl mTabModelSelectorImpl;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private TabModelSelectorTabModelObserver mTabModelObserver;

    private BrowserControlsVisibilityDelegate mVrBrowserControlsVisibilityDelegate;
    private TabModalLifetimeHandler mTabModalHandler;

    private boolean mUIWithNativeInitialized;

    private Boolean mIsAccessibilityTabSwitcherEnabled;

    private LocaleManager mLocaleManager;

    private AppIndexingUtil mAppIndexingUtil;

    private Runnable mShowHistoryRunnable;

    private CompositorViewHolder mCompositorViewHolder;
    private OverviewListLayout mOverviewListLayout;
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
    // Supplier for a dependency to inform about the type of intent used to launch Chrome.
    private OneshotSupplierImpl<ToolbarIntentMetadata> mIntentMetadataOneshotSupplier =
            new OneshotSupplierImpl<>();

    // Time at which an intent was received and handled.
    private long mIntentHandlingTimeMs;

    /**
     * Whether the StartSurface is shown when Chrome is launched.
     */
    private boolean mOverviewShownOnStart;

    private NextTabPolicySupplier mNextTabPolicySupplier;

    private final UnownedUserDataSupplier<StartupPaintPreviewHelper>
            mStartupPaintPreviewHelperSupplier = new StartupPaintPreviewHelperSupplier();

    private final OneshotSupplierImpl<LayoutStateProvider> mLayoutStateProviderOneshotSupplier =
            new OneshotSupplierImpl<>();
    // TODO(crbug.com/1108496): Removed after all usages has been migrated to LayoutStateProvider.
    private final OneshotSupplierImpl<OverviewModeBehavior> mOverviewModeBehaviorSupplier =
            new OneshotSupplierImpl<>();
    private OverviewModeController mOverviewModeController;

    private ObservableSupplierImpl<EphemeralTabCoordinator> mEphemeralTabCoordinatorSupplier =
            new ObservableSupplierImpl<>();
    private final OneshotSupplierImpl<StartSurface> mStartSurfaceSupplier =
            new OneshotSupplierImpl<>();
    private ObservableSupplierImpl<Tab> mStartSurfaceParentTabSupplier =
            new ObservableSupplierImpl<>();

    private CallbackController mCallbackController = new CallbackController();
    private TabbedModeTabDelegateFactory mTabDelegateFactory;

    private final AppLaunchDrawBlocker mAppLaunchDrawBlocker;

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

            getTabModelSelector().getModel(true).closeAllTabs(false, false);
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
     * Return whether the passed in component name matches any of the supported tabbed mode
     * activities.
     */
    public static boolean isTabbedModeComponentName(String componentName) {
        return TextUtils.equals(componentName, ChromeTabbedActivity.class.getName())
                || TextUtils.equals(
                        componentName, MultiInstanceChromeTabbedActivity.class.getName())
                || TextUtils.equals(componentName, ChromeTabbedActivity2.class.getName())
                || TextUtils.equals(componentName, MAIN_LAUNCHER_ACTIVITY_NAME);
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
        mMainIntentMetrics = new MainIntentBehaviorMetrics();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            mMultiInstanceManager =
                    new MultiInstanceManager(this, getTabModelOrchestratorSupplier(),
                            getMultiWindowModeStateDispatcher(), getLifecycleDispatcher(), this);
        } else {
            mMultiInstanceManager = null;
        }

        // AppLaunchDrawBlocker may block drawing the Activity content until the initial tab is
        // available.
        // clang-format off
        mAppLaunchDrawBlocker = new AppLaunchDrawBlocker(getLifecycleDispatcher(),
                () -> findViewById(android.R.id.content),
                this::getIntent, this::shouldIgnoreIntent, this::isTablet,
                this::shouldShowTabSwitcherOnStart);
        // clang-format on
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
        @LaunchIntentDispatcher.Action
        int action = maybeDispatchExplicitMainViewIntent(intent, DispatchedBy.ON_CREATE);
        if (action != LaunchIntentDispatcher.Action.CONTINUE) {
            return action;
        }
        return super.maybeDispatchLaunchIntent(intent, savedInstanceState);
    }

    // We know of at least one app that explicitly specifies .Main activity in custom tab
    // intents. The app shouldn't be doing that, but until it's updated, we need to support
    // such use case.
    //
    // This method attempts to treat VIEW intents explicitly sent to .Main as custom tab
    // intents, and dispatch them accordingly. If the intent was not dispatched, the method
    // returns Action.CONTINUE.
    //
    // The method also updates the supplied boolean histogram with the dispatching result,
    // but only if the intent is a VIEW intent sent explicitly to .Main activity.
    private @LaunchIntentDispatcher.Action int maybeDispatchExplicitMainViewIntent(
            Intent intent, @DispatchedBy int dispatchedBy) {
        // The first check ensures that this is .Main activity alias (we can't check exactly, but
        // this gets us sufficiently close).
        if (getClass().equals(ChromeTabbedActivity.class)
                && Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getComponent() != null
                && MAIN_LAUNCHER_ACTIVITY_NAME.equals(intent.getComponent().getClassName())) {
            @LaunchIntentDispatcher.Action
            int action = LaunchIntentDispatcher.dispatchToCustomTabActivity(this, intent);
            switch (dispatchedBy) {
                case DispatchedBy.ON_CREATE:
                    RecordHistogram.recordBooleanHistogram(
                            "Android.MainActivity.ExplicitMainViewIntentDispatched.OnCreate",
                            action != LaunchIntentDispatcher.Action.CONTINUE);
                    break;
                case DispatchedBy.ON_NEW_INTENT:

                    RecordHistogram.recordBooleanHistogram(
                            "Android.MainActivity.ExplicitMainViewIntentDispatched.OnNewIntent",
                            action != LaunchIntentDispatcher.Action.CONTINUE);
                    break;
                default:
                    assert false : "Unknown dispatchedBy value " + dispatchedBy;
            }
            if (action == LaunchIntentDispatcher.Action.CONTINUE) {
                // Intent was not dispatched, record its source.
                @IntentHandler.ExternalAppId
                int externalId = IntentHandler.determineExternalIntentSource(intent);
                RecordHistogram.recordEnumeratedHistogram(
                        "Android.MainActivity.UndispatchedExplicitMainViewIntentSource", externalId,
                        IntentHandler.ExternalAppId.NUM_ENTRIES);

                // Crash if intent came from us, but only in debug builds and only if we weren't
                // explicitly told not to. Hopefully we'll get enough reports to find where
                // these intents come from.
                if (externalId == IntentHandler.ExternalAppId.CHROME
                        && 0 != (getApplicationInfo().flags & ApplicationInfo.FLAG_DEBUGGABLE)
                        && !CommandLine.getInstance().hasSwitch(
                                ChromeSwitches.DONT_CRASH_ON_VIEW_MAIN_INTENTS)) {
                    String intentInfo = intent.toString();
                    Bundle extras = intent.getExtras();
                    if (extras != null) {
                        intentInfo +=
                                ", extras.keySet = [" + TextUtils.join(", ", extras.keySet()) + "]";
                    }
                    String message = String.format((Locale) null,
                            "VIEW intent sent to .Main activity alias was not dispatched. PLEASE "
                                    + "report the following info to crbug.com/789732: \"%s\". Use "
                                    + "--%s flag to disable this check.",
                            intentInfo, ChromeSwitches.DONT_CRASH_ON_VIEW_MAIN_INTENTS);
                    throw new IllegalStateException(message);
                }
            }
            return action;
        }
        return LaunchIntentDispatcher.Action.CONTINUE;
    }

    @Override
    public void initializeCompositor() {
        try {
            TraceEvent.begin("ChromeTabbedActivity.initializeCompositor");
            super.initializeCompositor();

            // LocaleManager can only function after the native library is loaded.
            mLocaleManager = LocaleManager.getInstance();
            mLocaleManager.showSearchEnginePromoIfNeeded(this, null);

            mTabModelOrchestrator.onNativeLibraryReady(getTabContentManager());

            mTabModelObserver = new TabModelSelectorTabModelObserver(mTabModelSelectorImpl) {
                @Override
                public void didCloseTab(int tabId, boolean incognito) {
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
                    if (getTabModelSelector().getTotalTabCount() == 0) {
                        // If the last tab is closed, and homepage is enabled, then exit Chrome.
                        if (HomepageManager.shouldCloseAppWithZeroTabs()) {
                            finish();
                        } else if (isPendingClosure) {
                            NewTabPageUma.recordNTPImpression(
                                    NewTabPageUma.NTP_IMPESSION_POTENTIAL_NOTAB);
                        }
                    }

                    // TODO(960196) : remove this when the associated bug fix. This is a band-aid
                    //  fix for TabGrid and closing tabs with TabGroupUi.
                    //  If one of the following is true, then exit Chrome when TabGroupsAndroid is
                    //  enabled, and tab switcher is not shown:
                    //   1. If the very last tab is closed.
                    //   2. If normal tab model is selected and no normal tabs.
                    if (TabUiFeatureUtilities.isGridTabSwitcherEnabled()
                            && !mOverviewModeController.overviewVisible()) {
                        if (getTabModelSelector().getTotalTabCount() == 0
                                || (!getTabModelSelector().isIncognitoSelected()
                                        && getTabModelSelector().getModel(false).getCount() == 0)) {
                            finish();
                        }
                    }
                }

                @Override
                public void didAddTab(
                        Tab tab, @TabLaunchType int type, @TabCreationState int creationState) {
                    if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                            && !DeviceClassManager.enableAnimations()) {
                        Toast.makeText(ChromeTabbedActivity.this, R.string.open_in_new_tab_toast,
                                     Toast.LENGTH_SHORT)
                                .show();
                    }
                    SyncErrorInfoBar.maybeLaunchSyncErrorInfoBar(tab.getWebContents());
                }

                @Override
                public void multipleTabsPendingClosure(List<Tab> tabs, boolean isAllTabs) {
                    if (!isAllTabs) return;
                    NewTabPageUma.recordNTPImpression(NewTabPageUma.NTP_IMPESSION_POTENTIAL_NOTAB);
                }
            };
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeCompositor");
        }
    }

    private void refreshSignIn() {
        try (TraceEvent e = TraceEvent.scoped("ChromeTabbedActivity.refreshSignIn")) {
            FirstRunSignInProcessor.start(this);
        }
    }

    private void setupCompositorContentPreNativeForPhone() {
        if (isTablet()) return;

        try (TraceEvent e = TraceEvent.scoped(
                     "ChromeTabbedActivity.setupCompositorContentPreNativeForPhone")) {
            CompositorViewHolder compositorViewHolder = getCompositorViewHolder();

            // TODO(1169205): Remove all GTS enabled checks after M5 is default.
            if (TabUiFeatureUtilities.isGridTabSwitcherEnabled()) {
                TabManagementDelegate tabManagementDelegate =
                        TabManagementModuleProvider.getDelegate();
                if (tabManagementDelegate != null) {
                    StartSurface startSurface = tabManagementDelegate.createStartSurface(this,
                            mRootUiCoordinator.getScrimCoordinator(),
                            mRootUiCoordinator.getBottomSheetController(), mStartSurfaceSupplier,
                            mStartSurfaceParentTabSupplier, hadWarmStart(), getWindowAndroid());
                }
            }

            // clang-format off
            mLayoutManager = new LayoutManagerChromePhone(compositorViewHolder, mContentContainer,
                    mStartSurfaceSupplier.get(), getTabContentManagerSupplier(),
                    () -> {
                        if (getCompositorViewHolder() == null) return null;
                        return getCompositorViewHolder().getLayerTitleCache();
                    },
                    mOverviewModeBehaviorSupplier, mLayoutStateProviderOneshotSupplier,
                    mRootUiCoordinator::getTopUiThemeColorProvider);
            // clang-format on
            mOverviewModeController = mLayoutManager;
        }
    }

    private void setupCompositorContentPreNativeForTablet() {
        if (!isTablet()) return;

        try (TraceEvent e = TraceEvent.scoped(
                     "ChromeTabbedActivity.setupCompositorContentPreNativeForTablet")) {
            // clang-format off
            mLayoutManager = new LayoutManagerChromeTablet(getCompositorViewHolder(),
                    mContentContainer, getTabContentManagerSupplier(),
                    () -> {
                        if (getCompositorViewHolder() == null) return null;
                        return getCompositorViewHolder().getLayerTitleCache();
                    },
                    mOverviewModeBehaviorSupplier, mLayoutStateProviderOneshotSupplier,
                    mRootUiCoordinator::getTopUiThemeColorProvider);
            // clang-format on
            mOverviewModeController = mLayoutManager;
        }
    }

    private void setupCompositorContentPostNative() {
        try (TraceEvent e = TraceEvent.scoped(
                     "ChromeTabbedActivity.setupCompositorContentPostNative")) {
            if (!isLayoutManagerCreated()) {
                if (isTablet()) {
                    setupCompositorContentPreNativeForTablet();
                } else {
                    setupCompositorContentPreNativeForPhone();
                }
            }

            mLayoutManager.setEnableAnimations(DeviceClassManager.enableAnimations());

            // TODO(yusufo): get rid of findViewById(R.id.url_bar).
            initializeCompositorContent(mLayoutManager, findViewById(R.id.url_bar),
                    mContentContainer, mControlContainer);
        }
    }

    private boolean isLayoutManagerCreated() {
        return mLayoutManager != null;
    }

    private void initializeToolbarManager() {
        try (TraceEvent e = TraceEvent.scoped("ChromeTabbedActivity.initializeToolbarManager")) {
            mUndoBarPopupController.initialize();

            OnClickListener tabSwitcherClickHandler = v -> {
                if (ChromeFeatureList.isEnabled(ChromeFeatureList.TOOLBAR_IPH_ANDROID)) {
                    Profile profile = mTabModelProfileSupplier.get();
                    if (profile != null) {
                        TrackerFactory.getTrackerForProfile(profile).notifyEvent(
                                EventConstants.TAB_SWITCHER_BUTTON_CLICKED);
                    }
                }

                if (getFullscreenManager().getPersistentFullscreenMode()) {
                    return;
                }

                if (isInOverviewMode() && !StartSurfaceConfiguration.isStartSurfaceEnabled()) {
                    hideOverview();
                } else {
                    showOverview(StartSurfaceState.SHOWING_TABSWITCHER);
                }
            };
            OnClickListener newTabClickHandler = v -> {
                getTabModelSelector().getModel(false).commitAllTabClosures();
                // This assumes that the keyboard can not be seen at the same time as the
                // newtab button on the toolbar.
                getCurrentTabCreator().launchNTP();
                mLocaleManager.showSearchEnginePromoIfNeeded(ChromeTabbedActivity.this, null);
                if (getTabModelSelector().isIncognitoSelected()) {
                    RecordUserAction.record("MobileToolbarStackViewNewIncognitoTab");
                } else {
                    RecordUserAction.record("MobileToolbarStackViewNewTab");
                }
                RecordUserAction.record("MobileTopToolbarNewTabButton");

                RecordUserAction.record("MobileNewTabOpened");
            };
            OnClickListener bookmarkClickHandler = v -> addOrEditBookmark(getActivityTab());

            Supplier<Boolean> showStartSurfaceSupplier = () -> {
                if (ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePageOnPhone(
                            isTablet())) {
                    showOverview(StartSurfaceState.SHOWING_HOMEPAGE);
                    return true;
                }
                return false;
            };

            getToolbarManager().initializeWithNative(mLayoutManager, tabSwitcherClickHandler,
                    newTabClickHandler, bookmarkClickHandler, null, showStartSurfaceSupplier);

            if (!TabUiFeatureUtilities.supportInstantStart(isTablet())) {
                assert !(mOverviewModeController != null
                        && mOverviewModeController.overviewVisible());
            }
        }
    }

    private void maybeCreateIncognitoTabSnapshotController() {
        try (TraceEvent e = TraceEvent.scoped(
                     "ChromeTabbedActivity.maybeCreateIncognitoTabSnapshotController")) {
            if (!CommandLine.getInstance().hasSwitch(
                        ChromeSwitches.ENABLE_INCOGNITO_SNAPSHOTS_IN_ANDROID_RECENTS)) {
                IncognitoTabSnapshotController.createIncognitoTabSnapshotController(
                        getWindow(), mLayoutManager, mTabModelSelectorImpl);
            }

            mUIWithNativeInitialized = true;
            onAccessibilityTabSwitcherModeChanged();

            // The dataset has already been created, we need to initialize our state.
            mTabModelSelectorImpl.notifyChanged();

            // Check for incognito tabs to handle the case where Chrome was swiped away in the
            // background.
            if (!IncognitoUtils.doIncognitoTabsExist()) {
                IncognitoNotificationManager.dismissIncognitoNotification();
            }
        }
    }

    private void maybeGetFeedAppLifecycleAndMaybeCreatePageViewObserver() {
        try (TraceEvent e = TraceEvent.scoped("ChromeTabbedActivity."
                     + "maybeGetFeedAppLifecycleAndMaybeCreatePageViewObserver")) {
            FeedStreamSurface.startup();

            if (UsageStatsService.isEnabled()) {
                UsageStatsService.getInstance().createPageViewObserver(
                        mTabModelSelectorImpl, this, getTabContentManagerSupplier());
            }
        }
    }

    private void initJourneyManager() {
        assert mOverviewModeController != null;

        try (TraceEvent e = TraceEvent.scoped("ChromeTabbedActivity.initJourneyManager")) {
            if (ChromeFeatureList.isEnabled(ChromeFeatureList.TAB_ENGAGEMENT_REPORTING_ANDROID)) {
                // The lifecycle of this object is managed by the lifecycle dispatcher.
                new JourneyManager(getTabModelSelector(), getLifecycleDispatcher(),
                        mOverviewModeController, new EngagementTimeUtil());
            }
        }
    }

    @Override
    public void onNewIntent(Intent intent) {
        // The intent to use in maybeDispatchExplicitMainViewIntent(). We're explicitly
        // adding NEW_TASK flag to make sure backing from CCT brings up the caller activity,
        // and not Chrome
        Intent intentForDispatching = new Intent(intent);
        intentForDispatching.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        @LaunchIntentDispatcher.Action
        int action = maybeDispatchExplicitMainViewIntent(
                intentForDispatching, DispatchedBy.ON_NEW_INTENT);
        if (action != LaunchIntentDispatcher.Action.CONTINUE) {
            // Pressing back button in CCT should bring user to the caller activity.
            moveTaskToBack(true);
            // Intent was dispatched to CustomTabActivity, consume it.
            return;
        }

        mIntentHandlingTimeMs = SystemClock.uptimeMillis();
        super.onNewIntent(intent);
    }

    @Override
    public void startNativeInitialization() {
        try (TraceEvent e = TraceEvent.scoped("ChromeTabbedActivity.startNativeInitialization")) {
            // This is on the critical path so don't delay.
            if (ChromeFeatureList.isEnabled(
                        ChromeFeatureList.DARKEN_WEBSITES_CHECKBOX_IN_THEMES_SETTING)) {
                WebContentsDarkModeController.createInstance();
            }
            setupCompositorContentPostNative();

            // All this initialization can be expensive so it's split into multiple tasks.
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::refreshSignIn);
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::initializeToolbarManager);
            PostTask.postTask(
                    UiThreadTaskTraits.DEFAULT, this::maybeCreateIncognitoTabSnapshotController);
            PostTask.postTask(
                    UiThreadTaskTraits.DEFAULT, this::onAccessibilityTabSwitcherModeChanged);

            PostTask.postTask(UiThreadTaskTraits.DEFAULT,
                    this::maybeGetFeedAppLifecycleAndMaybeCreatePageViewObserver);
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::initJourneyManager);
            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::finishNativeInitialization);
            ChromeAccessibilityUtil.get().addObserver(this);
        }
    }

    @Override
    public void finishNativeInitialization() {
        super.finishNativeInitialization();

        // TODO(jinsukkim): Let these classes handle the registration by themselves.
        mCompositorViewHolder = getCompositorViewHolder();
        mOverviewListLayout = (OverviewListLayout) mLayoutManager.getOverviewListLayout();
        getTabObscuringHandler().addObserver(mCompositorViewHolder);
        getTabObscuringHandler().addObserver(mOverviewListLayout);

        ChromeAccessibilityUtil.get().addObserver(mLayoutManager);
        if (isTablet()) ChromeAccessibilityUtil.get().addObserver(mCompositorViewHolder);
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();

        if (IncognitoUtils.shouldDestroyIncognitoProfileOnStartup(
                    getTabModelSelector().getCurrentModel().isIncognito())) {
            Profile.getLastUsedRegularProfile()
                    .getPrimaryOTRProfile(/*createIfNeeded=*/true)
                    .destroyWhenAppropriate();
        } else {
            CookiesFetcher.restoreCookies();
        }

        mLocaleManager.setSnackbarManager(getSnackbarManager());
        mLocaleManager.startObservingPhoneChanges();

        if (isWarmOnResume()) {
            NavigationPredictorBridge.onActivityWarmResumed();
        } else {
            NavigationPredictorBridge.onColdStart();
        }

        // This call is not guarded by a feature flag.
        SearchEngineChoiceNotification.handleSearchEngineChoice(
                this, getSnackbarManager(), new SettingsLauncherImpl());

        if (!isWarmOnResume()) {
            SuggestionsMetrics.recordArticlesListVisible();
        }
    }

    @Override
    public void onPauseWithNative() {
        mTabModelSelectorImpl.commitAllTabClosures();
        CookiesFetcher.persistCookies();

        mLocaleManager.setSnackbarManager(null);
        mLocaleManager.stopObservingPhoneChanges();

        NavigationPredictorBridge.onPause();

        super.onPauseWithNative();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();

        mTabModelOrchestrator.saveState();
        mHasDeterminedOverviewStateForCurrentSession = false;
    }

    @Override
    public void onStartWithNative() {
        mMainIntentMetrics.logLaunchBehavior();
        super.onStartWithNative();

        // Don't call setInitialOverviewState if we're waiting for the tab's creation or we risk
        // showing a glimpse of the tab selector during start up.
        if (!mPendingInitialTabCreation) {
            setInitialOverviewState();
        }

        if (TabUiFeatureUtilities.isConditionalTabStripEnabled()
                || ConditionalTabStripUtils.getOptOutIndicator()) {
            ConditionalTabStripUtils.updateFeatureExpiration(
                    mInactivityTracker.getLastBackgroundedTimeMs());
        }

        resetSavedInstanceState();
        StartSurfaceConfiguration.addFeedVisibilityObserver();
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        try {
            TraceEvent.begin("ChromeTabbedActivity.onNewIntentWithNative");

            super.onNewIntentWithNative(intent);
            if (IntentUtils.isMainIntentFromLauncher(intent)) {
                logMainIntentBehavior(intent);
            }

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

    @Override
    public @Nullable OneshotSupplier<OverviewModeBehavior> getOverviewModeBehaviorSupplier() {
        return mOverviewModeBehaviorSupplier;
    }

    /**
     * @return The toolbar button in-product help controller for this activity.
     * TODO(pnoland, https://crbug.com/865801): remove this in favor
     *        of injecting the controller directly where it's needed.
     */
    public ToolbarButtonInProductHelpController getToolbarButtonInProductHelpController() {
        return ((TabbedRootUiCoordinator) mRootUiCoordinator)
                .getToolbarButtonInProductHelpController();
    }

    private void handleDebugIntent(Intent intent) {
        if (ACTION_CLOSE_TABS.equals(intent.getAction())) {
            getTabModelSelector().closeAllTabs();
        } else if (MemoryPressureListener.handleDebugIntent(
                           ChromeTabbedActivity.this, intent.getAction())) {
            // Handled.
        }
    }

    private void setTrackColdStartupMetrics(boolean shouldTrackColdStartupMetrics) {
        assert getActivityTabStartupMetricsTracker() != null;

        if (shouldTrackColdStartupMetrics) {
            getActivityTabStartupMetricsTracker().trackStartupMetrics(STARTUP_UMA_HISTOGRAM_SUFFIX);
        } else {
            getActivityTabStartupMetricsTracker().cancelTrackingStartupMetrics();
        }

        // Paint Preview should follow the same logic as startup UMA histograms as the feature
        // should only run on cold startup of Chrome when the user is unable to interact before
        // entering a tab.
        StartupPaintPreviewHelper.setShouldShowOnRestore(shouldTrackColdStartupMetrics);
    }

    private void setInitialOverviewState() {
        if (mHasDeterminedOverviewStateForCurrentSession) return;

        mHasDeterminedOverviewStateForCurrentSession = true;
        boolean isOverviewVisible = mOverviewModeController.overviewVisible();

        if (shouldRefreshAndShowOverview(isOverviewVisible)) {
            if (getCurrentTabModel() != null) {
                RecordHistogram.recordCountHistogram(
                        TAB_COUNT_ON_RETURN, getCurrentTabModel().getCount());
            }
            if (TabUiFeatureUtilities.isGridTabSwitcherEnabled() && !isTablet()) {
                mStartSurfaceSupplier.get().getController().enableRecordingFirstMeaningfulPaint(
                        getOnCreateTimestampMs());
            }
            mOverviewShownOnStart = true;
            // Cancel recording cold startup metrics if an overview is shown as they expect a tab to
            // be the first thing shown after startup.
            setTrackColdStartupMetrics(false);
            showOverview(StartSurfaceState.SHOWING_START);
            return;
        }

        if (getActivityTab() == null && !isOverviewVisible) {
            mOverviewShownOnStart = true;
            // Cancel recording cold startup metrics if an overview is shown as they expect a tab to
            // be the first thing shown after startup.
            setTrackColdStartupMetrics(false);
            showOverview(StartSurfaceState.SHOWING_START);
        }

        if (IntentUtils.isMainIntentFromLauncher(getIntent())
                && mOverviewModeController.overviewVisible()) {
            RecordUserAction.record("MobileStartup.UserEnteredTabSwitcher");
        }
    }

    private boolean shouldRefreshAndShowOverview(boolean isOverviewVisible) {
        // If StartSurfaceConfiguration.NEW_SURFACE_FROM_HOME_BUTTON is turned on, MV tiles and
        // carousels may be hidden before Chrome is brought to the background. If overview should be
        // shown, no matter overview was already visible or not, we should call
        // showOverview(StartSurfaceState.SHOWING_START) to show MV tiles and carousels again.
        return shouldShowTabSwitcherOnStart()
                && (!isOverviewVisible
                        || StartSurfaceConfiguration.shouldShowNewSurfaceFromHomeButton());
    }

    private boolean shouldShowTabSwitcherOnStart() {
        String intentUrl = IntentHandler.getUrlFromIntent(getIntent());
        // If Chrome is launched by tapping the New Tab Item from the launch icon and
        // {@link OMNIBOX_FOCUSED_ON_NEW_TAB} is enabled, a new Tab with omnibox focused will be
        // shown on Startup.
        boolean isCanonicalizedNTPUrl =
                ReturnToChromeExperimentsUtil.isCanonicalizedNTPUrl(intentUrl);
        if (shouldIntentShowNewTabOmniboxFocused(getIntent())) {
            return false;
        }

        if (isCanonicalizedNTPUrl
                && ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePage()) {
            // Handle NTP intent.
            return true;
        } else if (ReturnToChromeExperimentsUtil.shouldShowStartSurfaceAsTheHomePageNoTabs()
                && IntentUtils.isMainIntentFromLauncher(getIntent())
                && ReturnToChromeExperimentsUtil.getTotalTabCount(getTabModelSelector()) <= 0) {
            // Handle initial tab creation.
            return true;
        }
        long lastBackgroundedTimeMillis = mInactivityTracker.getLastBackgroundedTimeMs();
        return IntentUtils.isMainIntentFromLauncher(getIntent())
                && ReturnToChromeExperimentsUtil.shouldShowTabSwitcher(lastBackgroundedTimeMillis);
    }

    private void logMainIntentBehavior(Intent intent) {
        assert IntentUtils.isMainIntentFromLauncher(intent);
        // TODO(tedchoc): We should cache the last visible time and reuse it to avoid different
        //                values of this depending on when it is called after the activity was
        //                shown.

        // Temporary safety check to make sure none of this code runs if the feature is
        // disabled.
        if (ReengagementNotificationController.isEnabled()) {
            if (mCallbackController != null) {
                new OneShotCallback<>(
                        mTabModelProfileSupplier, mCallbackController.makeCancelable(profile -> {
                            assert profile != null : "Unexpectedly null profile from TabModel.";
                            if (profile == null) return;

                            TrackerFactory.getTrackerForProfile(profile).notifyEvent(
                                    EventConstants.STARTED_FROM_MAIN_INTENT);
                        }));
            }
        }

        mMainIntentMetrics.onMainIntentWithNative(
                mInactivityTracker.getTimeSinceLastBackgroundedMs());
    }

    /** Access the main intent metrics for test validation. */
    @VisibleForTesting
    public MainIntentBehaviorMetrics getMainIntentBehaviorMetricsForTesting() {
        return mMainIntentMetrics;
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
                    logMainIntentBehavior(intent);
                }
            }

            mIntentMetadataOneshotSupplier.set(
                    new ToolbarIntentMetadata(isMainIntentFromLauncher, isIntentWithEffect));

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
                    || (shouldShowTabSwitcherOnStart()
                            && !mTabModelSelectorImpl.isIncognitoSelected());

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
                    if (!isActivityFinishingOrDestroyed()) createInitialTab();
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
        mPendingInitialTabCreation = false;

        // If the start surface will be shown on start, do not create a new tab.
        if (!shouldShowTabSwitcherOnStart()) {
            String url = HomepageManager.getHomepageUri();
            if (TextUtils.isEmpty(url)) {
                url = UrlConstants.NTP_URL;
            } else {
                // Migrate legacy NTP URLs (chrome://newtab) to the newer format
                // (chrome-native://newtab)
                if (UrlUtilities.isNTPUrl(url)) {
                    url = UrlConstants.NTP_URL;
                }
            }

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

        boolean accessibilityTabSwitcherEnabled = DeviceClassManager.enableAccessibilityLayout();
        if (mOverviewModeController != null && mOverviewModeController.overviewVisible()
                && (mIsAccessibilityTabSwitcherEnabled == null
                        || mIsAccessibilityTabSwitcherEnabled
                                != DeviceClassManager.enableAccessibilityLayout())) {
            mOverviewModeController.hideOverview(true);
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
        public void processUrlViewIntent(String url, String referer, String headers,
                @TabOpenType int tabOpenType, String externalAppId, int tabIdToBringToFront,
                boolean hasUserGesture, boolean isRendererInitiated,
                @Nullable Origin initiatorOrigin, Intent intent) {
            if (isActivityFinishingOrDestroyed()) {
                return;
            }
            if (isFromChrome(intent, externalAppId)) {
                RecordUserAction.record("MobileTabbedModeViewIntentFromChrome");
            } else {
                RecordUserAction.record("MobileTabbedModeViewIntentFromApp");
            }

            boolean fromLauncherShortcut = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false);
            boolean focus = false;

            TabModel tabModel = getCurrentTabModel();
            switch (tabOpenType) {
                case TabOpenType.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB:
                    mTabModelOrchestrator.tryToRestoreTabStateForUrl(url);
                    int tabToBeClobberedIndex = TabModelUtils.getTabIndexByUrl(tabModel, url);
                    Tab tabToBeClobbered = tabModel.getTabAt(tabToBeClobberedIndex);
                    if (tabToBeClobbered != null) {
                        TabModelUtils.setIndex(tabModel, tabToBeClobberedIndex);
                        tabToBeClobbered.reload();
                    } else {
                        launchIntent(url, referer, headers, externalAppId, true,
                                isRendererInitiated, initiatorOrigin, intent);
                    }
                    logMobileReceivedExternalIntent(externalAppId, intent);
                    int shortcutSource =
                            intent.getIntExtra(ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
                    LaunchMetrics.recordHomeScreenLaunchIntoTab(url, shortcutSource);
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
                            TabModelUtils.setIndex(otherModel, tabIndex);
                        } else {
                            Log.e(TAG, "Failed to bring tab to front because it doesn't exist.");
                            return;
                        }
                    } else {
                        TabModelUtils.setIndex(tabModel, tabIndex);
                    }

                    logMobileReceivedExternalIntent(externalAppId, intent);
                    break;
                case TabOpenType.CLOBBER_CURRENT_TAB:
                    // The browser triggered the intent. This happens when clicking links which
                    // can be handled by other applications (e.g. www.youtube.com links).
                    Tab currentTab = getActivityTab();
                    if (currentTab != null) {
                        RedirectHandlerTabHelper.updateIntentInTab(currentTab, intent);
                        LoadUrlParams loadUrlParams =
                                ChromeTabbedActivity.createLoadUrlParamsForIntent(url, referer,
                                        hasUserGesture, mIntentHandlingTimeMs, intent);
                        loadUrlParams.setIsRendererInitiated(isRendererInitiated);
                        loadUrlParams.setInitiatorOrigin(initiatorOrigin);
                        currentTab.loadUrl(loadUrlParams);
                    } else {
                        launchIntent(url, referer, headers, externalAppId, true,
                                isRendererInitiated, initiatorOrigin, intent);
                    }
                    break;
                case TabOpenType.REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB:
                    openNewTab(url, referer, headers, externalAppId, isRendererInitiated,
                            initiatorOrigin, intent, false);
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
                            if (tab.getUrlString().equals(url)
                                    || tab.getUrlString().equals(IntentUtils.safeGetStringExtra(
                                            intent, TabOpenType.REUSE_TAB_ORIGINAL_URL_STRING))) {
                                tabModel.setIndex(matchingTabIndex, TabSelectionType.FROM_USER);
                                LoadUrlParams loadUrlParams =
                                        ChromeTabbedActivity.createLoadUrlParamsForIntent(url,
                                                referer, hasUserGesture, mIntentHandlingTimeMs,
                                                intent);
                                loadUrlParams.setVerbatimHeaders(headers);
                                loadUrlParams.setIsRendererInitiated(isRendererInitiated);
                                loadUrlParams.setInitiatorOrigin(initiatorOrigin);
                                tab.loadUrl(loadUrlParams);
                                loaded = true;
                            }
                        }
                        if (!loaded) {
                            openNewTab(url, referer, headers, externalAppId, isRendererInitiated,
                                    initiatorOrigin, intent, false);
                        }
                    }
                    break;
                case TabOpenType.OPEN_NEW_TAB:
                    if (fromLauncherShortcut) {
                        recordLauncherShortcutAction(false);
                        reportNewTabShortcutUsed(false);
                    }

                    openNewTab(url, referer, headers, externalAppId, isRendererInitiated,
                            initiatorOrigin, intent, true);
                    break;
                case TabOpenType.OPEN_NEW_INCOGNITO_TAB:
                    if (!TextUtils.equals(externalAppId, getPackageName())) {
                        assert false : "Only Chrome is allowed to open incognito tabs";
                        Log.e(TAG, "Only Chrome is allowed to open incognito tabs");
                        return;
                    }

                    if (!IncognitoUtils.isIncognitoModeEnabled()) {
                        // The incognito launcher shortcut is manipulated in #onDeferredStartup(),
                        // so it's possible for a user to invoke the shortcut before it's disabled.
                        // Opening an incognito tab while incognito mode is disabled from somewhere
                        // besides the launcher shortcut is an error.
                        if (!fromLauncherShortcut) {
                            assert false : "Tried to open incognito tab while incognito disabled";
                            Log.e(TAG, "Tried to open incognito tab while incognito disabled");
                        }

                        return;
                    }

                    if (url == null || url.equals(UrlConstants.NTP_URL)) {
                        if (fromLauncherShortcut) {
                            getTabCreator(true).launchUrl(
                                    UrlConstants.NTP_URL, TabLaunchType.FROM_LAUNCHER_SHORTCUT);
                            recordLauncherShortcutAction(true);
                            reportNewTabShortcutUsed(true);
                        } else if (IncognitoTabLauncher.didCreateIntent(intent)) {
                            Tab tab = getTabCreator(true).launchUrl(UrlConstants.NTP_URL,
                                    TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB);
                            if (IncognitoTabLauncher.shouldFocusOmnibox(intent)) {
                                // Since the Tab is created in the foreground, its View will gain
                                // focus, and since the Tab and the URL bar are not yet in the same
                                // View hierarchy, setting the URL bar's focus here won't clear the
                                // Tab's focus. When the Tab is added to the hierarchy, we want the
                                // URL bar to retain focus, so we clear the Tab's focus here.
                                tab.getView().clearFocus();
                                focus = true;
                            }

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

            getToolbarManager().setUrlBarFocusOnceNativeInitialized(focus,
                    focus ? OmniboxFocusReason.LAUNCH_NEW_INCOGNITO_TAB
                          : OmniboxFocusReason.UNFOCUS);

            if (tabModel.getCount() > 0 && isInOverviewMode() && !isTablet()
                    && !shouldShowTabSwitcherOnStart()) {
                mOverviewModeController.hideOverview(true);
            }
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

        /**
         * Opens a new Tab with the possibility of showing it in a Custom Tab, instead.
         *
         * See IntentHandler#processUrlViewIntent() for an explanation most of the parameters.
         * @param forceNewTab If not handled by a Custom Tab, forces the new tab to be created.
         */
        private void openNewTab(String url, String referer, String headers, String externalAppId,
                boolean isRendererInitiated, @Nullable Origin initiatorOrigin, Intent intent,
                boolean forceNewTab) {
            // Create a new tab.
            launchIntent(url, referer, headers, externalAppId, forceNewTab, isRendererInitiated,
                    initiatorOrigin, intent);
            logMobileReceivedExternalIntent(externalAppId, intent);
        }

        // TODO(tedchoc): Remove once we have verified that MobileTabbedModeViewIntentFromChrome
        //                and MobileTabbedModeViewIntentFromApp are suitable/more correct
        //                replacments for these.
        private void logMobileReceivedExternalIntent(String externalAppId, Intent intent) {
            RecordUserAction.record("MobileReceivedExternalIntent");
            if (isFromChrome(intent, externalAppId)) {
                RecordUserAction.record("MobileReceivedExternalIntent.Chrome");
            } else {
                RecordUserAction.record("MobileReceivedExternalIntent.App");
            }
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

        // Decide whether to record startup UMA histograms. This is done  early in the main
        // Activity.onCreate() to avoid recording navigation delays when they require user input to
        // proceed. For example, FRE (First Run Experience) happens before the activity is created,
        // and triggers initialization of the native library.
        //
        // An uninitialized native library is an indication of an application start that is followed
        // by navigation immediately without user input.
        if (!LibraryLoader.getInstance().isInitialized()) {
            setTrackColdStartupMetrics(true);
        }

        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);

        IncognitoTabHostRegistry.getInstance().register(mIncognitoTabHost);

        mStartupPaintPreviewHelperSupplier.attach(getWindowAndroid().getUnownedUserDataHost());
    }

    @Override
    protected RootUiCoordinator createRootUiCoordinator() {
        return new TabbedRootUiCoordinator(this, this::onOmniboxFocusChanged,
                mIntentMetadataOneshotSupplier, getShareDelegateSupplier(),
                getActivityTabProvider(), mEphemeralTabCoordinatorSupplier,
                mTabModelProfileSupplier, mBookmarkBridgeSupplier,
                getOverviewModeBehaviorSupplier(), this::getContextualSearchManager,
                getTabModelSelectorSupplier(), mStartSurfaceSupplier,
                mLayoutStateProviderOneshotSupplier, mStartSurfaceParentTabSupplier);
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.control_container;
    }

    @Override
    public int getControlContainerHeightResource() {
        return R.dimen.control_container_height;
    }

    @Override
    protected int getToolbarLayoutId() {
        return isTablet() ? R.layout.toolbar_tablet : R.layout.toolbar_phone;
    }

    @Override
    public void performPostInflationStartup() {
        super.performPostInflationStartup();

        FontPreloader.getInstance().onPostInflationStartupTabbedActivity();

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
        mControlContainer = (ToolbarControlContainer) findViewById(R.id.control_container);

        Supplier<Boolean> dialogVisibilitySupplier = null;
        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()) {
            dialogVisibilitySupplier = () -> {
                assert mStartSurfaceSupplier.get() != null;
                assert getToolbarManager().getTabGroupUi() != null;
                // Return true if dialog from either tab switcher or tab strip is visible.
                Supplier<Boolean> tabGroupUiDialogVisibilitySupplier =
                        getToolbarManager().getTabGroupUi().getTabGridDialogVisibilitySupplier();
                Supplier<Boolean> tabSwitcherDialogVisibilitySupplier =
                        mStartSurfaceSupplier.get().getTabGridDialogVisibilitySupplier();
                boolean isDialogVisible = false;
                if (tabGroupUiDialogVisibilitySupplier != null) {
                    isDialogVisible = tabGroupUiDialogVisibilitySupplier.get();
                }
                if (tabSwitcherDialogVisibilitySupplier != null) {
                    isDialogVisible = isDialogVisible || tabSwitcherDialogVisibilitySupplier.get();
                }
                return isDialogVisible;
            };
        }

        mUndoBarPopupController = new UndoBarController(this, mTabModelSelectorImpl,
                this::getSnackbarManager, mOverviewModeBehaviorSupplier, dialogVisibilitySupplier);

        mInactivityTracker = new ChromeInactivityTracker(
                ChromePreferenceKeys.TABBED_ACTIVITY_LAST_BACKGROUNDED_TIME_MS_PREF);

        assert getActivityTabStartupMetricsTracker() != null;
        if (StartupPaintPreviewHelper.isEnabled()) {
            mStartupPaintPreviewHelperSupplier.set(new StartupPaintPreviewHelper(getWindowAndroid(),
                    getOnCreateTimestampMs(), getBrowserControlsManager(), getTabModelSelector(),
                    shouldShowTabSwitcherOnStart(), () -> {
                        return getToolbarManager() == null
                                ? null
                                : getToolbarManager().getProgressBarCoordinator();
                    }, getActivityTabStartupMetricsTracker()::pagePreviewRendered));
        }
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

        // When the feature flag {@link ChromeFeatureList.INSTANT_START} turns on phones (not
        // tablet), a view-only start page created on Java will be shown before native is
        // initialized. The {@link prepareToShowStartPagePreNative()} is only called in a cold
        // start.
        if (StartSurfaceConfiguration.isStartSurfaceEnabled()
                && TabUiFeatureUtilities.supportInstantStart(isTablet()) && !hadWarmStart()) {
            prepareToShowStartPagePreNative();
        }
    }

    /**
     * Prepares to show the start page before native is initialized. For example, create
     * an LayoutManagerChrome object, add overview mode observer and so on.
     */
    private void prepareToShowStartPagePreNative() {
        assert TabUiFeatureUtilities.supportInstantStart(isTablet() && !hadWarmStart());
        try (TraceEvent e =
                        TraceEvent.scoped("ChromeTabbedActivity.prepareToShowStartPagePreNative")) {
            setupCompositorContentPreNativeForPhone();
            getCompositorViewHolder().setLayoutManager(mLayoutManager);

            if (shouldShowTabSwitcherOnStart()) {
                mLayoutManager.setTabModelSelector(mTabModelSelectorImpl);
                mIsAccessibilityTabSwitcherEnabled = DeviceClassManager.enableAccessibilityLayout();
                assert !mHasDeterminedOverviewStateForCurrentSession;
                setInitialOverviewState();
            }
        }
    }

    @Override
    protected TabModelOrchestrator createTabModelOrchestrator() {
        mTabModelOrchestrator = new TabbedModeTabModelOrchestrator();
        return mTabModelOrchestrator;
    }

    @Override
    protected void createTabModels() {
        assert mTabModelSelectorImpl == null;

        Bundle savedInstanceState = getSavedInstanceState();

        // We determine the model as soon as possible so every systems get initialized coherently.
        boolean startIncognito = savedInstanceState != null
                && savedInstanceState.getBoolean("is_incognito_selected", false);
        int index = savedInstanceState != null ? savedInstanceState.getInt(WINDOW_INDEX, 0) : 0;

        mNextTabPolicySupplier = new ChromeNextTabPolicySupplier(mOverviewModeBehaviorSupplier);

        boolean tabModelWasCreated =
                mTabModelOrchestrator.createTabModels(this, this, mNextTabPolicySupplier, index);
        if (!tabModelWasCreated) {
            finish();
            return;
        }

        mTabModelSelectorImpl = mTabModelOrchestrator.getTabModelSelector();
        mTabModelSelectorImpl.addObserver(new TabModelSelectorObserver() {
            @Override
            public void onTabStateInitialized() {
                if (!mCreatedTabOnStartup) return;

                TabModel model = mTabModelSelectorImpl.getModel(false);
                TasksUma.recordTasksUma(model);
            }
        });

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelectorImpl) {
            @Override
            public void onDidFinishNavigation(Tab tab, NavigationHandle navigation) {
                if (navigation.hasCommitted() && navigation.isInMainFrame()) {
                    DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(ChromeTabbedActivity.this,
                            tab.getWebContents(), navigation.getUrl(), tab.isShowingErrorPage(),
                            navigation.isFragmentNavigation(), navigation.httpStatusCode());
                }
            }
        };
        mAppIndexingUtil = new AppIndexingUtil(mTabModelSelectorImpl);

        if (startIncognito) mTabModelSelectorImpl.selectModel(true);
    }

    @Override
    protected LaunchCauseMetrics createLaunchCauseMetrics() {
        return new TabbedActivityLaunchCauseMetrics(this);
    }

    @Override
    public AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new TabbedAppMenuPropertiesDelegate(this, getActivityTabProvider(),
                getMultiWindowModeStateDispatcher(), getTabModelSelector(), getToolbarManager(),
                getWindow().getDecorView(), this, mOverviewModeBehaviorSupplier,
                mBookmarkBridgeSupplier, getSnackbarManager(), new WebFeedBridge());
    }

    private TabDelegateFactory getTabDelegateFactory() {
        if (mTabDelegateFactory == null) {
            mTabDelegateFactory = new TabbedModeTabDelegateFactory(this,
                    getAppBrowserControlsVisibilityDelegate(), getShareDelegateSupplier(),
                    mEphemeralTabCoordinatorSupplier,
                    ((TabbedRootUiCoordinator) mRootUiCoordinator)::onContextMenuCopyLink,
                    mRootUiCoordinator.getBottomSheetController(),
                    /* ChromeActivityNativeDelegate */ this, /* isCustomTab= */ false,
                    getBrowserControlsManager(), getFullscreenManager(),
                    /* TabCreatorManager */ this, getTabModelSelectorSupplier(),
                    this::getCompositorViewHolder, getModalDialogManagerSupplier());
        }
        return mTabDelegateFactory;
    }

    @Override
    protected Pair<ChromeTabCreator, ChromeTabCreator> createTabCreators() {
        ChromeTabCreator.OverviewNTPCreator overviewNTPCreator = null;

        if (StartSurfaceConfiguration.isStartSurfaceEnabled()) {
            overviewNTPCreator = new ChromeTabCreator.OverviewNTPCreator() {
                @Override
                public boolean handleCreateNTPIfNeeded(
                        boolean isNTP, boolean incognito, Tab parentTab) {
                    boolean shouldShowStart =
                            showStartSurfaceHomeForNTP(isNTP, incognito, parentTab);
                    if (shouldShowStart) {
                        mStartSurfaceParentTabSupplier.set(parentTab);
                    }
                    return shouldShowStart;
                }

                @Override
                public void preTabInitialization(Tab tab, String url) {
                    StartSurfaceConfiguration.maySetUserDataForEmptyTab(tab, url);
                }
            };
        }
        return Pair.create(
                new ChromeTabCreator(this, getWindowAndroid(), getStartupTabPreloader(),
                        this::getTabDelegateFactory, false, overviewNTPCreator,
                        AsyncTabParamsManagerSingleton.getInstance(), getTabModelSelectorSupplier(),
                        getCompositorViewHolderSupplier()),
                new ChromeTabCreator(this, getWindowAndroid(), getStartupTabPreloader(),
                        this::getTabDelegateFactory, true, overviewNTPCreator,
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

            ChromeSurveyController.initialize(mTabModelSelectorImpl, getLifecycleDispatcher());

            if (mStartSurfaceSupplier.get() != null && mOverviewShownOnStart) {
                mStartSurfaceSupplier.get().onOverviewShownAtLaunch(getOnCreateTimestampMs());
            }
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
        if (mMultiInstanceManager != null
                && !mMultiInstanceManager.isStartedUpCorrectly(getTaskId())) {
            return false;
        }

        return super.isStartedUpCorrectly(intent);
    }

    @Override
    public void terminateIncognitoSession() {
        getTabModelSelector().getModel(true).closeAllTabs();
    }

    @Override
    public boolean onMenuOrKeyboardAction(final int id, boolean fromMenu) {
        final Tab currentTab = getActivityTab();
        boolean currentTabIsNtp = isTabNtp(currentTab);
        if (id == R.id.new_tab_menu_id) {
            getTabModelSelector().getModel(false).commitAllTabClosures();
            RecordUserAction.record("MobileMenuNewTab");
            RecordUserAction.record("MobileNewTabOpened");
            reportNewTabShortcutUsed(false);
            if (fromMenu) RecordUserAction.record("MobileMenuNewTab.AppMenu");

            getTabCreator(false).launchNTP();

            mLocaleManager.showSearchEnginePromoIfNeeded(this, null);
        } else if (id == R.id.new_incognito_tab_menu_id) {
            if (IncognitoUtils.isIncognitoModeEnabled()) {
                getTabModelSelector().getModel(false).commitAllTabClosures();
                // This action must be recorded before opening the incognito tab since UMA actions
                // are dropped when an incognito tab is open.
                RecordUserAction.record("MobileMenuNewIncognitoTab");
                RecordUserAction.record("MobileNewTabOpened");
                reportNewTabShortcutUsed(true);
                if (fromMenu) RecordUserAction.record("MobileMenuNewIncognitoTab.AppMenu");
                getTabCreator(true).launchNTP();
            }
        } else if (id == R.id.all_bookmarks_menu_id) {
            // Note that 'currentTab' could be null in overview mode when start surface is
            // enabled.
            getCompositorViewHolder().hideKeyboard(() -> {
                BookmarkUtils.showBookmarkManager(
                        ChromeTabbedActivity.this, getCurrentTabModel().isIncognito());
            });
            if (currentTabIsNtp) {
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_BOOKMARKS_MANAGER);
            }

            @BrowserProfileType
            int type = getCurrentTabModel().isIncognito() ? BrowserProfileType.INCOGNITO
                                                          : BrowserProfileType.REGULAR;
            RecordHistogram.recordEnumeratedHistogram(
                    "Bookmarks.OpenBookmarkManager.PerProfileType", type,
                    BrowserProfileType.MAX_VALUE + 1);

            RecordUserAction.record("MobileMenuAllBookmarks");
        } else if (id == R.id.recent_tabs_menu_id) {
            LoadUrlParams params =
                    new LoadUrlParams(UrlConstants.RECENT_TABS_URL, PageTransition.AUTO_BOOKMARK);
            if (currentTab != null) {
                currentTab.loadUrl(params);
            } else {
                // Note that 'currentTab' could be null in overview mode when start surface is
                // enabled.
                getTabCreator(getCurrentTabModel().isIncognito())
                        .createNewTab(params, TabLaunchType.FROM_CHROME_UI, null);
            }
            if (isInOverviewMode() && !isTablet()) {
                mOverviewModeController.hideOverview(true);
            }

            if (currentTabIsNtp) {
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_RECENT_TABS_MANAGER);
            }
            RecordUserAction.record("MobileMenuRecentTabs");
        } else if (id == R.id.close_tab) {
            getCurrentTabModel().closeTab(currentTab, true, false, true);
            RecordUserAction.record("MobileTabClosed");
        } else if (id == R.id.close_all_tabs_menu_id) {
            // Close both incognito and normal tabs
            getTabModelSelector().closeAllTabs();
            RecordUserAction.record("MobileMenuCloseAllTabs");
        } else if (id == R.id.close_all_incognito_tabs_menu_id) {
            // Close only incognito tabs
            getTabModelSelector().getModel(true).closeAllTabs();
            RecordUserAction.record("MobileMenuCloseAllIncognitoTabs");
        } else if (id == R.id.focus_url_bar) {
            boolean isUrlBarVisible = !mOverviewModeController.overviewVisible()
                    && (!isTablet() || getCurrentTabModel().getCount() != 0);
            if (isUrlBarVisible) {
                getToolbarManager().setUrlBarFocus(
                        true, OmniboxFocusReason.MENU_OR_KEYBOARD_ACTION);
            }
        } else if (id == R.id.downloads_menu_id) {
            OTRProfileID otrProfileID = null;
            if (currentTab != null && currentTab.getWebContents() != null) {
                Profile profile = Profile.fromWebContents(currentTab.getWebContents());
                otrProfileID = profile != null ? profile.getOTRProfileID() : null;
            }
            DownloadUtils.showDownloadManager(
                    this, currentTab, otrProfileID, DownloadOpenSource.MENU);
            if (currentTabIsNtp) {
                NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_DOWNLOADS_MANAGER);
            }
            RecordUserAction.record("MobileMenuDownloadManager");
        } else if (id == R.id.open_recently_closed_tab) {
            TabModel currentModel = mTabModelSelectorImpl.getCurrentModel();
            if (!currentModel.isIncognito()) currentModel.openMostRecentlyClosedTab();
            RecordUserAction.record("MobileTabClosedUndoShortCut");
        } else if (id == R.id.enter_vr_id) {
            VrModuleProvider.getDelegate().enterVrIfNecessary();
        } else {
            return super.onMenuOrKeyboardAction(id, fromMenu);
        }
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

        // TODO(1091411): Find a better mechanism for back-press handling for features.
        if (mRootUiCoordinator.getBottomSheetController().handleBackPress()) return true;

        if (mTabModalHandler.handleBackPress()) return true;

        final Tab currentTab = getActivityTab();
        if (currentTab == null) {
            moveTaskToBack(true);
            return true;
        }

        // If we are in the tab switcher mode (not in the Start surface homepage) and not a tablet,
        // then leave tab switcher mode on back.
        if (mOverviewModeController.overviewVisible() && !isTablet()
                && (mStartSurfaceSupplier.get() == null
                        || mStartSurfaceSupplier.get().getController().getStartSurfaceState()
                                == StartSurfaceState.SHOWN_TABSWITCHER)) {
            mOverviewModeController.hideOverview(true);
            return true;
        }

        final WebContents webContents = currentTab.getWebContents();
        if (webContents != null) {
            RenderFrameHost focusedFrame = webContents.getFocusedFrame();
            if (focusedFrame != null && focusedFrame.signalModalCloseWatcherIfActive()) return true;
        }

        if (getToolbarManager().back()) return true;

        // If the current tab url is HELP_URL, then the back button should close the tab to
        // get back to the previous state. The reason for startsWith check is that the
        // actual redirected URL is a different system language based help url.
        final @TabLaunchType int type = currentTab.getLaunchType();
        final boolean helpUrl = currentTab.getUrl().getSpec().startsWith(HELP_URL_PREFIX);
        if (type == TabLaunchType.FROM_CHROME_UI && helpUrl) {
            getCurrentTabModel().closeTab(currentTab);
            return true;
        }

        // If we aren't in the overview mode, we handle the Tab with launchType
        // TabLaunchType.FROM_START_SURFACE here.
        if (!mOverviewModeController.overviewVisible()
                && type == TabLaunchType.FROM_START_SURFACE) {
            if (StartSurfaceUserData.getKeepTab(currentTab)) {
                // If the current tab is created from the start surface with the keepTab property,
                // shows the Start surface Homepage to prevent a loop between the current tab and
                // previous overview mode. Once in the Start surface, it will close Chrome if back
                // button is tapped again.
                showOverview(StartSurfaceState.SHOWING_HOMEPAGE);
            } else {
                // Otherwise, clicking the back button should close the tab and go back to the
                // previous overview mode.
                showOverview(StartSurfaceState.SHOWING_PREVIOUS);
                getCurrentTabModel().closeTab(currentTab);
            }
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
            currentTab.getWebContents().dispatchBeforeUnload(false);
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
                || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
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
                boolean hasNextTab =
                        getCurrentTabModel().getNextTabIfClosed(tabToClose.getId()) != null;
                getCurrentTabModel().closeTab(tabToClose, false, true, false);

                // If there is no next tab to open, enter overview mode.
                if (!hasNextTab) showOverview(StartSurfaceState.SHOWING_START);
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
     * Create a LoadUrlParams for handling a VIEW intent.
     */
    private static LoadUrlParams createLoadUrlParamsForIntent(String url, String referer,
            boolean hasUserGesture, long intentHandlingTimeMs, Intent intent) {
        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setIntentReceivedTimestamp(intentHandlingTimeMs);
        loadUrlParams.setHasUserGesture(hasUserGesture);
        // Add FROM_API to ensure intent handling isn't used again. Without FROM_API Chrome could
        // get stuck in a loop continually being asked to open a link, and then calling out to the
        // system.
        int transitionType = PageTransition.LINK | PageTransition.FROM_API;
        loadUrlParams.setTransitionType(
                IntentHandler.getTransitionTypeFromIntent(intent, transitionType));
        if (referer != null) {
            loadUrlParams.setReferrer(
                    new Referrer(referer, IntentHandler.getReferrerPolicyFromIntent(intent)));
        }
        return loadUrlParams;
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
    private Tab launchIntent(String url, String referer, String headers, String externalAppId,
            boolean forceNewTab, boolean isRendererInitiated, @Nullable Origin initiatorOrigin,
            Intent intent) {
        if (mUIWithNativeInitialized && !UrlUtilities.isNTPUrl(url)) {
            mOverviewModeController.hideOverview(false);
            getToolbarManager().finishAnimations();
        }
        if (TextUtils.equals(externalAppId, getPackageName())) {
            // If the intent was launched by chrome, open the new tab in the appropriate model.
            // Using FROM_LINK ensures the tab is parented to the current tab, which allows
            // the back button to close these tabs and restore selection to the previous tab.
            boolean isIncognito = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
            LoadUrlParams loadUrlParams = new LoadUrlParams(url);
            loadUrlParams.setIntentReceivedTimestamp(mIntentHandlingTimeMs);
            loadUrlParams.setVerbatimHeaders(headers);
            loadUrlParams.setIsRendererInitiated(isRendererInitiated);
            loadUrlParams.setInitiatorOrigin(initiatorOrigin);
            @TabLaunchType
            Integer launchType = IntentHandler.getTabLaunchType(intent);
            if (launchType == null) {
                if (IntentUtils.safeGetBooleanExtra(
                            intent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false)) {
                    launchType = TabLaunchType.FROM_LAUNCHER_SHORTCUT;
                } else if (IncognitoTabLauncher.didCreateIntent(intent)) {
                    launchType = TabLaunchType.FROM_LAUNCH_NEW_INCOGNITO_TAB;
                } else {
                    launchType = TabLaunchType.FROM_LINK;
                }
            }
            return getTabCreator(isIncognito).createNewTab(loadUrlParams, launchType, null, intent);
        } else {
            // Check if the tab is being created from a Reader Mode navigation.
            if (ReaderModeManager.isEnabled()
                    && ReaderModeManager.isReaderModeCreatedIntent(intent)) {
                Bundle extras = intent.getExtras();
                int readerParentId = IntentUtils.safeGetInt(
                        extras, ReaderModeManager.EXTRA_READER_MODE_PARENT, Tab.INVALID_TAB_ID);
                extras.remove(ReaderModeManager.EXTRA_READER_MODE_PARENT);
                // Set the parent tab to the tab that Reader Mode started from.
                if (readerParentId != Tab.INVALID_TAB_ID && mTabModelSelectorImpl != null) {
                    return getCurrentTabCreator().createNewTab(
                            new LoadUrlParams(url, PageTransition.LINK), TabLaunchType.FROM_LINK,
                            mTabModelSelectorImpl.getTabById(readerParentId));
                }
            }

            return getTabCreator(false).launchUrlFromExternalApp(url, referer, headers,
                    externalAppId, forceNewTab, intent, mIntentHandlingTimeMs);
        }
    }

    // TODO(crbug.com/1115757): After crrev.com/c/2315823, Overview state and Startsurface state are
    // two different things, we actual can split this into two methods: showOverview() and
    // showStartSurface(state). Let's do some auditing and clean up before perform the actual split.
    private void showOverview(@StartSurfaceState int state) {
        assert (state == StartSurfaceState.SHOWING_TABSWITCHER
                || state == StartSurfaceState.SHOWING_HOMEPAGE
                || state == StartSurfaceState.SHOWING_PREVIOUS
                || state == StartSurfaceState.SHOWING_START);
        if (mIsAccessibilityTabSwitcherEnabled != null && mIsAccessibilityTabSwitcherEnabled
                && mOverviewModeController != null) {
            // TODO(1200727): This is a temporary fix that should be removed once grid tab switcher
            //                is completely launched. The "start surface" is now created regardless
            //                of the state of accessibility, so we check that mode first and try
            //                showing the overview list before going to the start surface.
            mOverviewModeController.showOverview(false);
        } else if (mStartSurfaceSupplier.get() != null) {
            if (StartSurfaceConfiguration.shouldHideStartSurfaceWithAccessibilityOn()) {
                state = StartSurfaceState.SHOWING_TABSWITCHER;
            }
            mStartSurfaceSupplier.get().getController().setOverviewState(state);
        }

        if (mOverviewModeController == null) return;

        if (mOverviewModeController.overviewVisible()) {
            if (didFinishNativeInitialization()) {
                getCompositorViewHolder().hideKeyboard(() -> {});
            }
            return;
        }

        Tab currentTab = getActivityTab();
        // If we don't have a current tab, show the overview mode.
        if (currentTab == null) {
            mOverviewModeController.showOverview(false);
        } else {
            getCompositorViewHolder().hideKeyboard(
                    () -> mOverviewModeController.showOverview(true));
            updateAccessibilityState(false);
            TasksUma.recordTabLaunchType(getCurrentTabModel());
        }
    }

    private void hideOverview() {
        assert (mOverviewModeController.overviewVisible());
        if (getCurrentTabModel().getCount() != 0) {
            // Don't hide overview if current tab stack is empty()
            mOverviewModeController.hideOverview(true);
            updateAccessibilityState(true);
        }
    }

    /**
     * @return Whether opening a new tab is handled by the Start surface. It may show the Start
     * surface, or open a new tab with the omnibox get focused, depending on the value of
     * {@link StartSurfaceConfiguration.OMNIBOX_FOCUSED_ON_NEW_TAB}.
     */
    private boolean showStartSurfaceHomeForNTP(boolean isNTP, boolean incognito, Tab parentTab) {
        if (!isNTP
                || !ReturnToChromeExperimentsUtil.shouldShowStartSurfaceHomeAsNTP(
                        incognito, isTablet())) {
            return false;
        }

        getTabModelSelector().selectModel(incognito);
        if (StartSurfaceConfiguration.OMNIBOX_FOCUSED_ON_NEW_TAB.getValue()) {
            Runnable emptyTabCloseCallback = isInOverviewMode() ? () -> {
                showOverview(StartSurfaceState.SHOWING_PREVIOUS);
            } : null;
            ReturnToChromeExperimentsUtil.handleLoadUrlFromStartSurfaceAsNewTab(null,
                    PageTransition.AUTO_TOPLEVEL, incognito, parentTab, getCurrentTabModel(),
                    emptyTabCloseCallback);
        } else if (TabUiFeatureUtilities.supportInstantStart(isTablet())
                || (getTabModelSelector().isTabStateInitialized() && isLayoutManagerCreated())) {
            showOverview(StartSurfaceState.SHOWING_HOMEPAGE);
        }
        return true;
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
        outState.putBoolean("is_incognito_selected", getCurrentTabModel().isIncognito());
        outState.putInt(
                WINDOW_INDEX, TabWindowManagerSingleton.getInstance().getIndexForWindow(this));
    }

    @Override
    public void onDestroyInternal() {
        if (mCallbackController != null) {
            mCallbackController.destroy();
            mCallbackController = null;
        }

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        if (mTabModelObserver != null) mTabModelObserver.destroy();

        if (mUndoBarPopupController != null) {
            mUndoBarPopupController.destroy();
            mUndoBarPopupController = null;
        }

        if (mAppIndexingUtil != null) {
            mAppIndexingUtil.destroy();
            mAppIndexingUtil = null;
        }

        if (mStartSurfaceSupplier.get() != null) {
            mStartSurfaceSupplier.get().destroy();
        }

        if (mStartupPaintPreviewHelperSupplier != null) {
            mStartupPaintPreviewHelperSupplier.destroy();
        }

        IncognitoTabHostRegistry.getInstance().unregister(mIncognitoTabHost);

        TabObscuringHandler tabObscuringHandler = getTabObscuringHandler();
        if (tabObscuringHandler != null) {
            getTabObscuringHandler().removeObserver(mCompositorViewHolder);
            getTabObscuringHandler().removeObserver(mOverviewListLayout);
        }

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
        if (ChromeApplicationImpl.isSevereMemorySignal(level)) {
            NativePageAssassin.getInstance().freezeAllHiddenPages();
        }
    }

    @Override
    public boolean dispatchKeyEvent(KeyEvent event) {
        Boolean result = KeyboardShortcuts.dispatchKeyEvent(event, this, mUIWithNativeInitialized);
        return result != null ? result : super.dispatchKeyEvent(event);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        if (!mUIWithNativeInitialized) {
            return super.onKeyDown(keyCode, event);
        }
        // Detecting a long press of the back button via onLongPress is broken in Android N.
        // To work around this, use a postDelayed, which is supported in all versions.
        if (keyCode == KeyEvent.KEYCODE_BACK && !isTablet()
                && !getFullscreenManager().getPersistentFullscreenMode()) {
            if (mShowHistoryRunnable == null) mShowHistoryRunnable = this::showFullHistorySheet;
            mHandler.postDelayed(mShowHistoryRunnable, ViewConfiguration.getLongPressTimeout());
            return super.onKeyDown(keyCode, event);
        }
        boolean isCurrentTabVisible = !mOverviewModeController.overviewVisible()
                && (!isTablet() || getCurrentTabModel().getCount() != 0);
        return KeyboardShortcuts.onKeyDown(event, this, isCurrentTabVisible, true)
                || super.onKeyDown(keyCode, event);
    }

    private void showFullHistorySheet() {
        ((TabbedRootUiCoordinator) mRootUiCoordinator).showFullHistorySheet();
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && !isTablet()) {
            mHandler.removeCallbacks(mShowHistoryRunnable);
            mShowHistoryRunnable = null;
            if (event.getEventTime() - event.getDownTime()
                            >= ViewConfiguration.getLongPressTimeout()
                    && NavigationSheet.isInstanceShowing(
                            mRootUiCoordinator.getBottomSheetController())) {
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
        return getCompositorViewHolder();
    }

    @VisibleForTesting
    public LayoutManagerChrome getLayoutManager() {
        return (LayoutManagerChrome) getCompositorViewHolder().getLayoutManager();
    }

    @VisibleForTesting
    public Layout getOverviewListLayout() {
        return getLayoutManager().getOverviewListLayout();
    }

    @VisibleForTesting
    public StartSurface getStartSurface() {
        return mStartSurfaceSupplier.get();
    }

    private ComposedBrowserControlsVisibilityDelegate getAppBrowserControlsVisibilityDelegate() {
        // TODO(jinsukkim): Move this to RootUiCoordinator.
        return ((TabbedRootUiCoordinator) mRootUiCoordinator)
                .getAppBrowserControlsVisibilityDelegate();
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        ModalDialogManager manager = super.createModalDialogManager();
        // TODO(crbug.com/1157310): Transition this::method refs to dedicated suppliers.
        mTabModalHandler = new TabModalLifetimeHandler(this, getLifecycleDispatcher(), manager,
                this::getAppBrowserControlsVisibilityDelegate, this::getTabObscuringHandler,
                this::getToolbarManager, this::getContextualSearchManager,
                getTabModelSelectorSupplier(), this::getBrowserControlsManager,
                this::getFullscreenManager);
        return manager;
    }

    // App Menu related code -----------------------------------------------------------------------

    @Override
    public boolean canShowAppMenu() {
        // The popup menu relies on the model created during the full UI initialization, so do not
        // attempt to show the menu until the UI creation has finished.
        if (!mUIWithNativeInitialized) return false;

        // If the current active tab is showing a tab modal dialog, an app menu shouldn't be shown
        // in any cases, e.g. when a hardware menu button is clicked.
        Tab tab = getActivityTab();
        if (tab != null && ChromeTabModalPresenter.isDialogShowing(tab)) return false;

        return super.canShowAppMenu();
    }

    @Override
    public boolean isInOverviewMode() {
        return mOverviewModeController != null && mOverviewModeController.overviewVisible();
    }

    @Override
    protected IntentHandlerDelegate createIntentHandlerDelegate() {
        return new InternalIntentDelegate();
    }

    @Override
    public void onSceneChange(Layout layout) {
        super.onSceneChange(layout);
        if (!layout.shouldDisplayContentOverlay()) mTabModelSelectorImpl.onTabsViewShown();
    }

    /**
     * Writes the tab state to disk.
     */
    @VisibleForTesting
    public void saveState() {
        mTabModelOrchestrator.saveState();
    }

    @Override
    public void onEnterVr() {
        super.onEnterVr();
        mControlContainer.setVisibility(View.INVISIBLE);
        if (mVrBrowserControlsVisibilityDelegate == null) {
            mVrBrowserControlsVisibilityDelegate =
                    new BrowserControlsVisibilityDelegate(BrowserControlsState.BOTH);
            getAppBrowserControlsVisibilityDelegate().addDelegate(
                    mVrBrowserControlsVisibilityDelegate);
        }
        mVrBrowserControlsVisibilityDelegate.set(BrowserControlsState.HIDDEN);
    }

    @Override
    public void onExitVr() {
        super.onExitVr();
        mControlContainer.setVisibility(View.VISIBLE);
        if (mVrBrowserControlsVisibilityDelegate != null) {
            mVrBrowserControlsVisibilityDelegate.set(BrowserControlsState.BOTH);
        }
    }

    /**
     * Reports that a new tab launcher shortcut was selected or an action equivalent to a shortcut
     * was performed.
     * @param isIncognito Whether the shortcut or action created a new incognito tab.
     */
    @TargetApi(Build.VERSION_CODES.N_MR1)
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
