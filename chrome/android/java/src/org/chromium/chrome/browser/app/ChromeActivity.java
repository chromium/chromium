// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app;

import android.annotation.SuppressLint;
import android.app.Activity;
import android.app.Fragment;
import android.app.SearchManager;
import android.app.assist.AssistContent;
import android.content.Intent;
import android.content.res.Configuration;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;
import android.util.Pair;
import android.util.TypedValue;
import android.view.Display.Mode;
import android.view.MenuItem;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.CallSuper;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.RequiresApi;
import androidx.annotation.VisibleForTesting;

import com.ark.browser.ArkBrowserActivity;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.Callback;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.MathUtils;
import org.chromium.base.PowerMonitor;
import org.chromium.base.StrictModeContext;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.supplier.UnownedUserDataSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.AppHooks;
import org.chromium.chrome.browser.ChromeActionModeHandler;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.ChromeKeyboardVisibilityDelegate;
import org.chromium.chrome.browser.ChromeTabbedActivity2;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.DeferredStartupHandler;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.IntentHandlerDelegate;
import org.chromium.chrome.browser.IntentHandler.TabOpenType;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.browser.app.metrics.LaunchCauseMetrics;
import org.chromium.chrome.browser.app.tab_activity_glue.ReparentingDelegateFactory;
import org.chromium.chrome.browser.app.tab_activity_glue.TabReparentingController;
import org.chromium.chrome.browser.app.tabmodel.AsyncTabParamsManagerSingleton;
import org.chromium.chrome.browser.app.tabmodel.TabModelOrchestrator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutManagerImpl;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.compositor.layouts.SceneChangeObserver;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManagerHandler;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchFieldTrial;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager.ContextualSearchTabPromotionDelegate;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorNotificationBridgeUiFactory;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSessionState;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManagerSupplier;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.gsa.ContextReporter;
import org.chromium.chrome.browser.gsa.GSAAccountChangeListener;
import org.chromium.chrome.browser.gsa.GSAState;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.init.ProcessInitializationHandler;
import org.chromium.chrome.browser.init.StartupTabPreloader;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponent;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentFactory;
import org.chromium.chrome.browser.keyboard_accessory.ManualFillingComponentSupplier;
import org.chromium.chrome.browser.layouts.LayoutManagerAppUtils;
import org.chromium.chrome.browser.media.PictureInPictureController;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.omnibox.geo.GeolocationHeader;
import org.chromium.chrome.browser.partnercustomizations.PartnerBrowserCustomizations;
import org.chromium.chrome.browser.preferences.Pref;
import com.ark.browser.core.utils.TabPrinter;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.tab.AccessibilityVisibilityHandler;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.RequestDesktopUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tabmodel.EmptyTabModel;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabCreatorManagerSupplier;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelInitializer;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorProfileSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorSupplier;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import com.ark.browser.translate.TranslateAssistContent;
import com.ark.browser.utils.ArkLogger;

import org.chromium.chrome.browser.ui.BottomContainer;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.chrome.browser.ui.messages.snackbar.Snackbar;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager.SnackbarManageable;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManagerProvider;
import org.chromium.components.browser_ui.accessibility.FontSizePrefs;
import org.chromium.components.browser_ui.modaldialog.AppModalPresenter;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.browser_ui.widget.InsetObserverViewSupplier;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.textbubble.TextBubble;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.feature_engagement.EventConstants;
import org.chromium.components.policy.CombinedPolicyProvider;
import org.chromium.components.policy.CombinedPolicyProvider.PolicyChangeListener;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.content_public.browser.ActionModeCallbackHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.common.ContentSwitches;
import org.chromium.printing.PrintManagerDelegateImpl;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.Clipboard;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.display.DisplayAndroid;
import org.chromium.ui.display.DisplayAndroid.DisplayAndroidObserver;
import org.chromium.ui.display.DisplayUtil;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * A {@link AsyncInitializationActivity} that builds and manages a {@link CompositorViewHolder}
 * and associated classes.
 */
public abstract class ChromeActivity
        extends AsyncInitializationActivity
        implements TabCreatorManager, PolicyChangeListener, ContextualSearchTabPromotionDelegate,
                   SnackbarManageable, SceneChangeObserver,
                   MenuOrKeyboardActionController, CompositorViewHolder.Initializer,
                   TabModelInitializer {
    private static final String TAG = "ChromeActivity";

    private final ObservableSupplierImpl<TabModelOrchestrator> mTabModelOrchestratorSupplier =
            new ObservableSupplierImpl<>();
    /** Used to access the {@link TabModelSelector} from {@link WindowAndroid}. */
    private final UnownedUserDataSupplier<TabModelSelector> mTabModelSelectorSupplier =
            new TabModelSelectorSupplier();
    /** Used to access the {@link TabCreatorManager} from {@link WindowAndroid}. */
    private final UnownedUserDataSupplier<TabCreatorManager> mTabCreatorManagerSupplier =
            new TabCreatorManagerSupplier();
    private final UnownedUserDataSupplier<ManualFillingComponent> mManualFillingComponentSupplier =
            new ManualFillingComponentSupplier();
    // TODO(crbug.com/1209864): Move ownership to RootUiCoordinator.
    private final UnownedUserDataSupplier<BrowserControlsManager> mBrowserControlsManagerSupplier =
            new BrowserControlsManagerSupplier();

    protected TabModelSelectorProfileSupplier mTabModelProfileSupplier =
            new TabModelSelectorProfileSupplier(mTabModelSelectorSupplier);
    private TabModelOrchestrator mTabModelOrchestrator;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    private ObservableSupplierImpl<TabContentManager> mTabContentManagerSupplier =
            new ObservableSupplierImpl<>();
    private TabContentManager mTabContentManager;

    private ContextReporter mContextReporter;

    private boolean mPartnerBrowserRefreshNeeded;

    protected final IntentHandler mIntentHandler;

    /** Set if {@link #postDeferredStartupIfNeeded()} is called before native has loaded. */
    private boolean mDeferredStartupQueued;

    /** Whether or not {@link #postDeferredStartupIfNeeded()} has already successfully run. */
    private boolean mDeferredStartupPosted;

    private boolean mNativeInitialized;
    private boolean mRemoveWindowBackgroundDone;
    protected AccessibilityVisibilityHandler mAccessibilityVisibilityHandler;

    // The PictureInPictureController is initialized lazily https://crbug.com/729738.
    private PictureInPictureController mPictureInPictureController;

    private ObservableSupplierImpl<CompositorViewHolder> mCompositorViewHolderSupplier =
            new ObservableSupplierImpl<>();
    private ObservableSupplierImpl<LayoutManagerImpl> mLayoutManagerSupplier =
            new ObservableSupplierImpl<>();
    protected final UnownedUserDataSupplier<InsetObserverView> mInsetObserverViewSupplier =
            new InsetObserverViewSupplier();
    private final ObservableSupplierImpl<ContextualSearchManager> mContextualSearchManagerSupplier =
            new ObservableSupplierImpl<>();

    private SnackbarManager mSnackbarManager;

    // Timestamp in ms when initial layout inflation begins
    private long mInflateInitialLayoutBeginMs;
    // Timestamp in ms when initial layout inflation ends
    private long mInflateInitialLayoutEndMs;

    /** Whether or not a PolicyChangeListener was added. */
    private boolean mDidAddPolicyChangeListener;

    /** A means of providing the foreground tab of the activity to different features. */
    private ActivityTabProvider mActivityTabProvider = new ActivityTabProvider();

    /** Whether or not the activity is in started state. */
    private boolean mStarted;

    /** The data associated with the most recently selected menu item. */
    @Nullable
    private Bundle mMenuItemData;

    /**
     * The current configuration, used to for diffing when the configuration is changed.
     */
    private Configuration mConfig;

    /**
     * Supplier of the instance to control the tab-reparenting tasks.
     */
    private OneshotSupplierImpl<TabReparentingController> mTabReparentingControllerSupplier =
            new OneshotSupplierImpl<>();

    /**
     * Track whether {@link #mTabReparentingController} has prepared tab reparenting.
     */
    private boolean mIsTabReparentingPrepared;

    /**
     * Listen to display change and start tab-reparenting if necessary.
     */
    private DisplayAndroidObserver mDisplayAndroidObserver;

    @Nullable
    private BottomContainer mBottomContainer;

    @Nullable
    private StartupTabPreloader mStartupTabPreloader;

    private LaunchCauseMetrics mLaunchCauseMetrics;

    private GSAAccountChangeListener mGSAAccountChangeListener;

    // TODO(972867): Pull MenuOrKeyboardActionController out of ChromeActivity.
    private List<MenuOrKeyboardActionController.MenuOrKeyboardActionHandler> mMenuActionHandlers =
            new ArrayList<>();

    // Whether this Activity is in Picture in Picture mode, based on the most recent call to
    // {@link onPictureInPictureModeChanged} from the platform.  This might disagree with the value
    // returned by {@link isInPictureInPictureMode}.
    private boolean mLastPictureInPictureModeForTesting;

    protected ChromeActivity() {
        mIntentHandler = new IntentHandler(this, createIntentHandlerDelegate());
        mManualFillingComponentSupplier.set(ManualFillingComponentFactory.createComponent());
    }

    @Override
    protected void onPreCreate() {
        CachedFeatureFlags.onStartOrResumeCheckpoint();
        super.onPreCreate();
        initializeBackPressHandling();
    }

    @Override
    protected void onAbortCreate() {
        super.onAbortCreate();
        CachedFeatureFlags.onPauseCheckpoint();
    }

    @Override
    protected ActivityWindowAndroid createWindowAndroid() {
        return new ChromeWindow(/* activity= */ this, mActivityTabProvider,
                mCompositorViewHolderSupplier, getModalDialogManagerSupplier(),
                mManualFillingComponentSupplier, getIntentRequestTracker());
    }

    @Override
    public boolean onIntentCallbackNotFoundError(String error) {
        createWindowErrorSnackbar(error, mSnackbarManager);
        return true;
    }

    @VisibleForTesting
    public static void createWindowErrorSnackbar(String error, SnackbarManager snackbarManager) {
        if (snackbarManager != null) {
            Snackbar snackbar = Snackbar.make(
                    error, null, Snackbar.TYPE_NOTIFICATION, Snackbar.UMA_WINDOW_ERROR);
            snackbar.setSingleLine(false);
            snackbar.setDuration(SnackbarManager.DEFAULT_SNACKBAR_DURATION_LONG_MS);
            snackbarManager.showSnackbar(snackbar);
        }
    }

    @Override
    public void performPreInflationStartup() {
        setupUnownedUserDataSuppliers();

        // Ensure that mConfig is initialized before tablet mode changes.
        mConfig = getResources().getConfiguration();

        // Create the orchestrator that manages Tab models and persistence
        mTabModelOrchestrator = createTabModelOrchestrator();
        mTabModelOrchestratorSupplier.set(mTabModelOrchestrator);

        super.performPreInflationStartup();

        // Force a partner customizations refresh if it has yet to be initialized.  This can happen
        // if Chrome is killed and you refocus a previous activity from Android recents, which does
        // not go through ChromeLauncherActivity that would have normally triggered this.
        mPartnerBrowserRefreshNeeded = !PartnerBrowserCustomizations.getInstance().isInitialized();

        CommandLine commandLine = CommandLine.getInstance();
        if (!commandLine.hasSwitch(ChromeSwitches.DISABLE_FULLSCREEN)) {
            TypedValue threshold = new TypedValue();
            getResources().getValue(R.dimen.top_controls_show_threshold, threshold, true);
            commandLine.appendSwitchWithValue(ContentSwitches.TOP_CONTROLS_SHOW_THRESHOLD,
                    threshold.coerceToString().toString());
            getResources().getValue(R.dimen.top_controls_hide_threshold, threshold, true);
            commandLine.appendSwitchWithValue(ContentSwitches.TOP_CONTROLS_HIDE_THRESHOLD,
                    threshold.coerceToString().toString());
        }

        getWindow().setBackgroundDrawable(getBackgroundDrawable());
    }

    private void setupUnownedUserDataSuppliers() {
        mTabModelSelectorSupplier.attach(getWindowAndroid().getUnownedUserDataHost());
        mTabCreatorManagerSupplier.attach(getWindowAndroid().getUnownedUserDataHost());
        mManualFillingComponentSupplier.attach(getWindowAndroid().getUnownedUserDataHost());
        mInsetObserverViewSupplier.attach(getWindowAndroid().getUnownedUserDataHost());
        mBrowserControlsManagerSupplier.attach(getWindowAndroid().getUnownedUserDataHost());
        // BrowserControlsManager is ready immediately.
        mBrowserControlsManagerSupplier.set(
                new BrowserControlsManager(this, BrowserControlsManager.ControlsPosition.TOP));
    }

    private NotificationManagerProxy getNotificationManagerProxy() {
        return new NotificationManagerProxyImpl(getApplicationContext());
    }

    @SuppressLint("NewApi")
    @Override
    public void performPostInflationStartup() {
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.performPostInflationStartup")) {
            super.performPostInflationStartup();

            Intent intent = getIntent();
            if (0 != (intent.getFlags() & Intent.FLAG_ACTIVITY_LAUNCHED_FROM_HISTORY)) {
                getLaunchCauseMetrics().onLaunchFromRecents();
            } else {
                getLaunchCauseMetrics().onReceivedIntent();
            }

            mBottomContainer = (BottomContainer) findViewById(R.id.bottom_container);

            // TODO(crbug.com/1199776): Move this to the RootUiCoordinator.
            mSnackbarManager = new SnackbarManager(this, mBottomContainer, getWindowAndroid());
            SnackbarManagerProvider.attach(getWindowAndroid(), mSnackbarManager);

            // Make the activity listen to policy change events
            CombinedPolicyProvider.get().addPolicyChangeListener(this);
            mDidAddPolicyChangeListener = true;

            // Set up the animation placeholder to be the SurfaceView. This disables the
            // SurfaceView's 'hole' clipping during animations that are notified to the window.
            getWindowAndroid().setAnimationPlaceholderView(
                    mCompositorViewHolderSupplier.get().getCompositorView());

            initializeTabModels();
            TabModelSelector tabModelSelector = mTabModelOrchestrator.getTabModelSelector();
            setTabContentManager(new TabContentManager(this, getContentOffsetProvider(),
                    !SysUtils.isLowEndDevice(),
                    tabModelSelector != null ? tabModelSelector::getTabById : null));

            if (!isFinishing()) {
                getBrowserControlsManager().initialize(
                        getActivityTabProvider(), getTabModelSelector());
            }

            mBottomContainer.initialize(getBrowserControlsManager(),
                    getWindowAndroid().getApplicationBottomInsetProvider(),
                    mManualFillingComponentSupplier.get().getBottomInsetSupplier());

            // If onStart was called before postLayoutInflation (because inflation was done in a
            // background thread) then make sure to call the relevant methods belatedly.
            if (mStarted) {
                mCompositorViewHolderSupplier.get().onStart();
            }


            // ActionMode功能，不能移除
            ChromeActionModeHandler mChromeActionModeHandler = new ChromeActionModeHandler(mActivityTabProvider,
                    aBoolean -> {}, (searchText) -> {
                if (mTabModelSelectorSupplier.get() == null) return;

                String query = ActionModeCallbackHelper.sanitizeQuery(
                        searchText, ActionModeCallbackHelper.MAX_SEARCH_QUERY_LENGTH);
                if (TextUtils.isEmpty(query)) return;

                Tab tab = mActivityTabProvider.get();
                TrackerFactory
                        .getTrackerForProfile(Profile.fromWebContents(tab.getWebContents()))
                        .notifyEvent(EventConstants.WEB_SEARCH_PERFORMED);

                mTabModelSelectorSupplier.get().openNewTab(
                        generateUrlParamsForSearch(tab, query),
                        TabLaunchType.FROM_LONGPRESS_FOREGROUND, tab, tab.isIncognito());
            });
        }
    }

    /**
     * Generate the LoadUrlParams necessary to load the specified search query.
     */
    private static LoadUrlParams generateUrlParamsForSearch(Tab tab, String query) {
        String url = TemplateUrlServiceFactory.get().getUrlForSearchQuery(query);
        String headers = GeolocationHeader.getGeoHeader(url, tab);

        LoadUrlParams loadUrlParams = new LoadUrlParams(url);
        loadUrlParams.setVerbatimHeaders(headers);
        loadUrlParams.setTransitionType(PageTransition.GENERATED);
        return loadUrlParams;
    }

    @Override
    protected void initializeStartupMetrics() {
        // Initialize the activity session tracker as early as possible so that
        // it can start background tasks.
        ChromeActivitySessionTracker chromeActivitySessionTracker =
                ChromeActivitySessionTracker.getInstance();
        chromeActivitySessionTracker.registerTabModelSelectorSupplier(
                this, mTabModelSelectorSupplier);
    }

    @Override
    protected View getViewToBeDrawnBeforeInitializingNative() {
        return super.getViewToBeDrawnBeforeInitializingNative();
    }

    /**
     * This function triggers the layout inflation. If subclasses override {@link
     * #doLayoutInflation}, no calls to {@link #getCompositorViewHolderSupplier().get()} can be done
     * until inflation is complete and {@link #onInitialLayoutInflationComplete()} is called. If the
     * subclass does not override {@link #doLayoutInflation}, then {@link
     * #getCompositorViewHolderSupplier().get()} is safe to be called after calling super.
     */
    @Override
    protected final void triggerLayoutInflation() {
        mInflateInitialLayoutBeginMs = SystemClock.elapsedRealtime();
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.triggerLayoutInflation")) {
            SelectionPopupController.setShouldGetReadbackViewFromWindowAndroid();

            enableHardwareAcceleration();
            setLowEndTheme();

            WarmupManager warmupManager = WarmupManager.getInstance();
            warmupManager.clearViewHierarchy();
            doLayoutInflation();
        }
    }

    /**
     * This function implements the actual layout inflation, Subclassing Activities that override
     * this method without calling super need to call {@link #onInitialLayoutInflationComplete()}.
     */
    protected void doLayoutInflation() {
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.doLayoutInflation")) {
            // Allow disk access for the content view and toolbar container setup.
            // On certain android devices this setup sequence results in disk writes outside
            // of our control, so we have to disable StrictMode to work. See
            // https://crbug.com/639352.
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                TraceEvent.begin("setContentView(R.layout.main)");
                setContentView(R.layout.main);


                TextView tvBack = findViewById(R.id.tv_back);
                TextView tvForward = findViewById(R.id.tv_forward);

                tvBack.setOnClickListener(v -> {
                    Tab tab = getActivityTab();
                    if (tab != null && tab.canGoBack()) {
                        tab.goBack();
                    } else {
                        Toast.makeText(this, "cant go back!", Toast.LENGTH_SHORT).show();
                    }
                });

                tvForward.setOnClickListener(v -> {
                    Tab tab = getActivityTab();
                    if (tab != null && tab.canGoForward()) {
                        tab.goForward();
                    } else {
                        Toast.makeText(this, "cant go forward!", Toast.LENGTH_SHORT).show();
                    }
                });

                EditText etUrl = findViewById(R.id.et_url);
                etUrl.setMaxLines(2);
                TextView tvGo = findViewById(R.id.tv_go);
                tvGo.setOnClickListener(v -> {
                    Tab tab = getActivityTab();
                    if (tab != null) {
                        String url = etUrl.getText().toString();
                        if (TextUtils.equals(url, tab.getUrl().getSpec())) {
                            Toast.makeText(ContextUtils.getApplicationContext(), "same url!", Toast.LENGTH_SHORT).show();
                            return;
                        }
                        LoadUrlParams params = new LoadUrlParams(etUrl.getText().toString()
                                , PageTransition.LINK);
                        tab.loadUrl(params);
                    }
                });

                mTabModelSelectorSupplier.addObserver(new Callback<TabModelSelector>() {
                    @Override
                    public void onResult(TabModelSelector tabModelSelector) {
                        tabModelSelector.addObserver(new TabModelSelectorObserver() {

                            private final EmptyTabObserver observer = new EmptyTabObserver() {

                                @Override
                                public void onPageLoadStarted(Tab tab, GURL url) {
                                    etUrl.setText(tab.getUrl().getSpec());
                                }

                                @Override
                                public void onPageLoadFinished(Tab tab, GURL url) {
                                    etUrl.setText(tab.getUrl().getSpec());
                                }
                            };

                            @Override
                            public void onTabModelSelected(TabModel newModel, TabModel oldModel) {
                                onTabChanged(getActivityTab());
                            }

                            @Override
                            public void onChange() {
                                onTabChanged(getActivityTab());
                            }

                            private void onTabChanged(Tab tab) {
                                if (tab != null) {
                                    if (!tab.hasObserver(observer)) {
                                        tab.addObserver(observer);
                                    }
                                    etUrl.setText(tab.getUrl().getSpec());
                                }
                            }

                        });
                    }
                });

                TextView tvRefresh = findViewById(R.id.tv_refresh);
                tvRefresh.setOnClickListener(v -> {
//                    Tab tab = getActivityTab();
//                    if (tab != null) {
//                        tab.reload();
//                    }
                    Intent intent = new Intent(ChromeActivity.this, ChromeTabbedActivity2.class);
                    startActivity(intent);
                });

                TextView tvTest = findViewById(R.id.tv_test);
                tvTest.setOnClickListener(v -> {
                    Intent intent = new Intent(ChromeActivity.this, ArkBrowserActivity.class);
                    startActivity(intent);
                });


                TraceEvent.end("setContentView(R.layout.main)");

            }
            onInitialLayoutInflationComplete();
        }
    }

    @Override
    protected void onInitialLayoutInflationComplete() {
        mInflateInitialLayoutEndMs = SystemClock.elapsedRealtime();

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();
        mCompositorViewHolderSupplier.set(
                (CompositorViewHolder) findViewById(R.id.compositor_view_holder));

        // If the UI was inflated on a background thread, then the CompositorView may not have been
        // fully initialized yet as that may require the creation of a handler which is not allowed
        // outside the UI thread. This call should fully initialize the CompositorView if it hasn't
        // been yet.
        mCompositorViewHolderSupplier.get().setRootView(rootView);

        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);

        // Add a custom view right after the root view that stores the insets to access later.
        // WebContents needs the insets to determine the portion of the screen obscured by
        // non-content displaying things such as the OSK.
        mInsetObserverViewSupplier.set(InsetObserverView.create(this));
        rootView.addView(mInsetObserverViewSupplier.get(), 0);

        super.onInitialLayoutInflationComplete();
    }

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public final void initializeTabModels() {
        if (mTabModelOrchestrator.areTabModelsInitialized()) return;

        createTabModels();
        TabModelSelector tabModelSelector = mTabModelOrchestrator.getTabModelSelector();

        if (tabModelSelector == null) {
            assert isFinishing();
            return;
        }

        mTabModelSelectorSupplier.set(tabModelSelector);
        mActivityTabProvider.setTabModelSelector(tabModelSelector);

        Pair<? extends TabCreator, ? extends TabCreator> tabCreators = createTabCreators();
        mTabCreatorManagerSupplier.set(
                incognito -> incognito ? tabCreators.second : tabCreators.first);

        OfflinePageUtils.observeTabModelSelector(this, tabModelSelector);
        if (mTabModelSelectorTabObserver != null) mTabModelSelectorTabObserver.destroy();

        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(tabModelSelector) {
            @Override
            public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                postDeferredStartupIfNeeded();
            }

            @Override
            public void onPageLoadFinished(Tab tab, GURL url) {
                postDeferredStartupIfNeeded();
                OfflinePageUtils.showOfflineSnackbarIfNecessary(tab);
            }

            @Override
            public void onCrash(Tab tab) {
                postDeferredStartupIfNeeded();
            }
        };
    }

    /**
     * @return The {@link StartupTabPreloader} associated with this ChromeActivity. If there isn't
     *         one it creates it.
     */
    protected StartupTabPreloader getStartupTabPreloader() {
        if (mStartupTabPreloader == null) {
            mStartupTabPreloader = new StartupTabPreloader(this::getIntent,
                    getLifecycleDispatcher(), getWindowAndroid(), this, mIntentHandler);
        }
        return mStartupTabPreloader;
    }

    /**
     * @return The {@link TabModelOrchestrator} owned by this {@link ChromeActivity}.
     */
    protected abstract TabModelOrchestrator createTabModelOrchestrator();

    /**
     * Call the {@link TabModelOrchestrator} to initialize its members.
     */
    protected abstract void createTabModels();

    /**
     * Call the {@link TabModelOrchestrator} to destroy its members.
     */
    protected abstract void destroyTabModels();

    /**
     * @return The {@link TabCreator}s owned
     *         by this {@link ChromeActivity}.  The first item in the Pair is the normal model tab
     *         creator, and the second is the tab creator for incognito tabs.
     */
    protected abstract Pair<? extends TabCreator, ? extends TabCreator> createTabCreators();

    /**
     * @return The {@link ManualFillingComponent} that belongs to this activity.
     */
    public ManualFillingComponent getManualFillingComponent() {
        return mManualFillingComponentSupplier.get();
    }

    /**
     * @return The {@link LaunchCauseMetrics} to be owned by this {@link ChromeActivity}.
     */
    protected abstract LaunchCauseMetrics createLaunchCauseMetrics();

    private LaunchCauseMetrics getLaunchCauseMetrics() {
        if (mLaunchCauseMetrics == null) {
            mLaunchCauseMetrics = createLaunchCauseMetrics();
        }
        return mLaunchCauseMetrics;
    }

    @Override
    public void initializeState() {
        super.initializeState();

        IntentHandler.setTestIntentsEnabled(
                CommandLine.getInstance().hasSwitch(ContentSwitches.ENABLE_TEST_INTENTS));
    }

    @Override
    public void initializeCompositor() {
        TraceEvent.begin("ChromeActivity:CompositorInitialization");
        super.initializeCompositor();

        getTabContentManager().initWithNative();
        mCompositorViewHolderSupplier.get().onNativeLibraryReady(
                getWindowAndroid(), getTabContentManager());

        // TODO(1107916): Move contextual search initialization to the RootUiCoordinator.
        ContextualSearchFieldTrial.setEnabled(true);
        ArkLogger.e(TAG, "initializeCompositor ContextualSearch isEnabled=" + ContextualSearchFieldTrial.isEnabled());
        if (ContextualSearchFieldTrial.isEnabled()) {
            mContextualSearchManagerSupplier.set(new ContextualSearchManager(this, this,
                    getActivityTabProvider(),
                    getFullscreenManager(), getBrowserControlsManager(), getWindowAndroid(),
                    getTabModelSelectorSupplier().get(), () -> getLastUserInteractionTime()));
        }

        TraceEvent.end("ChromeActivity:CompositorInitialization");
    }

    @Override
    public void onStartWithNative() {
        assert mNativeInitialized : "onStartWithNative was called before native was initialized.";
        super.onStartWithNative();
        ChromeActivitySessionTracker.getInstance().onStartWithNative();
        ChromeCachedFlags.getInstance().cacheNativeFlags();

        // postDeferredStartupIfNeeded() is called in TabModelSelectorTabObsever#onLoadStopped(),
        // #onPageLoadFinished() and #onCrash(). If we are not actively loading a tab (e.g.
        // in Android N multi-instance, which is created by re-parenting an existing tab),
        // ensure onDeferredStartup() gets called by calling postDeferredStartupIfNeeded() here.
        if (mDeferredStartupQueued || shouldPostDeferredStartupForReparentedTab()) {
            postDeferredStartupIfNeeded();
        }
    }

    /**
     * Returns whether deferred startup should be run if we are not actively loading a tab (e.g.
     * in Android N multi-instance, which is created by re-parenting an existing tab).
     */
    public boolean shouldPostDeferredStartupForReparentedTab() {
        return getActivityTab() == null || !getActivityTab().isLoading();
    }

    private void onActivityShown() {
        maybeRemoveWindowBackground();

        Tab tab = getActivityTab();
        if (tab != null) {
            if (tab.isHidden()) {
                tab.show(TabSelectionType.FROM_USER);
            } else {
                // The visible Tab's renderer process may have died after the activity was
                // paused. Ensure that it's restored appropriately.
                tab.loadIfNeeded();
            }
        }

        MultiWindowUtils.getInstance().recordMultiWindowStateUkm(this, tab);
    }

    private void onActivityHidden() {

        Tab tab = getActivityTab();
        TabModelSelector tabModelSelector = mTabModelOrchestrator.getTabModelSelector();
        if (tabModelSelector != null && !tabModelSelector.isReparentingInProgress()
                && tab != null) {
            tab.hide(TabHidingType.ACTIVITY_HIDDEN);
        }
    }

    private boolean useWindowFocusForVisibility() {
        return Build.VERSION.SDK_INT < Build.VERSION_CODES.Q;
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);

        if (useWindowFocusForVisibility()) {
            if (hasFocus) {
                onActivityShown();
            } else {
                if (ApplicationStatus.getStateForActivity(this) == ActivityState.STOPPED) {
                    onActivityHidden();
                }
            }
        }

        Clipboard.getInstance().onWindowFocusChanged(hasFocus);
    }

    /**
     * Returns theme color which should be used when:
     * - Web page does not provide a custom theme color.
     * AND
     * - Browser is in a state where it can be themed (no  intersitial showing etc.)
     * {@link TabState#UNSPECIFIED_THEME_COLOR} should be returned if the activity should use the
     * default color in this scenario.
     */
    public int getActivityThemeColor() {
        return TabState.UNSPECIFIED_THEME_COLOR;
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();

        // Resume the ChromeActivity...

        RecordUserAction.record("MobileComeToForeground");
        getLaunchCauseMetrics().recordLaunchCause();

        Tab tab = getActivityTab();
        if (tab != null) {
            WebContents webContents = tab.getWebContents();

            // For picture-in-picture mode / auto-darken web contents.
            if (webContents != null) webContents.notifyRendererPreferenceUpdate();
        }

        ChromeSessionState.setIsInMultiWindowMode(
                MultiWindowUtils.getInstance().isInMultiWindowMode(this));

        ChromeSessionState.setDarkModeState(false, false);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            ensurePictureInPictureController();
        }
        if (mPictureInPictureController != null) {
            mPictureInPictureController.onFrameworkExitedPictureInPicture();
        }

        getManualFillingComponent().onResume();
    }

    private void ensurePictureInPictureController() {
        if (mPictureInPictureController == null) {
            mPictureInPictureController = new PictureInPictureController(
                    this, getActivityTabProvider(), getFullscreenManager());
        }
    }

    @Override
    protected void onUserLeaveHint() {
        super.onUserLeaveHint();

        getLaunchCauseMetrics().onUserLeaveHint();

        // Can be in finishing state. No need to attempt PIP.
        if (isActivityFinishingOrDestroyed()) return;

        ensurePictureInPictureController();
        mPictureInPictureController.attemptPictureInPicture();
        // The attempt might not be successful.  If it is, then `onPictureInPictureModeChanged` will
        // let us know later.  Note that the activity might report that it is in PictureInPicture
        // mode at any point after this, which might be before we finish setup after receiving
        // notification from mOnPictureInPictureModeChanged.
    }

    /**
     * When we're notified that Picture-in-Picture mode has changed, make sure that the controller
     * is kept up-to-date.
     */
    @Override
    @RequiresApi(api = Build.VERSION_CODES.O)
    public void onPictureInPictureModeChanged(boolean inPicture, Configuration newConfig) {
        super.onPictureInPictureModeChanged(inPicture, newConfig);
        if (inPicture) {
            ensurePictureInPictureController();
            mPictureInPictureController.onEnteredPictureInPictureMode();
            mLastPictureInPictureModeForTesting = true;
        } else if (mPictureInPictureController != null) {
            mLastPictureInPictureModeForTesting = false;
            mPictureInPictureController.onFrameworkExitedPictureInPicture();
        }
    }

    /**
     * Return the status of a Picture-in-Picture transition.  This is separate from
     * {@link isInPictureInPictureMode}, because this will trigger only after we have received and
     * processed an Activity.onPictureInPictureModeChanged call.
     */
    public boolean getLastPictureInPictureModeForTesting() {
        return mLastPictureInPictureModeForTesting;
    }

    @Override
    public void onPauseWithNative() {
        RecordUserAction.record("MobileGoToBackground");
        Tab tab = getActivityTab();
        if (tab != null) getTabContentManager().cacheTabThumbnail(tab);
        getManualFillingComponent().onPause();

        markSessionEnd();

        super.onPauseWithNative();
    }

    @Override
    public void onStopWithNative() {
        if (GSAState.getInstance(this).isGsaAvailable() && !SysUtils.isLowEndDevice()) {
            if (mGSAAccountChangeListener != null) mGSAAccountChangeListener.disconnect();
        }
        if (mContextReporter != null) mContextReporter.disable();

        super.onStopWithNative();
    }

    @Override
    public void onNewIntentWithNative(Intent intent) {
        if (mPictureInPictureController != null) {
            mPictureInPictureController.onFrameworkExitedPictureInPicture();
        }

        super.onNewIntentWithNative(intent);
        getLaunchCauseMetrics().onReceivedIntent();
        if (mIntentHandler.shouldIgnoreIntent(intent, /*startedActivity=*/false)) return;

        mIntentHandler.onNewIntent(intent);
    }

    /**
     * @return The type for this activity.
     */
    @ActivityType
    public abstract int getActivityType();

    /**
     * @return Whether the given activity contains a CustomTab.
     */
    public boolean isCustomTab() {
        return getActivityType() == ActivityType.CUSTOM_TAB
                || getActivityType() == ActivityType.TRUSTED_WEB_ACTIVITY;
    }

    /**
     * Actions that may be run at some point after startup. Place tasks that are not critical to the
     * startup path here.  This method will be called automatically.
     */
    private void onDeferredStartup() {
        initDeferredStartupForActivity();
        ProcessInitializationHandler.getInstance().initializeDeferredStartupTasks();
        DeferredStartupHandler.getInstance().queueDeferredTasksOnIdleHandler();
    }

    /**
     * All deferred startup tasks that require the activity rather than the app should go here.
     *
     * Overriding methods should queue tasks on the DeferredStartupHandler before or after calling
     * super depending on whether the tasks should run before or after these ones.
     */
    @CallSuper
    protected void initDeferredStartupForActivity() {
        DeferredStartupHandler.getInstance().addDeferredTask(() -> {
            if (isActivityFinishingOrDestroyed()) return;

            if (MultiWindowUtils.getInstance().isInMultiWindowMode(ChromeActivity.this)) {
                onDeferredStartupForMultiWindowMode();
            }

            long intentTimestamp = IntentHandler.getTimestampFromIntent(getIntent());
            if (intentTimestamp != -1) {
                recordIntentToCreationTime(getOnCreateTimestampMs() - intentTimestamp);
            }

            recordDisplayDimensions();

            FontSizePrefs.getInstance(Profile.getLastUsedRegularProfile())
                    .recordUserFontPrefOnStartup();
        });

        // GSA connection is not needed on low-end devices because Icing is disabled.
        if (!SysUtils.isLowEndDevice()) {
            if (isActivityFinishingOrDestroyed()) return;
            DeferredStartupHandler.getInstance().addDeferredTask(() -> {
                if (!GSAState.getInstance(this).isGsaAvailable()) {
                    ContextReporter.reportStatus(ContextReporter.STATUS_GSA_NOT_AVAILABLE);
                    return;
                }

                if (mGSAAccountChangeListener == null) {
                    mGSAAccountChangeListener =
                            GSAAccountChangeListener.create(AppHooks.get().createGsaHelper());
                }
                mGSAAccountChangeListener.connect();
            });
        }
    }

    /**
     * Actions that may be run at some point after startup for Android N multi-window mode. Should
     * be called from #onDeferredStartup() if the activity is in multi-window mode.
     */
    private void onDeferredStartupForMultiWindowMode() {
        // If the Activity was launched in multi-window mode, record a user action.
        recordMultiWindowModeChanged(
                /* isInMultiWindowMode= */ true, /* isDeferredStartup= */ true);
    }

    /**
     * Records the time it takes from creating an intent for {@link ChromeActivity} to activity
     * creation, including time spent in the framework.
     * @param timeMs The time from creating an intent to activity creation.
     */
    @CallSuper
    protected void recordIntentToCreationTime(long timeMs) {
        RecordHistogram.recordTimesHistogram("MobileStartup.IntentToCreationTime", timeMs);
    }

    @Override
    public void onStart() {
        // Sometimes mCompositorViewHolder is null, see crbug.com/1057613.
        if (AsyncTabParamsManagerSingleton.getInstance().hasParamsWithTabToReparent()) {
            // TODO(https://crbug.com/1252526): Remove logging once root cause of bug is identified
            //  & fixed.
            Log.i(TAG,
                    "#onStart, num async tabs: "
                            + AsyncTabParamsManagerSingleton.getInstance()
                                      .getAsyncTabParams()
                                      .size());

            if (mCompositorViewHolderSupplier.hasValue()) {
                mCompositorViewHolderSupplier.get().prepareForTabReparenting();
            }
        }
        super.onStart();

        if (!useWindowFocusForVisibility()) {
            onActivityShown();
        }

        if (mPartnerBrowserRefreshNeeded) {
            mPartnerBrowserRefreshNeeded = false;
            PartnerBrowserCustomizations.getInstance().initializeAsync(getApplicationContext());
            PartnerBrowserCustomizations.getInstance().setOnInitializeAsyncFinished(() -> {
                if (PartnerBrowserCustomizations.isIncognitoDisabled()) {
                    terminateIncognitoSession();
                }
            });
        }
        if (mCompositorViewHolderSupplier.hasValue()) mCompositorViewHolderSupplier.get().onStart();

        mStarted = true;
    }

    /**
     * WARNING: DO NOT USE THIS METHOD. PASS TabObscuringHandler TO THE OBJECT CONSTRUCTOR INSTEAD.
     * @return {@link TabObscuringHandler} object.
     */
    public TabObscuringHandler getTabObscuringHandler() {
        return null;
    }

    @Override
    public void onStop() {
        super.onStop();

        if (useWindowFocusForVisibility()) {
            if (!hasWindowFocus()) onActivityHidden();
        } else {
            onActivityHidden();
        }

        // We want to refresh partner browser provider every onStart().
        mPartnerBrowserRefreshNeeded = true;
        if (mCompositorViewHolderSupplier.hasValue()) mCompositorViewHolderSupplier.get().onStop();

        // If postInflationStartup hasn't been called yet (because inflation was done asynchronously
        // and has not yet completed), it no longer needs to do the belated onStart code since we
        // were stopped in the mean time.
        mStarted = false;
    }

    @Override
    @RequiresApi(Build.VERSION_CODES.M)
    public void onProvideAssistContent(AssistContent outContent) {
        Tab tab = getActivityTab();
        boolean inOverviewMode = isInOverviewMode();

        // Attempt to fetch translate data here so we can record UMA even if it won't be attached.
        @Nullable
        String structuredData = TranslateAssistContent.getTranslateDataForTab(tab, inOverviewMode);

        // No information is provided in incognito mode and overview mode.
        if (tab != null && !tab.isIncognito() && !inOverviewMode) {
            outContent.setWebUri(Uri.parse(tab.getUrl().getSpec()));
            if (structuredData != null) {
                outContent.setStructuredData(structuredData);
            }
        }
    }

    @Override
    public long getOnCreateTimestampMs() {
        return super.getOnCreateTimestampMs();
    }

    /**
     * This cannot be overridden in order to preserve destruction order.  Override
     * {@link #onDestroyInternal()} instead to perform clean up tasks.
     */
    @SuppressLint("NewApi")
    @Override
    protected final void onDestroy() {
        if (mContextualSearchManagerSupplier.hasValue()) {
            mContextualSearchManagerSupplier.get().destroy();
            mContextualSearchManagerSupplier.set(null);
        }

        if (mSnackbarManager != null) {
            SnackbarManagerProvider.detach(mSnackbarManager);
        }

        if (mTabModelSelectorTabObserver != null) {
            mTabModelSelectorTabObserver.destroy();
            mTabModelSelectorTabObserver = null;
        }

        // TODO(1168131): Destruction and detaching of the LayoutManager should be moved to the
        //                RootUiCoordinator.
        if (mLayoutManagerSupplier.get() != null) {
            LayoutManagerAppUtils.detach(mLayoutManagerSupplier.get());
        }

        if (mCompositorViewHolderSupplier.hasValue()) {
            CompositorViewHolder compositorViewHolder = mCompositorViewHolderSupplier.get();
            if (compositorViewHolder.getLayoutManager() != null) {
                compositorViewHolder.getLayoutManager().removeSceneChangeObserver(this);
            }
            compositorViewHolder.shutDown();
            mCompositorViewHolderSupplier.set(null);
        }

        onDestroyInternal();

        if (mDidAddPolicyChangeListener) {
            CombinedPolicyProvider.get().removePolicyChangeListener(this);
            mDidAddPolicyChangeListener = false;
        }

        if (mTabContentManager != null) {
            mTabContentManager.destroy();
            mTabContentManager = null;
        }

        if (mTabContentManagerSupplier != null) {
            mTabContentManagerSupplier = null;
        }

        if (mManualFillingComponentSupplier.hasValue()) {
            mManualFillingComponentSupplier.get().destroy();
        }
        mManualFillingComponentSupplier.destroy();

        if (mBrowserControlsManagerSupplier.hasValue()) {
            mBrowserControlsManagerSupplier.get().destroy();
        }
        mBrowserControlsManagerSupplier.destroy();

        destroyTabModels();

        if (mTabModelSelectorSupplier != null) {
            mTabModelSelectorSupplier.destroy();
        }

        if (mBottomContainer != null) {
            mBottomContainer.destroy();
            mBottomContainer = null;
        }

        if (mDisplayAndroidObserver != null) {
            getWindowAndroid().getDisplay().removeObserver(mDisplayAndroidObserver);
            mDisplayAndroidObserver = null;
        }

        mActivityTabProvider.destroy();
        ChromeActivitySessionTracker.getInstance().unregisterTabModelSelectorSupplier(this);

        super.onDestroy();
    }

    /**
     * Override this to perform destruction tasks.  Note that by the time this is called, the
     * {@link CompositorViewHolder} will be destroyed, but the {@link WindowAndroid} and
     * {@link TabModelSelector} will not.
     * <p>
     * After returning from this, the {@link TabModelSelector} will be destroyed followed
     * by the {@link WindowAndroid}.
     */
    protected void onDestroyInternal() {}

    /**
     * @return The unified manager for all snackbar related operations.
     */
    @Override
    public SnackbarManager getSnackbarManager() {
        return mSnackbarManager;
    }

    @Override
    protected ModalDialogManager createModalDialogManager() {
        return new ModalDialogManager(
                new AppModalPresenter(this), ModalDialogManager.ModalDialogType.APP);
    }

    protected Drawable getBackgroundDrawable() {
        return new ColorDrawable(getColor(R.color.light_background_color));
    }

    /**
     * Change the Window background color that will be used as the resizing background color on
     * Android N+ multi-window mode. Note that subclasses can override this behavior accordingly in
     * case there is already a Window background Drawable and don't want it to be replaced with the
     * ColorDrawable.
     */
    protected void changeBackgroundColorForResizing() {
        getWindow().setBackgroundDrawable(
                new ColorDrawable(getColor(R.color.resizing_background_color)));
    }

    private void maybeRemoveWindowBackground() {
        // Only need to do this logic once.
        if (mRemoveWindowBackgroundDone) return;

        // Remove the window background only after native init and window getting focus. It's done
        // after native init because before native init, a fake background gets shown. The window
        // focus dependency is because doing it earlier can cause drawing bugs, e.g. crbug/673831.
        if (!mNativeInitialized || !hasWindowFocus()) return;

        // The window background color is used as the resizing background color in Android N+
        // multi-window mode. See crbug.com/602366.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            changeBackgroundColorForResizing();
        } else {
            // Post the removeWindowBackground() call as a separate task, as doing it synchronously
            // here can cause redrawing glitches. See crbug.com/686662 for an example problem.
            Handler handler = new Handler();
            handler.post(() -> removeWindowBackground());
        }

        mRemoveWindowBackgroundDone = true;
    }

    @Override
    public void finishNativeInitialization() {
        mNativeInitialized = true;
        OfflineContentAggregatorNotificationBridgeUiFactory.instance();
        maybeRemoveWindowBackground();
        DownloadManagerService.getDownloadManagerService().onActivityLaunched();

        PowerMonitor.create();

        super.finishNativeInitialization();

        mManualFillingComponentSupplier.get().initialize(getWindowAndroid(),
                (ChromeKeyboardVisibilityDelegate) getWindowAndroid().getKeyboardDelegate(),
                findViewById(R.id.keyboard_accessory_stub),
                findViewById(R.id.keyboard_accessory_sheet_stub));

        mTabReparentingControllerSupplier.set(new TabReparentingController(
                ReparentingDelegateFactory.createReparentingControllerDelegate(
                        getTabModelSelector()),
                AsyncTabParamsManagerSingleton.getInstance()));

        // This must be initialized after initialization of tab reparenting controller.
        DisplayAndroid display = getWindowAndroid().getDisplay();
        mDisplayAndroidObserver = new DisplayAndroidObserver() {
            @Override
            public void onDisplayModesChanged(List<Mode> supportedModes) {
                maybeOnScreenSizeChange();
            }

            @Override
            public void onCurrentModeChanged(Mode currentMode) {
                maybeOnScreenSizeChange();
            }

            private void maybeOnScreenSizeChange() {
                if (didChangeTabletMode()) {
                    onScreenLayoutSizeChange();
                }
            }
        };
        display.addObserver(mDisplayAndroidObserver);

        // Make sure the user is reporting into one of the feed spinner groups, so that we can
        // analyze daily power impact for a typical Chrome user. The flag only has an effect if the
        // spinner is shown, but our earlier UMA analysis shows that it may have a side-effect on
        // a future browsing session's power, even if the spinner is not shown (by causing more
        // cold-starts).
        // TODO(crbug.com/1151391): Remove after analysis is complete.
        ChromeFeatureList.isEnabled(ChromeFeatureList.INTEREST_FEED_SPINNER_ALWAYS_ANIMATE);
    }

    /**
     * @return OverviewModeBehavior if this activity supports an overview mode and the
     *         OverviewModeBehavior has been initialized, null otherwise.
     */
    @VisibleForTesting
    public @Nullable OverviewModeBehavior getOverviewModeBehavior() {
        return null;
    }

    /**
     * @return Whether native initialization has been completed for this activity.
     */
    public boolean didFinishNativeInitialization() {
        return mNativeInitialized;
    }

    @Override
    public boolean onOptionsItemSelected(MenuItem item) {
        return super.onOptionsItemSelected(item);
    }

    /**
     * @return Whether the activity is in overview mode.
     */
    public boolean isInOverviewMode() {
        return false;
    }

    /**
     * Returns whether grid Tab switcher or the Start surface should be shown at startup.
     */
    public boolean shouldShowOverviewPageOnStart() {
        return false;
    }

    protected IntentHandlerDelegate createIntentHandlerDelegate() {
        return new IntentHandlerDelegate() {
            @Override
            public void processWebSearchIntent(String query) {
                final Intent searchIntent = new Intent(Intent.ACTION_WEB_SEARCH);
                searchIntent.putExtra(SearchManager.QUERY, query);
                Callback<Boolean> callback = result -> {
                    if (result != null && result) startActivity(searchIntent);
                };
            }

            @Override
            public long getIntentHandlingTimeMs() {
                return 0;
            }

            @Override
            public void processTranslateTabIntent(
                    @Nullable String targetLanguageCode, @Nullable String expectedUrl) {}

            @Override
            public void processUrlViewIntent(LoadUrlParams loadUrlParams,
                    @TabOpenType int tabOpenType, String externalAppId, int tabIdToBringToFront,
                    Intent intent) {}
        };
    }

    /**
     * @return Whether the tab models have been fully initialized.
     */
    public boolean areTabModelsInitialized() {
        return mTabModelOrchestrator.areTabModelsInitialized();
    }

    /**
     * {@link TabModelSelector} no longer implements TabModel.  Use getTabModelSelector() or
     * getCurrentTabModel() depending on your needs.
     * @return The {@link TabModelSelector}, possibly null.
     * @deprecated in favor of getTabModelSelectorSupplier.
     */
    @Deprecated
    public TabModelSelector getTabModelSelector() {
        if (!mTabModelOrchestrator.areTabModelsInitialized()) {
            throw new IllegalStateException(
                    "Attempting to access TabModelSelector before initialization");
        }
        return mTabModelOrchestrator.getTabModelSelector();
    }

    /**
     * Returns an {@link ObservableSupplier} for {@link TabModelOrchestrator}.
     */
    public final ObservableSupplier<TabModelOrchestrator> getTabModelOrchestratorSupplier() {
        return mTabModelOrchestratorSupplier;
    }

    /**
     * Returns an {@link ObservableSupplier} for {@link TabModelSelector}. Prefer this method over
     * using {@link #getTabModelSelector()} directly.
     */
    public final ObservableSupplier<TabModelSelector> getTabModelSelectorSupplier() {
        return mTabModelSelectorSupplier;
    }

    /**
     * @return The provider of the visible tab in the current activity.
     */
    public ActivityTabProvider getActivityTabProvider() {
        return mActivityTabProvider;
    }

    /**
     * @return The provider of the instance of {@link TabReparentingController}.
     */
    protected OneshotSupplier<TabReparentingController> getTabReparentingControllerSupplier() {
        return mTabReparentingControllerSupplier;
    }

    /**
     * Gets the supplier of the {@link TabCreatorManager} instance.
     */
    public ObservableSupplier<TabCreatorManager> getTabCreatorManagerSupplier() {
        return mTabCreatorManagerSupplier;
    }

    @Override
    public TabCreator getTabCreator(boolean incognito) {
        if (!mTabModelOrchestrator.areTabModelsInitialized()) {
            throw new IllegalStateException(
                    "Attempting to access TabCreator before initialization");
        }
        return mTabCreatorManagerSupplier.get().getTabCreator(incognito);
    }

    /**
     * Convenience method that returns a tab creator for the currently selected {@link TabModel}.
     * @return A tab creator for the currently selected {@link TabModel}.
     */
    public TabCreator getCurrentTabCreator() {
        return getTabCreator(getTabModelSelector().isIncognitoSelected());
    }

    /**
     * Gets the {@link TabContentManager} instance which holds snapshots of the tabs in this model.
     * @return The thumbnail cache, possibly null.
     * @Deprecated in favor of getTabContentManagerSupplier().
     */
    @Deprecated
    public TabContentManager getTabContentManager() {
        return mTabContentManager;
    }

    /**
     * Sets the {@link TabContentManager} owned by this {@link ChromeActivity}.
     * @param tabContentManager A {@link TabContentManager} instance.
     */
    private void setTabContentManager(TabContentManager tabContentManager) {
        mTabContentManager = tabContentManager;
        TabContentManagerHandler.create(
                tabContentManager, getFullscreenManager(), getTabModelSelector());
        mTabContentManagerSupplier.set(tabContentManager);
    }

    /**
     * Gets the supplier of the {@link TabContentManager} instance.
     */
    public ObservableSupplier<TabContentManager> getTabContentManagerSupplier() {
        return mTabContentManagerSupplier;
    }

    /**
     * Gets the current (inner) TabModel.  This is a convenience function for
     * getModelSelector().getCurrentModel().  It is *not* equivalent to the former getModel()
     * @return Never null, if modelSelector or its field is uninstantiated returns a
     *         {@link EmptyTabModel} singleton
     */
    public TabModel getCurrentTabModel() {
        TabModelSelector modelSelector = getTabModelSelector();
        if (modelSelector == null) return EmptyTabModel.getInstance();
        return modelSelector.getCurrentModel();
    }

    /**
     * DEPRECATED: Instead, use/hold a reference to {@link #mActivityTabProvider}. See
     *             https://crbug.com/871279 for more details. Note that there are important
     *             functional differences between {@link ActivityTabProvider} and this function
     *             when transitioning to/from the tab switcher. For a drop-in replacement, use
     *             {@link TabModelSelector#getCurrentTab} instead.
     *
     * Returns the tab being displayed by this ChromeActivity instance. This allows differentiation
     * between ChromeActivity subclasses that swap between multiple tabs (e.g. ChromeTabbedActivity)
     * and subclasses that only display one Tab (e.g. DocumentActivity).
     *
     * The default implementation grabs the tab currently selected by the TabModel, which may be
     * null if the Tab does not exist or the system is not initialized.
     */
    public Tab getActivityTab() {
        if (!mTabModelOrchestrator.areTabModelsInitialized()) {
            return null;
        }
        return TabModelUtils.getCurrentTab(getCurrentTabModel());
    }

    /**
     * @return The current WebContents, or null if the tab does not exist or is not showing a
     *         WebContents.
     */
    public WebContents getCurrentWebContents() {
        if (!mTabModelOrchestrator.areTabModelsInitialized()) {
            return null;
        }
        return TabModelUtils.getCurrentWebContents(getCurrentTabModel());
    }

    /**
     * Gets the browser controls manager, creates it unless already created.
     * @deprecated Instead, inject this directly to your constructor. If that's not possible, then
     *         use {@link BrowserControlsManagerSupplier}.
     */
    @NonNull
    @Deprecated
    public BrowserControlsManager getBrowserControlsManager() {
        if (!mBrowserControlsManagerSupplier.hasValue() && isActivityFinishingOrDestroyed()) {
            // BrowserControlsManagerSupplier should always have a value unless it's in the process
            // of destruction (and in that case, nothing should be called this method).
            throw new IllegalStateException();
        }
        assert mBrowserControlsManagerSupplier.hasValue();
        return mBrowserControlsManagerSupplier.get();
    }

    /**
     * @return Fullscreen manager object.
     */
    @NonNull
    public FullscreenManager getFullscreenManager() {
        return getBrowserControlsManager().getFullscreenManager();
    }

    /**
     * Sets the overlay mode.
     * Overlay mode means that we are currently using AndroidOverlays to display video, and
     * that the compositor's surface should support alpha and not be marked as opaque.
     */
    public void setOverlayMode(boolean useOverlayMode) {
        if (mCompositorViewHolderSupplier.hasValue()) {
            mCompositorViewHolderSupplier.get().setOverlayMode(useOverlayMode);
        }
    }

    /**
     * @return The content offset provider, may be null.
     */
    public ContentOffsetProvider getContentOffsetProvider() {
        return mCompositorViewHolderSupplier.get();
    }

    /**
     * @return The {@code ContextualSearchManager} or {@code null} if none;
     */
    public ObservableSupplier<ContextualSearchManager> getContextualSearchManagerSupplier() {
        return mContextualSearchManagerSupplier;
    }

    /**
     * Exits the fullscreen mode, if any. Does nothing if no fullscreen is present.
     * @return Whether the fullscreen mode is currently showing.
     */
    public boolean exitFullscreenIfShowing() {
        FullscreenManager fullscreenManager = getFullscreenManager();
        if (fullscreenManager.getPersistentFullscreenMode()) {
            fullscreenManager.exitPersistentFullscreenMode();
            return true;
        }
        return false;
    }

    @Override
    public void initializeCompositorContent(LayoutManagerImpl layoutManager,
            ViewGroup contentContainer) {
        // TODO(1168131): The responsibility of managing the availability of the LayoutManager
        //                should be moved to the RootUiCoordinator.
        LayoutManagerAppUtils.attach(getWindowAndroid(), layoutManager);
        mLayoutManagerSupplier.set(layoutManager);

        layoutManager.addSceneChangeObserver(this);
        CompositorViewHolder compositorViewHolder = mCompositorViewHolderSupplier.get();
        compositorViewHolder.setLayoutManager(layoutManager);
        compositorViewHolder.setFocusable(false);
        compositorViewHolder.setBrowserControlsManager(mBrowserControlsManagerSupplier.get());
        compositorViewHolder.setInsetObserverView(mInsetObserverViewSupplier.get());
        compositorViewHolder.setAutofillUiBottomInsetSupplier(
                mManualFillingComponentSupplier.get().getBottomInsetSupplier());
        compositorViewHolder.onFinishNativeInitialization(getTabModelSelector(), this);

        mActivityTabProvider.setLayoutStateProvider(layoutManager);

        if (mContextualSearchManagerSupplier.hasValue()) {
            mContextualSearchManagerSupplier.get().initialize(contentContainer, layoutManager,
                    compositorViewHolder,
                    0f,
                    getActivityType());
        }
    }

    /**
     * @return An {@link ObservableSupplier} that will supply the {@link LayoutManagerImpl} when it
     *         is ready.
     */
    public ObservableSupplier<LayoutManagerImpl> getLayoutManagerSupplier() {
        return mLayoutManagerSupplier;
    }

    /**
     * @return An {@link ObservableSupplier} that will supply the {@link CompositorViewHolder} when
     *         it is ready.
     */
    public ObservableSupplier<CompositorViewHolder> getCompositorViewHolderSupplier() {
        return mCompositorViewHolderSupplier;
    }

    public CompositorViewHolder getCompositorViewHolder() {
        return mCompositorViewHolderSupplier.get();
    }

    /**
     * Called when the back button is pressed.
     * @return Whether or not the back button was handled.
     */
    protected abstract boolean handleBackPressed();

    /**
     * @return If no higher priority back actions occur, whether pressing the back button
     *         would result in closing the tab. A true return value does not guarantee that
     *         a subsequent call to {@link #handleBackPressed()} will close the tab.
     */
    public boolean backShouldCloseTab(Tab tab) {
        return false;
    }

    @Override
    public void performOnConfigurationChanged(Configuration newConfig) {
        super.performOnConfigurationChanged(newConfig);
        if (mConfig != null) {
            if (mTabReparentingControllerSupplier.get() != null && didChangeTabletMode()) {
                onScreenLayoutSizeChange();
            }

            if (didChangeNonVrUiMode(mConfig.uiMode, newConfig.uiMode)
                    && !didChangeUiModeNight(mConfig.uiMode, newConfig.uiMode)) {
                recreate();
                return;
            }

            if (newConfig.densityDpi != mConfig.densityDpi) {
                recreate();
                return;
            }

            if (newConfig.orientation != mConfig.orientation) {
                RequestDesktopUtils.recordScreenOrientationChangedUkm(
                        newConfig.orientation == Configuration.ORIENTATION_LANDSCAPE,
                        getActivityTab());
            }
        }

        mConfig = newConfig;
    }

    private static boolean didChangeNonVrUiMode(int oldMode, int newMode) {
        if (oldMode == newMode) return false;
        return isInVrUiMode(oldMode) == isInVrUiMode(newMode);
    }

    private static boolean isInVrUiMode(int uiMode) {
        return (uiMode & Configuration.UI_MODE_TYPE_MASK) == Configuration.UI_MODE_TYPE_VR_HEADSET;
    }

    private static boolean didChangeUiModeNight(int oldMode, int newMode) {
        return (oldMode & Configuration.UI_MODE_NIGHT_MASK)
                != (newMode & Configuration.UI_MODE_NIGHT_MASK);
    }

    /**
     * Called by the system when the activity changes from fullscreen mode to multi-window mode
     * and visa-versa.
     * @param isInMultiWindowMode True if the activity is in multi-window mode.
     */
    @Override
    public void onMultiWindowModeChanged(boolean isInMultiWindowMode) {
        // If native is not initialized, the multi-window user action will be recorded in
        // #onDeferredStartupForMultiWindowMode() and CachedFeatureFlags#setIsInMultiWindowMode()
        // will be called in #onResumeWithNative(). Both of these methods require native to be
        // initialized, so do not call here to avoid crashing. See https://crbug.com/797921.
        if (mNativeInitialized) {
            recordMultiWindowModeChanged(isInMultiWindowMode, /* isDeferredStartup= */ false);
        }

        super.onMultiWindowModeChanged(isInMultiWindowMode);
    }

    /**
     * Records user actions and ukms associated with entering and exiting Android N multi-window
     * mode.
     * @param isInMultiWindowMode True if the activity is in multi-window mode.
     * @param isDeferredStartup True if the activity is deferred startup.
     */
    private void recordMultiWindowModeChanged(
            boolean isInMultiWindowMode, boolean isDeferredStartup) {
        MultiWindowUtils.getInstance().recordMultiWindowModeChanged(
                isInMultiWindowMode, isDeferredStartup, isFirstActivity(), getActivityTab());
    }

    /**
     * This method serves to distinguish windows in multi-window mode.
     * @return True if this activity is the first created activity.
     */
    protected boolean isFirstActivity() {
        return true;
    }

    /** Handles back press events for Chrome in various states. */
    protected final void handleOnBackPressed() {
        if (mNativeInitialized) RecordUserAction.record("SystemBack");

        TextBubble.dismissBubbles();

        if (mCompositorViewHolderSupplier.hasValue()) {
            LayoutManagerImpl layoutManager =
                    mCompositorViewHolderSupplier.get().getLayoutManager();
            if (layoutManager != null && layoutManager.onBackPressed()) return;
        }

        SelectionPopupController controller = getSelectionPopupController();
        if (controller != null && controller.isSelectActionBarShowing()) {
            controller.clearSelection();
            return;
        }

        handleBackPressed();
    }

    private void initializeBackPressHandling() {
        OnBackPressedCallback callback = new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                ChromeActivity.this.handleOnBackPressed();
            }
        };
        // Order matters here: first-come, last-served.
        // TODO(crbug.com/1279941): remove this line when all contents of handleOnBackPressed are
        // migrated to BackGestureManager.
        getOnBackPressedDispatcher().addCallback(this, callback);
    }

    @Override
    public void onTrimMemory(int level) {
        super.onTrimMemory(level);
    }

    private SelectionPopupController getSelectionPopupController() {
        WebContents webContents = getCurrentWebContents();
        return webContents != null ? SelectionPopupController.fromWebContents(webContents) : null;
    }

    @Override
    public void createContextualSearchTab(String searchUrl) {
        Tab currentTab = getActivityTab();
        if (currentTab == null) return;

        TabCreator tabCreator = getTabCreator(currentTab.isIncognito());
        if (tabCreator == null) return;

        tabCreator.createNewTab(new LoadUrlParams(searchUrl, PageTransition.LINK),
                TabLaunchType.FROM_LINK, getActivityTab());
    }

    /** Opens the chrome://management page on a new tab. */
    private void openChromeManagementPage() {
        Tab currentTab = getActivityTab();
        TabCreator tabCreator = getTabCreator(currentTab != null && currentTab.isIncognito());
        if (tabCreator == null) return;

        tabCreator.createNewTab(
                new LoadUrlParams(UrlConstants.MANAGEMENT_URL, PageTransition.AUTO_TOPLEVEL),
                TabLaunchType.FROM_CHROME_UI, getActivityTab());
    }

    /**
     * @return The {@link MenuOrKeyboardActionController} for registering menu or keyboard action
     *     handler for this activity.
     */
    public MenuOrKeyboardActionController getMenuOrKeyboardActionController() {
        return this;
    }

    @Override
    public void registerMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler handler) {
        mMenuActionHandlers.add(handler);
    }

    @Override
    public void unregisterMenuOrKeyboardActionHandler(MenuOrKeyboardActionHandler handler) {
        mMenuActionHandlers.remove(handler);
    }

    /**
     * Handles menu item selection and keyboard shortcuts.
     *
     * @param id The ID of the selected menu item (defined in main_menu.xml) or keyboard shortcut
     *     (defined in values.xml).
     * @param fromMenu Whether this was triggered from the menu.
     * @return Whether the action was handled.
     */
    @Override
    public boolean onMenuOrKeyboardAction(int id, boolean fromMenu) {
        return false;
    }

    /**
     * Mark that the UMA session has ended.
     */
    private void markSessionEnd() {

    }

    public final void postDeferredStartupIfNeeded() {
        if (!mNativeInitialized) {
            // Native hasn't loaded yet.  Queue it up for later.
            mDeferredStartupQueued = true;
            return;
        }
        mDeferredStartupQueued = false;

        if (!mDeferredStartupPosted) {
            mDeferredStartupPosted = true;
            onDeferredStartup();
        }
    }

    @Override
    public void terminateIncognitoSession() {}

    @Override
    public void onTabSelectionHinted(int tabId) {}

    @Override
    public void onSceneChange(Layout layout) {}

    @Override
    public void onAttachFragment(Fragment fragment) {
    }

    /**
     * Looks up the Chrome activity of the given web contents. This can be null. Should never be
     * cached, because web contents can change activities, e.g., when user selects "Open in Chrome"
     * menu item.
     *
     * @param webContents The web contents for which to lookup the Chrome activity.
     * @return Possibly null Chrome activity that should never be cached.
     * @deprecated Use {@link ActivityUtils#getActivityFromWebContents(WebContents)} instead.
     */
    @Nullable
    @Deprecated
    public static ChromeActivity fromWebContents(@Nullable WebContents webContents) {
        Activity activity = ActivityUtils.getActivityFromWebContents(webContents);
        if (!(activity instanceof ChromeActivity)) return null;

        return (ChromeActivity) activity;
    }

    private void setLowEndTheme() {
        if (ActivityUtils.getThemeId() == R.style.Theme_Chromium_WithWindowAnimation_LowEnd) {
            setTheme(R.style.Theme_Chromium_WithWindowAnimation_LowEnd);
        }
    }

    /**
     * Records histograms related to display dimensions.
     */
    private void recordDisplayDimensions() {
        DisplayAndroid display = DisplayAndroid.getNonMultiDisplay(this);
        int displayWidth = DisplayUtil.pxToDp(display, display.getDisplayWidth());
        int displayHeight = DisplayUtil.pxToDp(display, display.getDisplayHeight());
        int largestDisplaySize = displayWidth > displayHeight ? displayWidth : displayHeight;
        int smallestDisplaySize = displayWidth < displayHeight ? displayWidth : displayHeight;

        RecordHistogram.recordSparseHistogram("Android.DeviceSize.SmallestDisplaySize",
                MathUtils.clamp(smallestDisplaySize, 0, 1000));
        RecordHistogram.recordSparseHistogram("Android.DeviceSize.LargestDisplaySize",
                MathUtils.clamp(largestDisplaySize, 200, 1200));
    }

    @Override
    public boolean onActivityResultWithNative(int requestCode, int resultCode, Intent intent) {
        if (super.onActivityResultWithNative(requestCode, resultCode, intent)) return true;
        return false;
    }

    /**
     * @return  Whether this activity supports the find in page feature.
     */
    public boolean supportsFindInPage() {
        return true;
    }

    @VisibleForTesting
    public boolean didChangeTabletMode() {
        assert mConfig
                != null : "Can not determine the tablet mode when mConfig is not initialized";
        int smallestWidth = getCurrentSmallestScreenWidth(this);
        boolean isTablet = smallestWidth >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
        boolean wasTablet =
                mConfig.smallestScreenWidthDp >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
        return wasTablet != isTablet;
    }

    /**
     * Switch between phone and tablet mode and do the tab re-parenting in the meantime.
     */
    private void onScreenLayoutSizeChange() {
        if (mTabReparentingControllerSupplier.get() != null && !mIsTabReparentingPrepared) {
            mTabReparentingControllerSupplier.get().prepareTabsForReparenting();
            mIsTabReparentingPrepared = true;
            if (!isFinishing()) recreate();
        }
    }

    @VisibleForTesting
    public boolean deferredStartupPostedForTesting() {
        return mDeferredStartupPosted;
    }

    /** Returns whether the print action was successfully started. */
    private boolean doPrintShare(Activity activity, Supplier<Tab> currentTabSupplier) {
        PrintingController printingController = PrintingControllerImpl.getInstance();

        if (!currentTabSupplier.hasValue()) return false;
        if (printingController == null || printingController.isBusy()) return false;
        if (!UserPrefs.get(Profile.getLastUsedRegularProfile()).getBoolean(Pref.PRINTING_ENABLED)) {
            return false;
        }
        printingController.startPrint(
                new TabPrinter(currentTabSupplier.get()), new PrintManagerDelegateImpl(activity));
        return true;
    }

    /**
     * Returns a {@link CompositorViewHolder} instance for testing.
     */
    public CompositorViewHolder getCompositorViewHolderForTesting() {
        return mCompositorViewHolderSupplier.get();
    }
}
