// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.annotation.SuppressLint;
import android.annotation.TargetApi;
import android.app.Activity;
import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.app.ActivityManager.RecentTaskInfo;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.ApplicationInfo;
import android.content.pm.ShortcutManager;
import android.graphics.Color;
import android.os.Build;
import android.os.Bundle;
import android.os.SystemClock;
import android.support.annotation.IntDef;
import android.support.annotation.Nullable;
import android.text.TextUtils;
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
import android.widget.FrameLayout;

import org.chromium.base.ActivityState;
import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.MemoryPressureListener;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.VisibleForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.CachedMetrics.BooleanHistogramSample;
import org.chromium.base.metrics.CachedMetrics.EnumeratedHistogramSample;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.appmenu.AppMenu;
import org.chromium.chrome.browser.appmenu.AppMenuIconRowFooter;
import org.chromium.chrome.browser.appmenu.AppMenuPropertiesDelegate;
import org.chromium.chrome.browser.bookmarks.BookmarkUtils;
import org.chromium.chrome.browser.browseractions.BrowserActionsService;
import org.chromium.chrome.browser.browseractions.BrowserActionsTabModelSelector;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManager;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChrome;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromePhone;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerChromeTablet;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior.OverviewModeObserver;
import org.chromium.chrome.browser.compositor.layouts.phone.StackLayout;
import org.chromium.chrome.browser.contextual_suggestions.PageViewTimer;
import org.chromium.chrome.browser.cookies.CookiesFetcher;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.device.DeviceClassManager;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.chrome.browser.document.DocumentUtils;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.download.DownloadUtils;
import org.chromium.chrome.browser.feature_engagement.ScreenshotMonitor;
import org.chromium.chrome.browser.feature_engagement.ScreenshotMonitorDelegate;
import org.chromium.chrome.browser.feature_engagement.ScreenshotTabObserver;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.feed.FeedProcessScopeFactory;
import org.chromium.chrome.browser.firstrun.FirstRunSignInProcessor;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.fullscreen.ComposedBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.incognito.IncognitoNotificationManager;
import org.chromium.chrome.browser.incognito.IncognitoTabHost;
import org.chromium.chrome.browser.incognito.IncognitoTabHostRegistry;
import org.chromium.chrome.browser.incognito.IncognitoTabSnapshotController;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.infobar.DataReductionPromoInfoBar;
import org.chromium.chrome.browser.language.LanguageAskPrompt;
import org.chromium.chrome.browser.locale.LocaleManager;
import org.chromium.chrome.browser.metrics.ActivityStopMetrics;
import org.chromium.chrome.browser.metrics.LaunchMetrics;
import org.chromium.chrome.browser.metrics.MainIntentBehaviorMetrics;
import org.chromium.chrome.browser.modaldialog.ModalDialogManager;
import org.chromium.chrome.browser.modaldialog.TabModalLifetimeHandler;
import org.chromium.chrome.browser.multiwindow.MultiInstanceChromeTabbedActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.native_page.NativePageAssassin;
import org.chromium.chrome.browser.net.spdyproxy.DataReductionProxySettings;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ntp.NewTabPageUma;
import org.chromium.chrome.browser.omaha.OmahaBase;
import org.chromium.chrome.browser.omnibox.suggestions.AutocompleteController;
import org.chromium.chrome.browser.partnercustomizations.HomepageManager;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionMainMenuItem;
import org.chromium.chrome.browser.preferences.datareduction.DataReductionPromoScreen;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.SigninPromoUtil;
import org.chromium.chrome.browser.snackbar.undo.UndoBarController;
import org.chromium.chrome.browser.suggestions.SuggestionsEventReporterBridge;
import org.chromium.chrome.browser.suggestions.SuggestionsMetrics;
import org.chromium.chrome.browser.survey.ChromeSurveyController;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabRedirectHandler;
import org.chromium.chrome.browser.tab.TabStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.ChromeTabCreator;
import org.chromium.chrome.browser.tabmodel.EmptyTabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModel.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorImpl;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tabmodel.TabWindowManager;
import org.chromium.chrome.browser.tasks.TasksUma;
import org.chromium.chrome.browser.toolbar.ToolbarButtonInProductHelpController;
import org.chromium.chrome.browser.toolbar.ToolbarControlContainer;
import org.chromium.chrome.browser.util.AccessibilityUtil;
import org.chromium.chrome.browser.util.FeatureUtilities;
import org.chromium.chrome.browser.util.IntentUtils;
import org.chromium.chrome.browser.vr.VrModuleProvider;
import org.chromium.chrome.browser.widget.OverviewListLayout;
import org.chromium.chrome.browser.widget.emptybackground.EmptyBackgroundViewWrapper;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.feature_engagement.FeatureConstants;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.content_public.common.Referrer;
import org.chromium.ui.UiUtils;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.widget.Toast;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.List;
import java.util.Locale;
import java.util.concurrent.TimeUnit;

/**
 * This is the main activity for ChromeMobile when not running in document mode.  All the tabs
 * are accessible via a chrome specific tab switching UI.
 */
public class ChromeTabbedActivity
        extends ChromeActivity implements OverviewModeObserver, ScreenshotMonitorDelegate {
    @IntDef({BackPressedResult.NOTHING_HAPPENED, BackPressedResult.HELP_URL_CLOSED,
            BackPressedResult.MINIMIZED_NO_TAB_CLOSED, BackPressedResult.MINIMIZED_TAB_CLOSED,
            BackPressedResult.TAB_CLOSED, BackPressedResult.TAB_IS_NULL,
            BackPressedResult.EXITED_TAB_SWITCHER, BackPressedResult.EXITED_FULLSCREEN,
            BackPressedResult.NAVIGATED_BACK})
    @Retention(RetentionPolicy.SOURCE)
    private @interface BackPressedResult {
        int NOTHING_HAPPENED = 0;
        int HELP_URL_CLOSED = 1;
        int MINIMIZED_NO_TAB_CLOSED = 2;
        int MINIMIZED_TAB_CLOSED = 3;
        int TAB_CLOSED = 4;
        int TAB_IS_NULL = 5;
        int EXITED_TAB_SWITCHER = 6;
        int EXITED_FULLSCREEN = 7;
        int NAVIGATED_BACK = 8;

        int NUM_ENTRIES = 9;
    }

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
     * Sending an intent with this extra sets the app into single process mode.
     * This is only used for testing, when certain tests want to force this behaviour.
     */
    public static final String INTENT_EXTRA_TEST_RENDER_PROCESS_LIMIT = "render_process_limit";

    /**
     * Sending an intent with this action to Chrome will cause it to close all tabs
     * (iff the --enable-test-intents command line flag is set). If a URL is supplied in the
     * intent data, this will be loaded and unaffected by the close all action.
     */
    private static final String ACTION_CLOSE_TABS =
            "com.google.android.apps.chrome.ACTION_CLOSE_TABS";

    @VisibleForTesting
    public static final String LAST_BACKGROUNDED_TIME_MS_PREF =
            "ChromeTabbedActivity.BackgroundTimeMs";
    private static final String NTP_LAUNCH_DELAY_IN_MINS_PARAM = "delay_in_mins";

    @VisibleForTesting
    public static final String STARTUP_UMA_HISTOGRAM_SUFFIX = ".Tabbed";

    /** The task id of the activity that tabs were merged into. */
    private static int sMergedInstanceTaskId;

    // Name of the ChromeTabbedActivity alias that handles MAIN intents.
    public static final String MAIN_LAUNCHER_ACTIVITY_NAME = "com.google.android.apps.chrome.Main";

    // Boolean histograms used with maybeDispatchExplicitMainViewIntent().
    private static final BooleanHistogramSample sExplicitMainViewIntentDispatchedOnCreate =
            new BooleanHistogramSample(
                    "Android.MainActivity.ExplicitMainViewIntentDispatched.OnCreate");
    private static final BooleanHistogramSample sExplicitMainViewIntentDispatchedOnNewIntent =
            new BooleanHistogramSample(
                    "Android.MainActivity.ExplicitMainViewIntentDispatched.OnNewIntent");
    private static final EnumeratedHistogramSample sUndispatchedExplicitMainViewIntentSource =
            new EnumeratedHistogramSample(
                    "Android.MainActivity.UndispatchedExplicitMainViewIntentSource",
                    IntentHandler.ExternalAppId.NUM_ENTRIES);

    private final ActivityStopMetrics mActivityStopMetrics;
    private final MainIntentBehaviorMetrics mMainIntentMetrics;

    private UndoBarController mUndoBarPopupController;

    private LayoutManagerChrome mLayoutManager;

    private ViewGroup mContentContainer;

    private ToolbarControlContainer mControlContainer;

    private TabModelSelectorImpl mTabModelSelectorImpl;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;
    private TabModelSelectorTabModelObserver mTabModelObserver;

    private ScreenshotMonitor mScreenshotMonitor;

    private TabModalLifetimeHandler mTabModalHandler;

    private NavigationBarColorController mNavigationBarColorController;

    private boolean mUIWithNativeInitialized;

    private Boolean mMergeTabsOnResume;

    private Boolean mIsAccessibilityTabSwitcherEnabled;

    private LocaleManager mLocaleManager;

    private AppIndexingUtil mAppIndexingUtil;

    private Runnable mShowHistoryRunnable;
    private NavigationPopup mNavigationPopup;

    /**
     * Keeps track of whether or not a specific tab was created based on the startup intent.
     */
    private boolean mCreatedTabOnStartup;

    // Whether or not chrome was launched with an intent to open a tab.
    private boolean mIntentWithEffect;

    // Time at which an intent was received and handled.
    private long mIntentHandlingTimeMs;

    private final IncognitoTabHost mIncognitoTabHost = new IncognitoTabHost() {

        @Override
        public boolean hasIncognitoTabs() {
            return getTabModelSelector().getModel(true).getCount() > 0;
        }

        @Override
        public void closeAllIncognitoTabs() {
            if (isActivityDestroyed()) return;

            // If the tabbed activity has not yet initialized, then finish the activity to avoid
            // timing issues with clearing the incognito tab state in the background.
            if (!areTabModelsInitialized() || !didFinishNativeInitialization()) {
                finish();
                return;
            }

            getTabModelSelector().getModel(true).closeAllTabs(false, false);
        }
    };

    private class TabbedAssistStatusHandler extends AssistStatusHandler {
        public TabbedAssistStatusHandler(Activity activity) {
            super(activity);
        }

        @Override
        public boolean isAssistSupported() {
            // If we are in the tab switcher and any incognito tabs are present, disable assist.
            if (isInOverviewMode() && mTabModelSelectorImpl != null
                    && mTabModelSelectorImpl.getModel(true).getCount() > 0) {
                return false;
            }
            return super.isAssistSupported();
        }
    }

    // TODO(mthiesse): Move VR control visibility handling into ChromeActivity. crbug.com/688611
    private static class TabbedModeBrowserControlsVisibilityDelegate
            extends TabStateBrowserControlsVisibilityDelegate {
        public TabbedModeBrowserControlsVisibilityDelegate(Tab tab) {
            super(tab);
        }

        @Override
        public boolean canShowBrowserControls() {
            if (VrModuleProvider.getDelegate().isInVr()) return false;
            return super.canShowBrowserControls();
        }

        @Override
        public boolean canAutoHideBrowserControls() {
            if (VrModuleProvider.getDelegate().isInVr()) return true;
            return super.canAutoHideBrowserControls();
        }
    }

    private class TabbedModeTabDelegateFactory extends TabDelegateFactory {
        @Override
        public BrowserControlsVisibilityDelegate createBrowserControlsVisibilityDelegate(Tab tab) {
            return new ComposedBrowserControlsVisibilityDelegate(
                    new TabbedModeBrowserControlsVisibilityDelegate(tab),
                    getFullscreenManager().getBrowserVisibilityDelegate());
        }
    }

    private class TabbedModeTabCreator extends ChromeTabCreator {
        public TabbedModeTabCreator(
                ChromeTabbedActivity activity, WindowAndroid nativeWindow, boolean incognito) {
            super(activity, nativeWindow, incognito);
        }

        @Override
        public TabDelegateFactory createDefaultTabDelegateFactory() {
            return new TabbedModeTabDelegateFactory();
        }
    }

    /**
     * Return whether the passed in class name matches any of the supported tabbed mode activities.
     */
    public static boolean isTabbedModeClassName(String className) {
        return TextUtils.equals(className, ChromeTabbedActivity.class.getName())
                || TextUtils.equals(className, MultiInstanceChromeTabbedActivity.class.getName())
                || TextUtils.equals(className, ChromeTabbedActivity2.class.getName());
    }

    /**
     * Specify the proper non-.Main-aliased Chrome Activity for the given component.
     *
     * @param intent The intent to set the component for.
     * @param component The client generated component to be validated.
     * @param currentActivity The activity triggering the intent.
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
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP) {
                intent.setClass(appContext, ChromeLauncherActivity.class);
            } else {
                intent.setClass(appContext, ChromeTabbedActivity.class);
            }
        } else {
            intent.setComponent(component);
        }
    }

    /**
     * Constructs a ChromeTabbedActivity.
     */
    public ChromeTabbedActivity() {
        mActivityStopMetrics = new ActivityStopMetrics();
        mMainIntentMetrics = new MainIntentBehaviorMetrics(this);
        mAppIndexingUtil = new AppIndexingUtil();
    }

    @Override
    protected @LaunchIntentDispatcher.Action int maybeDispatchLaunchIntent(Intent intent) {
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
        int action = maybeDispatchExplicitMainViewIntent(
                intent, sExplicitMainViewIntentDispatchedOnCreate);
        if (action != LaunchIntentDispatcher.Action.CONTINUE) {
            return action;
        }
        return super.maybeDispatchLaunchIntent(intent);
    }

    // We know of at least one app that explicitly specifies .Main activity in custom tab
    // intents. The app shouldn't be doing that, but until it's updated, we need to support
    // such use case.
    //
    // This method attempts to treat VIEW intents explicitly sent to .Main as custom tab
    // intents, and dispatch them accordingly. If the intent was not dispatched, the method
    // returns Action.CONTINUE.
    //
    // The method also updates the supplied binary histogram with the dispatching result,
    // but only if the intent is a VIEW intent sent explicitly to .Main activity.
    private @LaunchIntentDispatcher.Action int maybeDispatchExplicitMainViewIntent(
            Intent intent, BooleanHistogramSample dispatchedHistogram) {
        // The first check ensures that this is .Main activity alias (we can't check exactly, but
        // this gets us sufficiently close).
        if (getClass().equals(ChromeTabbedActivity.class)
                && Intent.ACTION_VIEW.equals(intent.getAction()) && intent.getComponent() != null
                && MAIN_LAUNCHER_ACTIVITY_NAME.equals(intent.getComponent().getClassName())) {
            @LaunchIntentDispatcher.Action
            int action = LaunchIntentDispatcher.dispatchToCustomTabActivity(this, intent);
            dispatchedHistogram.record(action != LaunchIntentDispatcher.Action.CONTINUE);
            if (action == LaunchIntentDispatcher.Action.CONTINUE) {
                // Intent was not dispatched, record its source.
                @IntentHandler.ExternalAppId
                int externalId = IntentHandler.determineExternalIntentSource(intent);
                sUndispatchedExplicitMainViewIntentSource.record(externalId);

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

            mTabModelSelectorImpl.onNativeLibraryReady(getTabContentManager());

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
                }

                @Override
                public void didAddTab(Tab tab, @TabLaunchType int type) {
                    if (type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                            && !DeviceClassManager.enableAnimations()) {
                        Toast.makeText(ChromeTabbedActivity.this,
                                R.string.open_in_new_tab_toast,
                                Toast.LENGTH_SHORT).show();
                    }
                }

                @Override
                public void allTabsPendingClosure(List<Tab> tabs) {
                    NewTabPageUma.recordNTPImpression(
                            NewTabPageUma.NTP_IMPESSION_POTENTIAL_NOTAB);
                }
            };
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeCompositor");
        }
    }

    private void refreshSignIn() {
        FirstRunSignInProcessor.start(this);
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
                intentForDispatching, sExplicitMainViewIntentDispatchedOnNewIntent);
        BrowserActionsService.recordTabOpenedNotificationClicked(intent);
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
    public void finishNativeInitialization() {
        try {
            TraceEvent.begin("ChromeTabbedActivity.finishNativeInitialization");

            refreshSignIn();

            initializeUIWithNative();

            // The dataset has already been created, we need to initialize our state.
            mTabModelSelectorImpl.notifyChanged();

            ApiCompatibilityUtils.setWindowIndeterminateProgress(getWindow());

            // Check for incognito tabs to handle the case where Chrome was swiped away in the
            // background.
            if (!IncognitoUtils.doIncognitoTabsExist()) {
                IncognitoNotificationManager.dismissIncognitoNotification();
            }

            ChromePreferenceManager preferenceManager = ChromePreferenceManager.getInstance();
            // Promos can only be shown when we start with ACTION_MAIN intent and
            // after FRE is complete. Native initialization can finish before the FRE flow is
            // complete, and this will only show promos on the second opportunity. This is
            // because the FRE is shown on the first opportunity, and we don't want to show such
            // content back to back.
            //
            // TODO(tedchoc): Unify promo dialog logic as the search engine promo dialog checks
            //                might not have completed at this point and we could show multiple
            //                promos.
            boolean isShowingPromo = mLocaleManager.hasShownSearchEnginePromoThisSession();
            // Promo dialogs in multiwindow mode are broken on some devices: http://crbug.com/354696
            boolean isLegacyMultiWindow = MultiWindowUtils.getInstance().isLegacyMultiWindow(this);
            if (!isShowingPromo && !mIntentWithEffect && FirstRunStatus.getFirstRunFlowComplete()
                    && preferenceManager.readBoolean(
                               ChromePreferenceManager.PROMOS_SKIPPED_ON_FIRST_START, false)
                    && !VrModuleProvider.getDelegate().isInVr()
                    // VrModuleProvider.getDelegate().isInVr may not return true at this point even
                    // though Chrome is about to enter VR, so we need to also check whether we're
                    // launching into VR.
                    && !VrModuleProvider.getIntentDelegate().isLaunchingIntoVr(this, getIntent())
                    && !isLegacyMultiWindow) {
                isShowingPromo = maybeShowPromo();
            } else {
                preferenceManager.writeBoolean(
                        ChromePreferenceManager.PROMOS_SKIPPED_ON_FIRST_START, true);
            }

            if (!isShowingPromo) {
                ToolbarButtonInProductHelpController.maybeShowColdStartIPH(this);
            }

            if (ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)) {
                // We call getFeedAppLifecycle() here to ensure the app lifecycle is created so that
                // it can start listening for state changes.
                FeedProcessScopeFactory.getFeedAppLifecycle();
            }

            super.finishNativeInitialization();
        } finally {
            TraceEvent.end("ChromeTabbedActivity.finishNativeInitialization");
        }
    }

    private boolean maybeShowPromo() {
        // Only one promo can be shown in one run to avoid nagging users too much.
        if (SigninPromoUtil.launchConsentBumpIfNeeded(this)) return true;
        if (SigninPromoUtil.launchSigninPromoIfNeeded(this)) return true;
        if (DataReductionPromoScreen.launchDataReductionPromo(
                    this, mTabModelSelectorImpl.getCurrentModel().isIncognito())) {
            return true;
        }

        return LanguageAskPrompt.maybeShowLanguageAskPrompt(this);
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();

        if (IncognitoUtils.shouldDestroyIncognitoProfileOnStartup()) {
            Profile.getLastUsedProfile().getOffTheRecordProfile().destroyWhenAppropriate();
        } else {
            CookiesFetcher.restoreCookies();
        }

        if (FeatureUtilities.isTabModelMergingEnabled()) {
            boolean inMultiWindowMode = MultiWindowUtils.getInstance().isInMultiWindowMode(this)
                    || MultiWindowUtils.getInstance().isInMultiDisplayMode(this);
            // Don't need to merge tabs when mMergeTabsOnResume is null (cold start) since they get
            // merged when TabPersistentStore.loadState(boolean) is called from initializeState().
            if (!inMultiWindowMode && (mMergeTabsOnResume != null && mMergeTabsOnResume)) {
                maybeMergeTabs();
            } else if (!inMultiWindowMode && mMergeTabsOnResume == null) {
                // This happens on cold start to kill any second activity that might exist.
                killOtherTask();
            }
            mMergeTabsOnResume = false;
        }

        mLocaleManager.setSnackbarManager(getSnackbarManager());
        mLocaleManager.startObservingPhoneChanges();

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_CONTENT_SUGGESTIONS)) {
            if (isWarmOnResume()) {
                SuggestionsEventReporterBridge.onActivityWarmResumed();
            } else {
                SuggestionsEventReporterBridge.onColdStart();
            }
        }

        if (!isWarmOnResume()) {
            SuggestionsMetrics.recordArticlesListVisible();
        }

        maybeStartMonitoringForScreenshots();
    }

    private void maybeStartMonitoringForScreenshots() {
        // Part of the (more runtime-related) check to determine whether to trigger help UI is
        // left until onScreenshotTaken() since it is less expensive to keep monitoring on and
        // check when the help UI is accessed than it is to start/stop monitoring per tab change
        // (e.g. tab switch or in overview mode).
        if (isTablet()) return;

        mScreenshotMonitor.startMonitoring();
    }

    @Override
    public void onPauseWithNative() {
        mTabModelSelectorImpl.commitAllTabClosures();
        CookiesFetcher.persistCookies();

        mLocaleManager.setSnackbarManager(null);
        mLocaleManager.stopObservingPhoneChanges();

        mScreenshotMonitor.stopMonitoring();

        super.onPauseWithNative();
    }

    @Override
    public void onStopWithNative() {
        super.onStopWithNative();

        if (getActivityTab() != null) getActivityTab().setIsAllowedToReturnToExternalApp(false);

        mTabModelSelectorImpl.saveState();
        mActivityStopMetrics.onStopWithNative(this);

        ContextUtils.getAppSharedPreferences()
                .edit()
                .putLong(LAST_BACKGROUNDED_TIME_MS_PREF, System.currentTimeMillis())
                .apply();
    }

    @Override
    public void onStartWithNative() {
        mMainIntentMetrics.logLaunchBehavior();
        super.onStartWithNative();

        setInitialOverviewState();
        BrowserActionsService.onTabbedModeForegrounded();

        resetSavedInstanceState();
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        try {
            TraceEvent.begin("ChromeTabbedActivity.onNewIntentWithNative");

            super.onNewIntentWithNative(intent);
            if (isMainIntentFromLauncher(intent)) {
                if (IntentHandler.getUrlFromIntent(intent) == null) {
                    maybeLaunchNtpOrResetBottomSheetFromMainIntent(intent);
                }
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
    public ChromeTabCreator getTabCreator(boolean incognito) {
        return (ChromeTabCreator) super.getTabCreator(incognito);
    }

    @Override
    public ChromeTabCreator getCurrentTabCreator() {
        return (ChromeTabCreator) super.getCurrentTabCreator();
    }

    @Override
    protected AssistStatusHandler createAssistStatusHandler() {
        return new TabbedAssistStatusHandler(this);
    }

    private void handleDebugIntent(Intent intent) {
        if (ACTION_CLOSE_TABS.equals(intent.getAction())) {
            getTabModelSelector().closeAllTabs();
        } else if (MemoryPressureListener.handleDebugIntent(ChromeTabbedActivity.this,
                intent.getAction())) {
            // Handled.
        }
    }

    private void setInitialOverviewState() {
        boolean isOverviewVisible = mLayoutManager.overviewVisible();
        if (getActivityTab() == null && !isOverviewVisible) {
            toggleOverview();
        }

        if (BrowserActionsService.shouldToggleOverview(getIntent(), isOverviewVisible)) {
            toggleOverview();
        }
    }

    private void initializeUIWithNative() {
        try {
            TraceEvent.begin("ChromeTabbedActivity.initializeUI");

            CompositorViewHolder compositorViewHolder = getCompositorViewHolder();
            if (isTablet()) {
                mLayoutManager = new LayoutManagerChromeTablet(compositorViewHolder);
            } else {
                mLayoutManager = new LayoutManagerChromePhone(compositorViewHolder);
            }
            mLayoutManager.setEnableAnimations(DeviceClassManager.enableAnimations());
            mLayoutManager.addOverviewModeObserver(this);

            // TODO(yusufo): get rid of findViewById(R.id.url_bar).
            initializeCompositorContent(mLayoutManager, findViewById(R.id.url_bar),
                    mContentContainer, mControlContainer);

            mTabModelSelectorImpl.setOverviewModeBehavior(mLayoutManager);

            mUndoBarPopupController.initialize();

            // Adjust the content container if we're not entering fullscreen mode.
            if (getFullscreenManager() == null) {
                float controlHeight = getResources().getDimension(R.dimen.control_container_height);
                ((FrameLayout.LayoutParams) mContentContainer.getLayoutParams()).topMargin =
                        (int) controlHeight;
            }

            OnClickListener tabSwitcherClickHandler = v -> {
                if (getFullscreenManager() != null
                        && getFullscreenManager().getPersistentFullscreenMode()) {
                    return;
                }

                Layout activeLayout = mLayoutManager.getActiveLayout();
                if (activeLayout instanceof StackLayout) {
                    if (!activeLayout.isHiding()) {
                        RecordUserAction.record("MobileToolbarStackViewButtonInStackView");
                    }
                }

                toggleOverview();
            };
            OnClickListener newTabClickHandler = v -> {
                getTabModelSelector().getModel(false).commitAllTabClosures();
                // This assumes that the keyboard can not be seen at the same time as the
                // newtab button on the toolbar.
                getCurrentTabCreator().launchNTP();
                mLocaleManager.showSearchEnginePromoIfNeeded(ChromeTabbedActivity.this, null);
            };
            OnClickListener bookmarkClickHandler = v -> addOrEditBookmark(getActivityTab());
            OnClickListener incognitoClickHandler = v -> {
                Layout activeLayout = mLayoutManager.getActiveLayout();
                if (!activeLayout.shouldAllowIncognitoSwitching()) return;

                if (activeLayout instanceof StackLayout) {
                    // Without this call, tapping the incognito toggle immediately after closing a
                    // non-incognito tab will not work properly, because the tab closure will bring
                    // us back to normal mode. We need to handle the tab closure here before running
                    // the animation.
                    ((StackLayout) activeLayout).commitOutstandingModelState(LayoutManager.time());
                }

                if (mTabModelSelectorImpl != null) {
                    mTabModelSelectorImpl.selectModel(!mTabModelSelectorImpl.isIncognitoSelected());
                }
            };

            getToolbarManager().initializeWithNative(mTabModelSelectorImpl,
                    getFullscreenManager().getBrowserVisibilityDelegate(), getFindToolbarManager(),
                    mLayoutManager, mLayoutManager, tabSwitcherClickHandler, newTabClickHandler,
                    bookmarkClickHandler, null, incognitoClickHandler);

            mLayoutManager.setToolbarManager(getToolbarManager());

            if (isTablet()) {
                EmptyBackgroundViewWrapper bgViewWrapper = new EmptyBackgroundViewWrapper(
                        getTabModelSelector(), getTabCreator(false), ChromeTabbedActivity.this,
                        getAppMenuHandler(), getSnackbarManager(), mLayoutManager);
                bgViewWrapper.initialize();
            }

            mLayoutManager.hideOverview(false);

            mScreenshotMonitor = new ScreenshotMonitor(ChromeTabbedActivity.this);

            if (!CommandLine.getInstance().hasSwitch(
                        ChromeSwitches.ENABLE_INCOGNITO_SNAPSHOTS_IN_ANDROID_RECENTS)) {
                IncognitoTabSnapshotController.createIncognitoTabSnapshotController(
                        getWindow(), mLayoutManager, mTabModelSelectorImpl);
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O_MR1) {
                mNavigationBarColorController = new NavigationBarColorController(
                        getWindow(), getTabModelSelector(), getLayoutManager());
            }

            mUIWithNativeInitialized = true;
            onAccessibilityTabSwitcherModeChanged();
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeUI");
        }
    }

    private boolean isMainIntentFromLauncher(Intent intent) {
        return intent != null && TextUtils.equals(intent.getAction(), Intent.ACTION_MAIN)
                && intent.hasCategory(Intent.CATEGORY_LAUNCHER);
    }

    private void logMainIntentBehavior(Intent intent) {
        assert isMainIntentFromLauncher(intent);
        mMainIntentMetrics.onMainIntentWithNative(getTimeSinceLastBackgroundedMs());
    }

    /** Access the main intent metrics for test validation. */
    @VisibleForTesting
    public MainIntentBehaviorMetrics getMainIntentBehaviorMetricsForTesting() {
        return mMainIntentMetrics;
    }

    /**
     * Determines if the intent should trigger an NTP and launches it if applicable. If Chrome Home
     * is enabled, we reset the bottom sheet state to half after some time being backgrounded.
     *
     * @param intent The intent to check whether an NTP should be triggered.
     * @return Whether an NTP was triggered as a result of this intent.
     */
    private boolean maybeLaunchNtpOrResetBottomSheetFromMainIntent(Intent intent) {
        assert isMainIntentFromLauncher(intent);

        if (!mIntentHandler.isIntentUserVisible()) return false;

        if (!ChromeFeatureList.isEnabled(ChromeFeatureList.NTP_LAUNCH_AFTER_INACTIVITY)) {
            return false;
        }

        int ntpLaunchDelayInMins = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                ChromeFeatureList.NTP_LAUNCH_AFTER_INACTIVITY, NTP_LAUNCH_DELAY_IN_MINS_PARAM, -1);
        if (ntpLaunchDelayInMins == -1) {
            Log.e(TAG, "No NTP launch delay specified despite enabled field trial");
            return false;
        }

        long lastBackgroundedTimeMs =
                ContextUtils.getAppSharedPreferences().getLong(LAST_BACKGROUNDED_TIME_MS_PREF, -1);
        if (lastBackgroundedTimeMs == -1) return false;

        long backgroundDurationMinutes = TimeUnit.MINUTES.convert(
                System.currentTimeMillis() - lastBackgroundedTimeMs, TimeUnit.MILLISECONDS);

        if (backgroundDurationMinutes < ntpLaunchDelayInMins) {
            Log.i(TAG, "Not launching NTP due to inactivity, background time: %d, launch delay: %d",
                    backgroundDurationMinutes, ntpLaunchDelayInMins);
            return false;
        }

        if (isInOverviewMode() && !isTablet()) {
            mLayoutManager.hideOverview(false);
        }

        if (!reuseOrCreateNewNtp()) return false;
        RecordUserAction.record("MobileStartup.MainIntent.NTPCreatedDueToInactivity");
        return true;
    }

    /**
     * Creates or reuses an existing NTP and displays it to the user.
     *
     * @return Whether an NTP was reused/created. This returns false if the currently selected
     * tab is an NTP, and no action is taken.
     */
    private boolean reuseOrCreateNewNtp() {
        // In cases where the tab model is initialized, attempt to reuse an existing NTP if
        // available before attempting to create a new one.
        TabModel normalTabModel = getTabModelSelector().getModel(false);
        Tab ntpToRefocus = null;
        for (int i = 0; i < normalTabModel.getCount(); i++) {
            Tab tab = normalTabModel.getTabAt(i);

            if (NewTabPage.isNTPUrl(tab.getUrl()) && !tab.canGoBack() && !tab.canGoForward()) {
                // If the currently selected tab is an NTP, then take no action.
                if (getActivityTab().equals(tab)) return false;
                ntpToRefocus = tab;
                break;
            }
        }

        if (ntpToRefocus != null) {
            normalTabModel.moveTab(ntpToRefocus.getId(), normalTabModel.getCount());
            normalTabModel.setIndex(
                    TabModelUtils.getTabIndexById(normalTabModel, ntpToRefocus.getId()),
                    TabSelectionType.FROM_USER);
        } else {
            getTabCreator(false).launchUrl(UrlConstants.NTP_URL, TabLaunchType.FROM_EXTERNAL_APP);
        }
        return true;
    }

    /**
     * Returns the number of milliseconds since Chrome was last backgrounded.
     */
    private long getTimeSinceLastBackgroundedMs() {
        // TODO(tedchoc): We should cache the last visible time and reuse it to avoid different
        //                values of this depending on when it is called after the activity was
        //                shown.
        long currentTime = System.currentTimeMillis();
        long lastBackgroundedTimeMs = ContextUtils.getAppSharedPreferences().getLong(
                LAST_BACKGROUNDED_TIME_MS_PREF, currentTime);
        return currentTime - lastBackgroundedTimeMs;
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

            Intent intent = getIntent();

            boolean hadCipherData =
                    CipherFactory.getInstance().restoreFromBundle(getSavedInstanceState());

            boolean noRestoreState =
                    CommandLine.getInstance().hasSwitch(ChromeSwitches.NO_RESTORE_STATE);
            if (noRestoreState) {
                // Clear the state files because they are inconsistent and useless from now on.
                mTabModelSelectorImpl.clearState();
            } else {
                // State should be clear when we start first run and hence we do not need to load
                // a previous state. This may change the current Model, watch out for initialization
                // based on the model.
                // Never attempt to restore incognito tabs when this activity was previously swiped
                // away in Recents. http://crbug.com/626629
                boolean ignoreIncognitoFiles = !hadCipherData;
                mTabModelSelectorImpl.loadState(ignoreIncognitoFiles);
            }

            mIntentWithEffect = false;
            if (getSavedInstanceState() == null && intent != null) {
                if (!mIntentHandler.shouldIgnoreIntent(intent)) {
                    mIntentWithEffect = mIntentHandler.onNewIntent(intent);
                }

                if (isMainIntentFromLauncher(intent)) {
                    if (IntentHandler.getUrlFromIntent(intent) == null) {
                        assert !mIntentWithEffect
                                : "ACTION_MAIN should not have triggered any prior action";
                        mIntentWithEffect = maybeLaunchNtpOrResetBottomSheetFromMainIntent(intent);
                    }
                    logMainIntentBehavior(intent);
                }
            }

            boolean hasTabWaitingForReparenting =
                    AsyncTabParamsManager.hasParamsWithTabToReparent();
            boolean hasBrowserActionTabs = BrowserActionsTabModelSelector.isInitialized()
                    && BrowserActionsTabModelSelector.getInstance().getTotalTabCount() != 0;
            mCreatedTabOnStartup = getCurrentTabModel().getCount() > 0
                    || mTabModelSelectorImpl.getRestoredTabCount() > 0 || mIntentWithEffect
                    || hasBrowserActionTabs || hasTabWaitingForReparenting;

            // We always need to try to restore tabs. The set of tabs might be empty, but at least
            // it will trigger the notification that tab restore is complete which is needed by
            // other parts of Chrome such as sync.
            boolean activeTabBeingRestored = !mIntentWithEffect;

            mMainIntentMetrics.setIgnoreEvents(true);
            mTabModelSelectorImpl.restoreTabs(activeTabBeingRestored);
            if (hasBrowserActionTabs) {
                BrowserActionsTabModelSelector.getInstance().mergeBrowserActionsTabModel(
                        this, !mIntentWithEffect);
            }
            mMainIntentMetrics.setIgnoreEvents(false);

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
                PartnerBrowserCustomizations.setOnInitializeAsyncFinished(
                        () -> {
                            mMainIntentMetrics.setIgnoreEvents(true);
                            createInitialTab();
                            mMainIntentMetrics.setIgnoreEvents(false);
                        }, INITIAL_TAB_CREATION_TIMEOUT_MS);
            }

            RecordHistogram.recordBooleanHistogram(
                    "MobileStartup.ColdStartupIntent", mIntentWithEffect);
        } finally {
            TraceEvent.end("ChromeTabbedActivity.initializeState");
        }
    }

    /**
     * Create an initial tab for cold start without restored tabs.
     */
    private void createInitialTab() {
        String url = HomepageManager.getHomepageUri();
        if (TextUtils.isEmpty(url)) {
            url = UrlConstants.NTP_URL;
        } else {
            boolean startupHomepageIsNtp = false;
            // Migrate legacy NTP URLs (chrome://newtab) to the newer format
            // (chrome-native://newtab)
            if (NewTabPage.isNTPUrl(url)) {
                url = UrlConstants.NTP_URL;
                startupHomepageIsNtp = true;
            }
            RecordHistogram.recordBooleanHistogram(
                    "MobileStartup.LoadedHomepageOnColdStart", startupHomepageIsNtp);
        }

        getTabCreator(false).launchUrl(url, TabLaunchType.FROM_CHROME_UI);
    }

    @Override
    public void onAccessibilityModeChanged(boolean enabled) {
        super.onAccessibilityModeChanged(enabled);

        if (mLayoutManager != null) {
            mLayoutManager.setEnableAnimations(DeviceClassManager.enableAnimations());
        }
        if (isTablet()) {
            if (getCompositorViewHolder() != null) {
                getCompositorViewHolder().onAccessibilityStatusChanged(enabled);
            }
        }

        onAccessibilityTabSwitcherModeChanged();
    }

    private void onAccessibilityTabSwitcherModeChanged() {
        if (!mUIWithNativeInitialized) return;

        boolean accessibilityTabSwitcherEnabled = DeviceClassManager.enableAccessibilityLayout();
        if (mLayoutManager != null && mLayoutManager.overviewVisible()
                && (mIsAccessibilityTabSwitcherEnabled == null
                           || mIsAccessibilityTabSwitcherEnabled
                                   != DeviceClassManager.enableAccessibilityLayout())) {
            mLayoutManager.hideOverview(false);
            if (getTabModelSelector().getCurrentModel().getCount() == 0) {
                getCurrentTabCreator().launchNTP();
            }
        }
        mIsAccessibilityTabSwitcherEnabled = accessibilityTabSwitcherEnabled;

        if (AccessibilityUtil.isAccessibilityEnabled()) {
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
                boolean hasUserGesture, Intent intent) {
            if (isFromChrome(intent, externalAppId)) {
                RecordUserAction.record("MobileTabbedModeViewIntentFromChrome");
            } else {
                RecordUserAction.record("MobileTabbedModeViewIntentFromApp");
            }

            boolean fromLauncherShortcut = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false);

            TabModel tabModel = getCurrentTabModel();
            switch (tabOpenType) {
                case TabOpenType.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB:
                    // Used by the bookmarks application.
                    if (tabModel.getCount() > 0 && mUIWithNativeInitialized
                            && mLayoutManager.overviewVisible()) {
                        mLayoutManager.hideOverview(true);
                    }
                    mTabModelSelectorImpl.tryToRestoreTabStateForUrl(url);
                    int tabToBeClobberedIndex = TabModelUtils.getTabIndexByUrl(tabModel, url);
                    Tab tabToBeClobbered = tabModel.getTabAt(tabToBeClobberedIndex);
                    if (tabToBeClobbered != null) {
                        TabModelUtils.setIndex(tabModel, tabToBeClobberedIndex);
                        tabToBeClobbered.reload();
                    } else {
                        launchIntent(url, referer, headers, externalAppId, true, intent);
                    }
                    logMobileReceivedExternalIntent(externalAppId, intent);
                    int shortcutSource = intent.getIntExtra(
                            ShortcutHelper.EXTRA_SOURCE, ShortcutSource.UNKNOWN);
                    LaunchMetrics.recordHomeScreenLaunchIntoTab(url, shortcutSource);
                    break;
                case TabOpenType.BRING_TAB_TO_FRONT:
                    mTabModelSelectorImpl.tryToRestoreTabStateForId(tabIdToBringToFront);

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

                    if (tabModel.getCount() > 0 && mUIWithNativeInitialized
                            && mLayoutManager.overviewVisible()) {
                        mLayoutManager.hideOverview(true);
                    }

                    logMobileReceivedExternalIntent(externalAppId, intent);
                    break;
                case TabOpenType.CLOBBER_CURRENT_TAB:
                    // The browser triggered the intent. This happens when clicking links which
                    // can be handled by other applications (e.g. www.youtube.com links).
                    Tab currentTab = getActivityTab();
                    if (currentTab != null) {
                        TabRedirectHandler.from(currentTab).updateIntent(intent);
                        int transitionType = PageTransition.LINK | PageTransition.FROM_API;
                        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
                        loadUrlParams.setIntentReceivedTimestamp(mIntentHandlingTimeMs);
                        loadUrlParams.setHasUserGesture(hasUserGesture);
                        loadUrlParams.setTransitionType(IntentHandler.getTransitionTypeFromIntent(
                                intent, transitionType));
                        if (referer != null) {
                            loadUrlParams.setReferrer(new Referrer(
                                    referer, IntentHandler.getReferrerPolicyFromIntent(intent)));
                        }
                        currentTab.loadUrl(loadUrlParams);
                    } else {
                        launchIntent(url, referer, headers, externalAppId, true, intent);
                    }
                    break;
                case TabOpenType.REUSE_APP_ID_MATCHING_TAB_ELSE_NEW_TAB:
                    openNewTab(url, referer, headers, externalAppId, intent, false);
                    break;
                case TabOpenType.OPEN_NEW_TAB:
                    if (fromLauncherShortcut) {
                        recordLauncherShortcutAction(false);
                        reportNewTabShortcutUsed(false);
                    }

                    openNewTab(url, referer, headers, externalAppId, intent, true);
                    break;
                case TabOpenType.OPEN_NEW_INCOGNITO_TAB:
                    if (!TextUtils.equals(externalAppId, getPackageName())) {
                        assert false : "Only Chrome is allowed to open incognito tabs";
                        Log.e(TAG, "Only Chrome is allowed to open incognito tabs");
                        return;
                    }

                    if (!PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
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
                        } else {
                            // Used by the Account management screen to open a new incognito tab.
                            // Account management screen collects its metrics separately.
                            getTabCreator(true).launchUrl(
                                    UrlConstants.NTP_URL, TabLaunchType.FROM_CHROME_UI,
                                    intent, mIntentHandlingTimeMs);
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
            getToolbarManager().setUrlBarFocus(false);
        }

        @Override
        public void processWebSearchIntent(String query) {
            assert false;
        }

        /**
         * Opens a new Tab with the possibility of showing it in a Custom Tab, instead.
         *
         * See IntentHandler#processUrlViewIntent() for an explanation most of the parameters.
         * @param forceNewTab If not handled by a Custom Tab, forces the new tab to be created.
         */
        private void openNewTab(String url, String referer, String headers,
                String externalAppId, Intent intent, boolean forceNewTab) {
            boolean isAllowedToReturnToExternalApp = IntentUtils.safeGetBooleanExtra(
                    intent, LaunchIntentDispatcher.EXTRA_IS_ALLOWED_TO_RETURN_TO_PARENT, true);

            // Create a new tab.
            Tab newTab =
                    launchIntent(url, referer, headers, externalAppId, forceNewTab, intent);
            if (newTab != null) {
                newTab.setIsAllowedToReturnToExternalApp(isAllowedToReturnToExternalApp);
            } else {
                // TODO(twellington): This should only happen for NTPs created in Chrome Home.  See
                //                    if we should be caching setIsAllowedToReturnToExternalApp
                //                    in those cases.
                assert NewTabPage.isNTPUrl(url);
                assert getBottomSheet() != null;
            }
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
    public void preInflationStartup() {
        super.preInflationStartup();

        // Decide whether to record startup UMA histograms. This is done  early in the main
        // Activity.onCreate() to avoid recording navigation delays when they require user input to
        // proceed. For example, FRE (First Run Experience) happens before the activity is created,
        // and triggers initialization of the native library. At the moment it seems safe to assume
        // that uninitialized native library is an indication of an application start that is
        // followed by navigation immediately without user input.
        if (!LibraryLoader.getInstance().isInitialized()) {
            getActivityTabStartupMetricsTracker().trackStartupMetrics(STARTUP_UMA_HISTOGRAM_SUFFIX);
        }

        CommandLine commandLine = CommandLine.getInstance();
        if (commandLine.hasSwitch(ContentSwitches.ENABLE_TEST_INTENTS)
                && getIntent() != null
                && getIntent().hasExtra(
                        ChromeTabbedActivity.INTENT_EXTRA_TEST_RENDER_PROCESS_LIMIT)) {
            int value = getIntent().getIntExtra(
                    ChromeTabbedActivity.INTENT_EXTRA_TEST_RENDER_PROCESS_LIMIT, -1);
            if (value != -1) {
                String[] args = new String[1];
                args[0] = "--" + ContentSwitches.RENDER_PROCESS_LIMIT
                        + "=" + Integer.toString(value);
                commandLine.appendSwitchesAndArguments(args);
            }
        }

        supportRequestWindowFeature(Window.FEATURE_ACTION_MODE_OVERLAY);

        // We are starting from history with a URL after data has been cleared. On Samsung this
        // can happen after user clears data and clicks on a recents item on pre-L devices.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.LOLLIPOP
                && getIntent().getData() != null
                && (getIntent().getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY) != 0
                && OmahaBase.isProbablyFreshInstall(this)) {
            getIntent().setData(null);
        }

        IncognitoTabHostRegistry.getInstance().register(mIncognitoTabHost);
    }

    @Override
    protected int getControlContainerLayoutId() {
        return R.layout.control_container;
    }

    @Override
    protected int getToolbarLayoutId() {
        return isTablet() ? R.layout.toolbar_tablet : R.layout.toolbar_phone;
    }

    @Override
    public void postInflationStartup() {
        super.postInflationStartup();

        // Critical path for startup. Create the minimum objects needed
        // to allow a blank screen draw (without depending on any native code)
        // and then yield ASAP.
        if (isFinishing()) return;

        // Don't show the keyboard until user clicks in.
        getWindow().setSoftInputMode(
                WindowManager.LayoutParams.SOFT_INPUT_STATE_ALWAYS_HIDDEN
                | WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);

        mContentContainer = (ViewGroup) findViewById(android.R.id.content);
        mControlContainer = (ToolbarControlContainer) findViewById(R.id.control_container);

        mUndoBarPopupController = new UndoBarController(this, mTabModelSelectorImpl,
                getSnackbarManager());
    }

    @Override
    protected void initializeToolbar() {
        super.initializeToolbar();
        if (isTablet()) {
            getToolbarManager().setShouldUpdateToolbarPrimaryColor(false);
        } else if (FeatureUtilities.isBottomToolbarEnabled()) {
            getToolbarManager().enableBottomToolbar();
        }
    }

    @Override
    protected TabModelSelector createTabModelSelector() {
        assert mTabModelSelectorImpl == null;

        Bundle savedInstanceState = getSavedInstanceState();

        // We determine the model as soon as possible so every systems get initialized coherently.
        boolean startIncognito = savedInstanceState != null
                && savedInstanceState.getBoolean("is_incognito_selected", false);
        int index = savedInstanceState != null ? savedInstanceState.getInt(WINDOW_INDEX, 0) : 0;

        mTabModelSelectorImpl = (TabModelSelectorImpl)
                TabWindowManager.getInstance().requestSelector(this, this, index);
        if (mTabModelSelectorImpl == null) {
            Toast.makeText(this, getString(R.string.unsupported_number_of_windows),
                    Toast.LENGTH_LONG).show();
            finish();
            return null;
        }

        mTabModelSelectorImpl.addObserver(new EmptyTabModelSelectorObserver() {
            @Override
            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                if (isInOverviewMode()) {
                    // The passed-in color is ignored when the tab switcher is open. This call
                    // causes the toolbar color to change (if necessary) based on whether or not
                    // we're in incognito mode.
                    setStatusBarColor(null, Color.BLACK);
                }
            }

            @Override
            public void onTabStateInitialized() {
                if (!mCreatedTabOnStartup) return;

                TabModel model = mTabModelSelectorImpl.getModel(false);
                TasksUma.recordTasksUma(model);
            }
        });

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(mTabModelSelectorImpl) {
            @Override
            public void onPageLoadFinished(final Tab tab) {
                mAppIndexingUtil.extractCopylessPasteMetadata(tab);
            }

            @Override
            public void onDidFinishNavigation(Tab tab, String url, boolean isInMainFrame,
                    boolean isErrorPage, boolean hasCommitted, boolean isSameDocument,
                    boolean isFragmentNavigation, Integer pageTransition, int errorCode,
                    int httpStatusCode) {
                if (hasCommitted && isInMainFrame) {
                    DataReductionPromoInfoBar.maybeLaunchPromoInfoBar(ChromeTabbedActivity.this,
                            tab.getWebContents(), url, tab.isShowingErrorPage(),
                            isFragmentNavigation, httpStatusCode);
                }
            }
        };

        if (startIncognito) mTabModelSelectorImpl.selectModel(true);

        return mTabModelSelectorImpl;
    }

    @Override
    protected AppMenuPropertiesDelegate createAppMenuPropertiesDelegate() {
        return new AppMenuPropertiesDelegate(this) {
            private boolean shouldShowDataSaverMenuItem() {
                return DataReductionProxySettings.getInstance()
                        .shouldUseDataReductionMainMenuItem();
            }

            @Override
            public int getFooterResourceId() {
                if (FeatureUtilities.isBottomToolbarEnabled()) {
                    return this.shouldShowPageMenu() ? R.layout.icon_row_menu_footer : 0;
                }
                return shouldShowDataSaverMenuItem() ? R.layout.data_reduction_main_menu_item : 0;
            }

            @Override
            public void onFooterViewInflated(AppMenu menu, View view) {
                if (view instanceof AppMenuIconRowFooter) {
                    ((AppMenuIconRowFooter) view)
                            .initialize(ChromeTabbedActivity.this, menu, mBookmarkBridge);
                }
            }

            @Override
            public int getHeaderResourceId() {
                if (FeatureUtilities.isBottomToolbarEnabled()) {
                    return shouldShowDataSaverMenuItem() ? R.layout.data_reduction_main_menu_item
                                                         : 0;
                }
                return 0;
            }

            @Override
            public void onHeaderViewInflated(AppMenu menu, View view) {
                if (view instanceof DataReductionMainMenuItem) {
                    view.findViewById(R.id.data_reduction_menu_divider).setVisibility(View.GONE);
                }
            }

            @Override
            public boolean shouldShowFooter(int maxMenuHeight) {
                if (FeatureUtilities.isBottomToolbarEnabled()) return true;
                if (shouldShowDataSaverMenuItem()) {
                    return canShowDataReductionItem(maxMenuHeight);
                }
                return super.shouldShowFooter(maxMenuHeight);
            }

            @Override
            public boolean shouldShowHeader(int maxMenuHeight) {
                if (!FeatureUtilities.isBottomToolbarEnabled()) {
                    return super.shouldShowHeader(maxMenuHeight);
                }

                if (DataReductionProxySettings.getInstance().shouldUseDataReductionMainMenuItem()) {
                    return canShowDataReductionItem(maxMenuHeight);
                }

                return super.shouldShowHeader(maxMenuHeight);
            }

            private boolean canShowDataReductionItem(int maxMenuHeight) {
                // TODO(twellington): Account for whether a different footer or header is showing.
                return maxMenuHeight >= getResources().getDimension(
                                                R.dimen.data_saver_menu_footer_min_show_height);
            }
        };
    }

    @Override
    protected Pair<TabbedModeTabCreator, TabbedModeTabCreator> createTabCreators() {
        return Pair.create(
                new TabbedModeTabCreator(this, getWindowAndroid(), false),
                new TabbedModeTabCreator(this, getWindowAndroid(), true));
    }

    @Override
    protected void initDeferredStartupForActivity() {
        super.initDeferredStartupForActivity();
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            ActivityManager am =
                    (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
            RecordHistogram.recordSparseSlowlyHistogram(
                    "MemoryAndroid.DeviceMemoryClass", am.getMemoryClass());

            AutocompleteController.nativePrefetchZeroSuggestResults();

            LauncherShortcutActivity.updateIncognitoShortcut(ChromeTabbedActivity.this);

            ChromeSurveyController.initialize(mTabModelSelectorImpl);
        });
    }

    @Override
    protected void recordIntentToCreationTime(long timeMs) {
        super.recordIntentToCreationTime(timeMs);
        RecordHistogram.recordCustomTimesHistogram("MobileStartup.IntentToCreationTime.TabbedMode",
                timeMs, 1, TimeUnit.SECONDS.toMillis(30), TimeUnit.MILLISECONDS, 50);
    }

    @Override
    protected boolean isStartedUpCorrectly(Intent intent) {
        // If tabs from this instance were merged into a different ChromeTabbedActivity instance
        // and the other instance is still running, then this instance should not be created. This
        // may happen if the process is restarted e.g. on upgrade or from about://flags.
        // See crbug.com/657418
        boolean tabsMergedIntoAnotherInstance =
                sMergedInstanceTaskId != 0 && sMergedInstanceTaskId != getTaskId();

        // Since a static is used to track the merged instance task id, it is possible that
        // sMergedInstanceTaskId is still set even though the associated task is not running.
        boolean mergedInstanceTaskStillRunning = isMergedInstanceTaskRunning();

        if (tabsMergedIntoAnotherInstance && mergedInstanceTaskStillRunning) {
            // Currently only two instances of ChromeTabbedActivity may be running at any given
            // time. If tabs were merged into another instance and this instance is being killed due
            // to incorrect startup, then no other instances should exist. Reset the merged instance
            // task id.
            setMergedInstanceTaskId(0);
            return false;
        } else if (!mergedInstanceTaskStillRunning) {
            setMergedInstanceTaskId(0);
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
        boolean currentTabIsNtp = currentTab != null && NewTabPage.isNTPUrl(currentTab.getUrl());
        if (id == R.id.move_to_other_window_menu_id) {
            if (currentTab != null) moveTabToOtherWindow(currentTab);
        } else if (id == R.id.new_tab_menu_id) {
            getTabModelSelector().getModel(false).commitAllTabClosures();
            RecordUserAction.record("MobileMenuNewTab");
            RecordUserAction.record("MobileNewTabOpened");
            reportNewTabShortcutUsed(false);
            getTabCreator(false).launchNTP();

            mLocaleManager.showSearchEnginePromoIfNeeded(this, null);
        } else if (id == R.id.new_incognito_tab_menu_id) {
            if (PrefServiceBridge.getInstance().isIncognitoModeEnabled()) {
                getTabModelSelector().getModel(false).commitAllTabClosures();
                // This action must be recorded before opening the incognito tab since UMA actions
                // are dropped when an incognito tab is open.
                RecordUserAction.record("MobileMenuNewIncognitoTab");
                RecordUserAction.record("MobileNewTabOpened");
                reportNewTabShortcutUsed(true);
                getTabCreator(true).launchNTP();
            }
        } else if (id == R.id.all_bookmarks_menu_id) {
            if (currentTab != null) {
                getCompositorViewHolder().hideKeyboard(() -> {
                    BookmarkUtils.showBookmarkManager(ChromeTabbedActivity.this);
                });
                if (currentTabIsNtp) {
                    NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_BOOKMARKS_MANAGER);
                }
                RecordUserAction.record("MobileMenuAllBookmarks");
            }
        } else if (id == R.id.recent_tabs_menu_id) {
            if (currentTab != null) {
                LoadUrlParams params = new LoadUrlParams(
                        UrlConstants.RECENT_TABS_URL, PageTransition.AUTO_BOOKMARK);
                currentTab.loadUrl(params);
                if (currentTabIsNtp) {
                    NewTabPageUma.recordAction(NewTabPageUma.ACTION_OPENED_RECENT_TABS_MANAGER);
                }

                RecordUserAction.record("MobileMenuRecentTabs");
            }
        } else if (id == R.id.close_all_tabs_menu_id) {
            // Close both incognito and normal tabs
            getTabModelSelector().closeAllTabs();
            RecordUserAction.record("MobileMenuCloseAllTabs");
        } else if (id == R.id.close_all_incognito_tabs_menu_id) {
            // Close only incognito tabs
            getTabModelSelector().getModel(true).closeAllTabs();
            // TODO(nileshagrawal) Record unique action for this. See bug http://b/5542946.
            RecordUserAction.record("MobileMenuCloseAllTabs");
        } else if (id == R.id.focus_url_bar) {
            boolean isUrlBarVisible = !mLayoutManager.overviewVisible()
                    && (!isTablet() || getCurrentTabModel().getCount() != 0);
            if (isUrlBarVisible) {
                getToolbarManager().setUrlBarFocus(true);
            }
        } else if (id == R.id.downloads_menu_id) {
            DownloadUtils.showDownloadManager(this, currentTab);
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

    @Override
    protected void onOmniboxFocusChanged(boolean hasFocus) {
        super.onOmniboxFocusChanged(hasFocus);

        mMainIntentMetrics.onOmniboxFocused();

        mTabModalHandler.onOmniboxFocusChanged(hasFocus);
    }

    private void recordBackPressedUma(String logMessage, @BackPressedResult int action) {
        Log.i(TAG, "Back pressed: " + logMessage);
        RecordHistogram.recordEnumeratedHistogram(
                "Android.Activity.ChromeTabbedActivity.SystemBackAction", action,
                BackPressedResult.NUM_ENTRIES);
    }

    private void recordLauncherShortcutAction(boolean isIncognito) {
        if (isIncognito) {
            RecordUserAction.record("Android.LauncherShortcut.NewIncognitoTab");
        } else {
            RecordUserAction.record("Android.LauncherShortcut.NewTab");
        }
    }

    private void moveTabToOtherWindow(Tab tab) {
        Class<? extends Activity> targetActivity =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(this);
        if (targetActivity == null) return;

        Intent intent = new Intent(this, targetActivity);
        MultiWindowUtils.setOpenInOtherWindowIntentExtras(intent, this, targetActivity);
        MultiWindowUtils.onMultiInstanceModeStarted();

        tab.detachAndStartReparenting(
                intent, MultiWindowUtils.getOpenInOtherWindowActivityOptions(this), null);
    }

    @Override
    public boolean handleBackPressed() {
        // BottomSheet can be opened before native is initialized.
        if (!mUIWithNativeInitialized) {
            return getBottomSheet() != null && getBottomSheet().handleBackPress();
        }

        if (getManualFillingController().handleBackPress()) return true;

        final Tab currentTab = getActivityTab();

        if (exitFullscreenIfShowing()) {
            recordBackPressedUma("Exited fullscreen", BackPressedResult.EXITED_FULLSCREEN);
            return true;
        }

        if (getBottomSheet() != null && getBottomSheet().handleBackPress()) return true;

        if (mTabModalHandler.handleBackPress()) return true;

        if (currentTab == null) {
            recordBackPressedUma("currentTab is null", BackPressedResult.TAB_IS_NULL);
            moveTaskToBack(true);
            return true;
        }

        // If we are in overview mode and not a tablet, then leave overview mode on back.
        if (mLayoutManager.overviewVisible() && !isTablet()) {
            recordBackPressedUma("Hid overview", BackPressedResult.EXITED_TAB_SWITCHER);
            mLayoutManager.hideOverview(true);
            return true;
        }

        if (getToolbarManager().back()) {
            recordBackPressedUma("Navigating backward", BackPressedResult.NAVIGATED_BACK);
            return true;
        }

        // If the current tab url is HELP_URL, then the back button should close the tab to
        // get back to the previous state. The reason for startsWith check is that the
        // actual redirected URL is a different system language based help url.
        final @TabLaunchType int type = currentTab.getLaunchType();
        final boolean helpUrl = currentTab.getUrl().startsWith(HELP_URL_PREFIX);
        if (type == TabLaunchType.FROM_CHROME_UI && helpUrl) {
            getCurrentTabModel().closeTab(currentTab);
            recordBackPressedUma("Closed tab for help URL", BackPressedResult.HELP_URL_CLOSED);
            return true;
        }

        final boolean shouldCloseTab = backShouldCloseTab(currentTab);

        // Minimize the app if either:
        // - we decided not to close the tab
        // - we decided to close the tab, but it was opened by an external app, so we will go
        //   exit Chrome on top of closing the tab
        final boolean minimizeApp = !shouldCloseTab || currentTab.isCreatedForExternalApp();
        if (minimizeApp) {
            if (shouldCloseTab) {
                recordBackPressedUma(
                        "Minimized and closed tab", BackPressedResult.MINIMIZED_TAB_CLOSED);
                mActivityStopMetrics.setStopReason(ActivityStopMetrics.StopReason.BACK_BUTTON);
                sendToBackground(currentTab);
                return true;
            } else {
                recordBackPressedUma(
                        "Minimized, kept tab", BackPressedResult.MINIMIZED_NO_TAB_CLOSED);
                mActivityStopMetrics.setStopReason(ActivityStopMetrics.StopReason.BACK_BUTTON);
                sendToBackground(null);
                return true;
            }
        } else if (shouldCloseTab) {
            recordBackPressedUma("Tab closed", BackPressedResult.TAB_CLOSED);
            getCurrentTabModel().closeTab(currentTab, true, false, false);
            return true;
        }

        assert false : "The back button should have already been handled by this point";
        recordBackPressedUma("Unhandled", BackPressedResult.NOTHING_HAPPENED);
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
    public static boolean backShouldCloseTab(Tab tab) {
        @TabLaunchType
        int type = tab.getLaunchType();

        return type == TabLaunchType.FROM_LINK || type == TabLaunchType.FROM_EXTERNAL_APP
                || type == TabLaunchType.FROM_LONGPRESS_FOREGROUND
                || type == TabLaunchType.FROM_LONGPRESS_BACKGROUND
                || (type == TabLaunchType.FROM_RESTORE && tab.getParentId() != Tab.INVALID_TAB_ID);
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
                if (!hasNextTab) mLayoutManager.showOverview(false);
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
     * @param intent        The original intent.
     */
    private Tab launchIntent(String url, String referer, String headers,
            String externalAppId, boolean forceNewTab, Intent intent) {
        if (mUIWithNativeInitialized && (getBottomSheet() == null || !NewTabPage.isNTPUrl(url))) {
            mLayoutManager.hideOverview(false);
            getToolbarManager().finishAnimations();
        }
        if (TextUtils.equals(externalAppId, getPackageName())) {
            // If the intent was launched by chrome, open the new tab in the appropriate model.
            // Using FROM_LINK ensures the tab is parented to the current tab, which allows
            // the back button to close these tabs and restore selection to the previous tab.
            boolean isIncognito = IntentUtils.safeGetBooleanExtra(intent,
                    IntentHandler.EXTRA_OPEN_NEW_INCOGNITO_TAB, false);
            boolean fromLauncherShortcut = IntentUtils.safeGetBooleanExtra(
                    intent, IntentHandler.EXTRA_INVOKED_FROM_SHORTCUT, false);
            LoadUrlParams loadUrlParams = new LoadUrlParams(url);
            loadUrlParams.setIntentReceivedTimestamp(mIntentHandlingTimeMs);
            loadUrlParams.setVerbatimHeaders(headers);
            return getTabCreator(isIncognito).createNewTab(
                    loadUrlParams,
                    fromLauncherShortcut ? TabLaunchType.FROM_LAUNCHER_SHORTCUT
                            : TabLaunchType.FROM_LINK,
                    null,
                    intent);
        } else {
            // Check if the tab is being created from a Reader Mode navigation.
            if (ReaderModeManager.isEnabled(this)
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

    private void toggleOverview() {
        Tab currentTab = getActivityTab();
        // If we don't have a current tab, show the overview mode.
        if (currentTab == null) {
            mLayoutManager.showOverview(false);
            return;
        }

        if (!mLayoutManager.overviewVisible()) {
            getCompositorViewHolder().hideKeyboard(() -> mLayoutManager.showOverview(true));
            updateAccessibilityState(false);
        } else {
            Layout activeLayout = mLayoutManager.getActiveLayout();
            if (activeLayout instanceof StackLayout) {
                ((StackLayout) activeLayout).commitOutstandingModelState(LayoutManager.time());
            }
            if (getCurrentTabModel().getCount() != 0) {
                // Don't hide overview if current tab stack is empty()
                mLayoutManager.hideOverview(true);
                updateAccessibilityState(true);
            }
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
        outState.putBoolean("is_incognito_selected", getCurrentTabModel().isIncognito());
        outState.putInt(WINDOW_INDEX,
                TabWindowManager.getInstance().getIndexForWindow(this));
    }

    @Override
    public void onDestroyInternal() {
        if (mLayoutManager != null) mLayoutManager.removeOverviewModeObserver(this);

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        if (mTabModelObserver != null) mTabModelObserver.destroy();

        if (mUndoBarPopupController != null) {
            mUndoBarPopupController.destroy();
            mUndoBarPopupController = null;
        }

        if (mTabModalHandler != null) {
            mTabModalHandler.destroy();
            mTabModalHandler = null;
        }

        if (mNavigationBarColorController != null) mNavigationBarColorController.destroy();

        IncognitoTabHostRegistry.getInstance().unregister(mIncognitoTabHost);
        super.onDestroyInternal();
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
        if (ChromeApplication.isSevereMemorySignal(level)) {
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
        if (keyCode == KeyEvent.KEYCODE_BACK && !isTablet()) {
            if (mShowHistoryRunnable == null) mShowHistoryRunnable = this ::showFullHistoryForTab;
            mHandler.postDelayed(mShowHistoryRunnable, ViewConfiguration.getLongPressTimeout());
            return super.onKeyDown(keyCode, event);
        }
        boolean isCurrentTabVisible = !mLayoutManager.overviewVisible()
                && (!isTablet() || getCurrentTabModel().getCount() != 0);
        return KeyboardShortcuts.onKeyDown(event, this, isCurrentTabVisible, true)
                || super.onKeyDown(keyCode, event);
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && !isTablet()) {
            mHandler.removeCallbacks(mShowHistoryRunnable);
            mShowHistoryRunnable = null;
        }
        return super.onKeyUp(keyCode, event);
    }

    @VisibleForTesting
    public NavigationPopup getNavigationPopupForTesting() {
        ThreadUtils.assertOnUiThread();
        return mNavigationPopup;
    }

    @VisibleForTesting
    public boolean hasPendingNavigationPopupForTesting() {
        ThreadUtils.assertOnUiThread();
        return mShowHistoryRunnable != null;
    }

    private void showFullHistoryForTab() {
        Tab tab = getActivityTab();
        if (tab == null || tab.getWebContents() == null || !tab.isUserInteractable()) return;

        mNavigationPopup = new NavigationPopup(tab.getProfile(), this,
                tab.getWebContents().getNavigationController(),
                NavigationPopup.Type.ANDROID_SYSTEM_BACK);
        mNavigationPopup.setOnDismissCallback(() -> mNavigationPopup = null);
        mNavigationPopup.show(findViewById(R.id.navigation_popup_anchor_stub));
    }

    @Override
    public void onProvideKeyboardShortcuts(List<KeyboardShortcutGroup> data, Menu menu,
            int deviceId) {
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

    @Override
    protected ModalDialogManager createModalDialogManager() {
        ModalDialogManager manager = super.createModalDialogManager();
        mTabModalHandler = new TabModalLifetimeHandler(this, manager);
        return manager;
    }

    // App Menu related code -----------------------------------------------------------------------

    @Override
    public boolean shouldShowAppMenu() {
        // The popup menu relies on the model created during the full UI initialization, so do not
        // attempt to show the menu until the UI creation has finished.
        if (!mUIWithNativeInitialized) return false;

        // If the current active tab is showing a tab modal dialog, an app menu shouldn't be shown
        // in any cases, e.g. when a hardware menu button is clicked.
        Tab tab = getActivityTab();
        if (tab != null && tab.isShowingTabModalDialog()) return false;

        return super.shouldShowAppMenu();
    }

    @Override
    protected void showAppMenuForKeyboardEvent() {
        if (!mUIWithNativeInitialized) return;
        super.showAppMenuForKeyboardEvent();
    }

    @Override
    public boolean isInOverviewMode() {
        return mLayoutManager != null && mLayoutManager.overviewVisible();
    }

    /**
     * Update focus and accessibility importance for background view when accessibility tab switcher
     * is used in Chrome Home.
     */
    private void updateAccessibilityVisibility() {
        if (getBottomSheet() == null || mLayoutManager == null) return;

        CompositorViewHolder compositorViewHolder = getCompositorViewHolder();
        if (compositorViewHolder != null) {
            compositorViewHolder.setFocusable(!isViewObscuringAllTabs());
        }

        OverviewListLayout overviewListLayout =
                (OverviewListLayout) mLayoutManager.getOverviewListLayout();
        if (overviewListLayout != null) {
            overviewListLayout.updateAccessibilityVisibility(!isViewObscuringAllTabs());
        }
    }

    @Override
    public void addViewObscuringAllTabs(View view) {
        super.addViewObscuringAllTabs(view);
        updateAccessibilityVisibility();
    }

    @Override
    public void removeViewObscuringAllTabs(View view) {
        super.removeViewObscuringAllTabs(view);
        updateAccessibilityVisibility();
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

    @Override
    public void onOverviewModeStartedShowing(boolean showToolbar) {
        if (getFindToolbarManager() != null) getFindToolbarManager().hideToolbar();
        if (getAssistStatusHandler() != null) getAssistStatusHandler().updateAssistState();
        if (getAppMenuHandler() != null) getAppMenuHandler().hideAppMenu();
        setStatusBarColor(null, Color.BLACK);
    }

    @Override
    public void onOverviewModeFinishedShowing() {}

    @Override
    public void onOverviewModeStartedHiding(boolean showToolbar, boolean delayAnimation) {
        if (getAppMenuHandler() != null) getAppMenuHandler().hideAppMenu();
    }

    @Override
    public void onOverviewModeFinishedHiding() {
        if (getAssistStatusHandler() != null) getAssistStatusHandler().updateAssistState();
        if (getActivityTab() != null) {
            setStatusBarColor(getActivityTab(), getActivityTab().getThemeColor());
        }
    }

    @Override
    protected void setStatusBarColor(@Nullable Tab tab, int color) {
        if (isTablet() || UiUtils.isSystemUiThemingDisabled()) return;

        if (!isInOverviewMode()) {
            super.setStatusBarColor(tab, color);
            return;
        }

        boolean supportsDarkStatusIcons = Build.VERSION.SDK_INT >= Build.VERSION_CODES.M;
        if (!supportsDarkStatusIcons) {
            super.setStatusBarColor(tab, Color.BLACK);
            return;
        }

        if (!ChromeFeatureList.isInitialized()
                || (!ChromeFeatureList.isEnabled(ChromeFeatureList.HORIZONTAL_TAB_SWITCHER_ANDROID)
                           && !DeviceClassManager.enableAccessibilityLayout())) {
            super.setStatusBarColor(tab,
                    ApiCompatibilityUtils.getColor(getResources(), R.color.modern_primary_color));
            return;
        }

        if (mTabModelSelectorImpl != null && mTabModelSelectorImpl.isIncognitoSelected()) {
            super.setStatusBarColor(tab,
                    ApiCompatibilityUtils.getColor(
                            getResources(), R.color.incognito_modern_primary_color));
        } else {
            super.setStatusBarColor(tab,
                    ApiCompatibilityUtils.getColor(getResources(), R.color.modern_primary_color));
        }
    }

    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        super.onMultiWindowModeChanged(isInMultiWindowMode);
        if (!FeatureUtilities.isTabModelMergingEnabled() || !didFinishNativeInitialization()) {
            return;
        }
        // Navigation Popup does not move with multi-window bar.
        // When multi-window starts, the popup is cut off, and the cut off portion is inaccessible.
        // When the window size changes through multi-window, the popup windows position does not
        // change. Because of these problems, dismiss the popup window until a solution appears.
        if (mNavigationPopup != null) mNavigationPopup.dismiss();

        if (!isInMultiWindowMode) {
            // If the activity is currently resumed when multi-window mode is exited, try to merge
            // tabs from the other activity instance.
            if (ApplicationStatus.getStateForActivity(this) == ActivityState.RESUMED) {
                maybeMergeTabs();
            } else {
                mMergeTabsOnResume = true;
            }
        }
    }

    /**
     * Writes the tab state to disk.
     */
    @VisibleForTesting
    public void saveState() {
        mTabModelSelectorImpl.saveState();
    }

    @TargetApi(Build.VERSION_CODES.M)
    private void killOtherTask() {
        if (!FeatureUtilities.isTabModelMergingEnabled()) return;

        Class<?> otherWindowActivityClass =
                MultiWindowUtils.getInstance().getOpenInOtherWindowActivity(this);

        // 1. Find the other activity's task if it's still running so that it can be removed from
        //    Android recents.
        ActivityManager activityManager = (ActivityManager) getSystemService(ACTIVITY_SERVICE);
        List<ActivityManager.AppTask> appTasks = activityManager.getAppTasks();
        ActivityManager.AppTask otherActivityTask = null;
        for (ActivityManager.AppTask task : appTasks) {
            if (task.getTaskInfo() == null || task.getTaskInfo().baseActivity == null) continue;
            String baseActivity = task.getTaskInfo().baseActivity.getClassName();
            // Contrary to the documentation task.getTaskInfo().baseActivity for the .LauncherMain
            // activity alias is the alias itself, and not the implementation. Filed b/66729258;
            // for now translate the alias manually.
            if (baseActivity.equals(MAIN_LAUNCHER_ACTIVITY_NAME)) {
                baseActivity = ChromeTabbedActivity.class.getName();
            }
            if (baseActivity.equals(otherWindowActivityClass.getName())) {
                otherActivityTask = task;
            }
        }

        if (otherActivityTask != null) {
            for (WeakReference<Activity> activityRef : ApplicationStatus.getRunningActivities()) {
                Activity activity = activityRef.get();
                if (activity == null) continue;
                // 2. If the other activity is still running (not destroyed), save its tab list.
                //    Saving the tab list prevents missing tabs or duplicate tabs if tabs have been
                //    reparented.
                // TODO(twellington): saveState() gets called in onStopWithNative() after the merge
                // starts, causing some duplicate work to be done. Avoid the redundancy.
                if (activity.getClass().equals(otherWindowActivityClass)) {
                    ((ChromeTabbedActivity) activity).saveState();
                    break;
                }
            }
            // 3. Kill the other activity's task to remove it from Android recents.
            otherActivityTask.finishAndRemoveTask();
        }
        setMergedInstanceTaskId(getTaskId());
    }

    /**
     * Merges tabs from a second ChromeTabbedActivity instance if necesssary and calls
     * finishAndRemoveTask() on the other activity.
     */
    @TargetApi(Build.VERSION_CODES.M)
    @VisibleForTesting
    public void maybeMergeTabs() {
        if (!FeatureUtilities.isTabModelMergingEnabled()) return;

        killOtherTask();

        // 4. Ask TabPersistentStore to merge state.
        RecordUserAction.record("Android.MergeState.Live");
        mTabModelSelectorImpl.mergeState();
    }

    @Override
    public void onEnterVr() {
        super.onEnterVr();
        mControlContainer.setVisibility(View.INVISIBLE);
    }

    @Override
    public void onExitVr() {
        super.onExitVr();
        mControlContainer.setVisibility(View.VISIBLE);
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

    @Override
    protected ChromeFullscreenManager createFullscreenManager() {
        return new ChromeFullscreenManager(this, ChromeFullscreenManager.ControlsPosition.TOP);
    }

    /**
     * Should be called when multi-instance mode is started.
     */
    public static void onMultiInstanceModeStarted() {
        // When a second instance is created, the merged instance task id should be cleared.
        setMergedInstanceTaskId(0);
    }

    private static void setMergedInstanceTaskId(int mergedInstanceTaskId) {
        sMergedInstanceTaskId = mergedInstanceTaskId;
    }

    @SuppressLint("NewApi")
    private boolean isMergedInstanceTaskRunning() {
        if (!FeatureUtilities.isTabModelMergingEnabled() || sMergedInstanceTaskId == 0) {
            return false;
        }

        ActivityManager manager = (ActivityManager) getSystemService(Context.ACTIVITY_SERVICE);
        for (AppTask task : manager.getAppTasks()) {
            RecentTaskInfo info = DocumentUtils.getTaskInfoFromTask(task);
            if (info == null) continue;
            if (info.id == sMergedInstanceTaskId) return true;
        }
        return false;
    }

    @Override
    public boolean supportsFullscreenActivity() {
        return !VrModuleProvider.getDelegate().isInVr();
    }

    @Override
    public boolean supportsContextualSuggestionsBottomSheet() {
        return true;
    }

    @Override
    public void onScreenshotTaken() {
        Tracker tracker = TrackerFactory.getTrackerForProfile(Profile.getLastUsedProfile());
        tracker.notifyEvent(EventConstants.SCREENSHOT_TAKEN_CHROME_IN_FOREGROUND);

        ThreadUtils.postOnUiThread(new Runnable() {
            @Override
            public void run() {
                getToolbarManager().showDownloadPageTextBubble(
                        getActivityTab(), FeatureConstants.DOWNLOAD_PAGE_SCREENSHOT_FEATURE);
                ScreenshotTabObserver tabObserver = ScreenshotTabObserver.from(getActivityTab());
                if (tabObserver != null) tabObserver.onScreenshotTaken();
            }
        });
    }

    @Override
    protected PageViewTimer createPageViewTimer() {
        return new PageViewTimer(mTabModelSelectorImpl, mLayoutManager);
    }
}
