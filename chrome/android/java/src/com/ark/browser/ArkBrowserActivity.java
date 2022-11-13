package com.ark.browser;

import android.graphics.drawable.ColorDrawable;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ProgressBar;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.core.utils.NavigationPredictorBridge;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.ui.dialog.MainMenuDialog;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.Callback;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.flags.ChromeSessionState;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

public class ArkBrowserActivity extends AsyncInitializationActivity {

    private static final String TAG = "BrowserActivity";

    private ArkCompositorViewHolder mViewHolder;
    private ProgressBar mProgressBar;
    private EditText mUrlBar;

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onPauseWithNative() {
        NavigationPredictorBridge.onPause();
        super.onPauseWithNative();
    }

    @Override
    public void onStartWithNative() {
        super.onStartWithNative();
        ChromeActivitySessionTracker.getInstance().onStartWithNative();
        ChromeCachedFlags.getInstance().cacheNativeFlags();
    }

    @Override
    public void onResumeWithNative() {
        super.onResumeWithNative();
        Tab tab = getActivityTab();
        if (tab != null) {
            WebContents webContents = tab.getWebContents();

            // For picture-in-picture mode / auto-darken web contents.
            if (webContents != null) webContents.notifyRendererPreferenceUpdate();
        }

        ChromeSessionState.setIsInMultiWindowMode(
                MultiWindowUtils.getInstance().isInMultiWindowMode(this));

        ChromeSessionState.setDarkModeState(false, false);

        if (isWarmOnResume()) {
            NavigationPredictorBridge.onActivityWarmResumed();
        } else {
            NavigationPredictorBridge.onColdStart();
        }
    }

    @Override
    public void onStart() {
        super.onStart();

        if (mViewHolder != null) {
            mViewHolder.onStart();
        }
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
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mViewHolder != null) {
            mViewHolder.onStop();
        }
        onActivityHidden();
    }

    private void onActivityHidden() {
        Tab tab = getActivityTab();
        if (tab != null) {
            tab.hide(TabHidingType.ACTIVITY_HIDDEN);
        }
    }

    @Override
    protected void onDestroy() {
        if (mViewHolder != null) {
            mViewHolder.shutDown();
            mViewHolder = null;
        }
        super.onDestroy();
    }

    @Override
    public void onBackPressed() {
        super.onBackPressed();
    }

    @Override
    protected void performPreInflationStartup() {
        super.performPreInflationStartup();
        getWindow().setBackgroundDrawable(new ColorDrawable(getResources().getColor(R.color.light_background_color)));
    }

    @Override
    protected void performPostInflationStartup() {
        super.performPostInflationStartup();
        getWindowAndroid().setAnimationPlaceholderView(mViewHolder.getCompositorView());


    }

    @Override
    protected void triggerLayoutInflation() {
        TabListManager.getInstance().restore(getWindowAndroid(), new Callback<Void>() {
            @Override
            public void onResult(Void result) {

            }
        });
        try (TraceEvent te = TraceEvent.scoped("BrowserActivity.triggerLayoutInflation")) {
            SelectionPopupController.setShouldGetReadbackViewFromWindowAndroid();

            enableHardwareAcceleration();
            setLowEndTheme();

            WarmupManager warmupManager = WarmupManager.getInstance();
            warmupManager.clearViewHierarchy();
            doLayoutInflation();
        }
    }

    @Override
    protected void onInitialLayoutInflationComplete() {
        DownloadManagerService.getDownloadManagerService().initForBackgroundTask();

        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();

        mViewHolder = findViewById(R.id.compositor_view_holder);
        mProgressBar = findViewById(R.id.progress_bar);
        mProgressBar.setMax(100);

        // If the UI was inflated on a background thread, then the CompositorView may not have been
        // fully initialized yet as that may require the creation of a handler which is not allowed
        // outside the UI thread. This call should fully initialize the CompositorView if it hasn't
        // been yet.
        mViewHolder.setRootView(rootView);

        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);

        // Add a custom view right after the root view that stores the insets to access later.
        // WebContents needs the insets to determine the portion of the screen obscured by
        // non-content displaying things such as the OSK.
        InsetObserverView insetObserverView = InsetObserverView.create(this);
        rootView.addView(insetObserverView, 0);

        super.onInitialLayoutInflationComplete();


        try (TraceEvent e = TraceEvent.scoped("BrowserActivity.startNativeInitialization")) {
            // This is on the critical path so don't delay.
            setupCompositorContentPostNative();

            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::finishNativeInitialization);

        }

        mViewHolder.onStart();
    }

    @Override
    public void initializeCompositor() {
//        mTabContentManager.initWithNative();
//        mViewHolder.onNativeLibraryReady(getWindowAndroid(), mTabContentManager);
    }

    @Override
    public void startNativeInitialization() {

    }

    @Override
    protected ArkWindowAndroid createWindowAndroid() {
        return new ArkWindowAndroid(this) {

            @Override
            public TabDelegateFactory getTabDelegateFactory() {
                return getCompositorViewHolder().getTabDelegateFactory();
            }

            @Override
            public ArkCompositorViewHolder getCompositorViewHolder() {
                return mViewHolder;
            }

            @Override
            public ArkNavigationHandler getNavigationHandler() {
                return new ArkNavigationHandler() {
                    @Override
                    public boolean canGoForward() {
                        return TabListManager.getInstance().getCurrentTabList().canGoForward();
                    }

                    @Override
                    public boolean goForward() {
                        return TabListManager.getInstance().getCurrentTabList().goForward();
                    }

                    @Override
                    public boolean canGoBack() {
                        return TabListManager.getInstance().getCurrentTabList().canGoBack();
                    }

                    @Override
                    public boolean goBack() {
                        return TabListManager.getInstance().getCurrentTabList().goBack();
                    }
                };
            }

        };
    }

    @Nullable
    @Override
    public ArkWindowAndroid getWindowAndroid() {
        return (ArkWindowAndroid) super.getWindowAndroid();
    }

    /**
     * This function implements the actual layout inflation, Subclassing Activities that override
     * this method without calling super need to call {@link #onInitialLayoutInflationComplete()}.
     */
    protected void doLayoutInflation() {
        ArkLogger.e(TAG, "doLayoutInflation");
        try (TraceEvent te = TraceEvent.scoped("ChromeActivity.doLayoutInflation")) {
            // Allow disk access for the content view and toolbar container setup.
            // On certain android devices this setup sequence results in disk writes outside
            // of our control, so we have to disable StrictMode to work. See
            // https://crbug.com/639352.
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                TraceEvent.begin("setContentView(R.layout.main)");
                setContentView(R.layout.activity_browser);


                ImageView btnBack = findViewById(R.id.btn_back);
                ImageView btnForward = findViewById(R.id.btn_forward);

                btnBack.setOnClickListener(v -> {
                    if (TabListManager.getInstance().getCurrentTabList().goBack()) {
                        return;
                    }
                    Toast.makeText(this, "cant go back!", Toast.LENGTH_SHORT).show();
                });

                btnForward.setOnClickListener(v -> {
                    if (TabListManager.getInstance().getCurrentTabList().goForward()) {
                        return;
                    }
                    Toast.makeText(this, "cant go forward!", Toast.LENGTH_SHORT).show();
                });

                mUrlBar = findViewById(R.id.et_url);
                mUrlBar.setOnEditorActionListener((v, actionId, event) -> {
                    if (actionId == EditorInfo.IME_ACTION_SEARCH) {
                        LoadUrlParams params = new LoadUrlParams(mUrlBar.getText().toString()
                                , PageTransition.LINK);
                        TabListManager.getInstance().openNewTab(params, TabLaunchType.FROM_CHROME_UI);
                    }
                    return false;
                });

                ImageView btnRefresh = findViewById(R.id.btn_refresh);
                btnRefresh.setOnClickListener(v -> {
                    Tab tab = getActivityTab();
                    if (tab != null) {
                        tab.reload();
                    }
                });

                ImageView btnMenu = findViewById(R.id.btn_menu);
                btnMenu.setOnClickListener(v -> MainMenuDialog.show(ArkBrowserActivity.this));

                TraceEvent.end("setContentView(R.layout.main)");

            }
            onInitialLayoutInflationComplete();
        }
    }

    private void setLowEndTheme() {
        if (ActivityUtils.getThemeId() == R.style.Theme_Chromium_WithWindowAnimation_LowEnd) {
            setTheme(R.style.Theme_Chromium_WithWindowAnimation_LowEnd);
        }
    }

    public Tab getActivityTab() {
        IPage page = TabListManager.getInstance().getCurrentPage();
        if (page != null) {
            return page.getNativePage();
        }
        return null;
    }



    private void setupCompositorContentPostNative() {
        ArkLogger.e(TAG, "setupCompositorContentPostNative");
        try (TraceEvent e = TraceEvent.scoped(
                "BrowserActivity.setupCompositorContentPostNative")) {

            TabListManager.getInstance().addObserver(() -> {
                Tab tab = getActivityTab();
                ArkLogger.d(TAG, "TabManagerObserver onChange tab=" + tab);
                if (tab != null) {
                    mViewHolder.getLayoutManager().initLayoutTabFromHost(tab.getId());
                }
                mViewHolder.setTab(tab);
            });

            mViewHolder.setFocusable(false);
            mViewHolder.initCompositor(getWindowAndroid(), new ArkCompositorViewHolder.Callback() {

                private final TabObserver observer = new EmptyTabObserver() {

                    @Override
                    public void onLoadProgressChanged(Tab tab, float progress) {
                        super.onLoadProgressChanged(tab, progress);
                        ArkLogger.d(TAG, "onLoadProgressChanged tab=" + tab.getId() + " progress=" + progress);
                        if (progress >= 1f) {
                            mProgressBar.setVisibility(View.GONE);
                        } else {
                            mProgressBar.setVisibility(View.VISIBLE);
                            mProgressBar.setProgress((int) (progress * 100));
                        }
                    }

                    @Override
                    public void onUrlUpdated(Tab tab) {
                        super.onUrlUpdated(tab);
                        if (tab == null) {
                            return;
                        }
                        mUrlBar.setText(tab.getUrl().getSpec());
                    }

                    @Override
                    public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                        super.onLoadStarted(tab, toDifferentDocument);
                        ArkLogger.d(TAG, "onLoadStarted tab=" + tab.getId());
                        mProgressBar.setVisibility(View.VISIBLE);
                        mProgressBar.setProgress(0);
                    }

                    @Override
                    public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
                        mProgressBar.setVisibility(View.GONE);
                    }

                    @Override
                    public void onPageLoadStarted(Tab tab, GURL url) {
                        super.onPageLoadStarted(tab, url);
                        ArkLogger.d(TAG, "onPageLoadStarted tab=" + tab.getId());
                        mProgressBar.setVisibility(View.VISIBLE);
                        mProgressBar.setProgress(0);
                    }

                    @Override
                    public void onPageLoadFinished(Tab tab, GURL url) {
                        mProgressBar.setVisibility(View.GONE);
                    }
                };


                @Override
                public boolean openNewPage(@NonNull Tab current, @TabLaunchType int type, String url) {
                    return TabListManager.getInstance().openNewPage(current, type, url);
                }

                @Override
                public ITabGroup getTabList(Tab current) {
                    if (current == null) {
                        return TabListManager.getInstance().getCurrentTabList();
                    }
                    return TabListManager.getInstance().getTabList(current.isIncognito());
                }

                @Override
                public void onPageAttached(@NonNull Tab page) {
                    page.addObserver(observer);
                    if (!page.isLoading()) {
                        mProgressBar.setVisibility(View.GONE);
                    }
                    mUrlBar.setText(page.getUrl().getSpec());
                }

                @Override
                public void onPageDetached(@NonNull Tab page) {
                    page.removeObserver(observer);
                    mProgressBar.setVisibility(View.GONE);
                }

                @Override
                public void onShutDown() {
                    TabListManager.getInstance().onDestroy();
                }
            });
        }
    }

}
