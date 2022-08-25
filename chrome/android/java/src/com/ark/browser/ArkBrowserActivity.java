package com.ark.browser;

import android.graphics.drawable.ColorDrawable;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.tab.TabInfoObserver;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.TabManagerObserver;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.utils.ArkLogger;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.StrictModeContext;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.ChromeWindow;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.ChromeSessionState;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityWindowAndroid;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.WindowAndroid;

public class ArkBrowserActivity extends AsyncInitializationActivity {

    private static final String TAG = "BrowserActivity";

    private ArkCompositorViewHolder mViewHolder;
    private ViewGroup mContentContainer;

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onPauseWithNative() {
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
        if (TabListManager.getInstance().getCurrentTabList().canGoBack()) {
            TabListManager.getInstance().getCurrentTabList().goBack();
            return;
        }
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
        ViewGroup rootView = (ViewGroup) getWindow().getDecorView().getRootView();

        mViewHolder = findViewById(R.id.compositor_view_holder);
        mContentContainer = (ViewGroup) findViewById(android.R.id.content);

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
        return new ArkWindowAndroid(this, true, null) {

            @Override
            public TabDelegateFactory getTabDelegateFactory() {
                return getCompositorViewHolder().getTabDelegateFactory();
            }

            @Override
            public ArkCompositorViewHolder getCompositorViewHolder() {
                return mViewHolder;
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


                TextView tvBack = findViewById(R.id.tv_back);
                TextView tvForward = findViewById(R.id.tv_forward);

                tvBack.setOnClickListener(v -> {
                    if (TabListManager.getInstance().getCurrentTabList().goBack()) {
                        return;
                    }
                    Toast.makeText(this, "cant go back!", Toast.LENGTH_SHORT).show();
                });

                tvForward.setOnClickListener(v -> {
                    if (TabListManager.getInstance().getCurrentTabList().goForward()) {
                        return;
                    }
                    Toast.makeText(this, "cant go forward!", Toast.LENGTH_SHORT).show();
                });

                EditText etUrl = findViewById(R.id.et_url);

                TabListManager.getInstance().getCurrentTabList().addObserver(new TabInfoObserver() {
                    @Override
                    public void didSelectTab(IPage page, int type, int lastId) {
                        Tab tab = page.getNativePage();
                        if (tab == null) {
                            return;
                        }
                        etUrl.setText(tab.getUrl().getSpec());
                    }

                    @Override
                    public void didCloseTab(int tabId, boolean incognito) {

                    }

                    @Override
                    public void didAddTab(IPage pageInfo, int type) {

                    }
                });


                TextView tvGo = findViewById(R.id.tv_go);
                tvGo.setOnClickListener(v -> {
                    LoadUrlParams params = new LoadUrlParams(etUrl.getText().toString()
                            , PageTransition.LINK);
                    TabListManager.getInstance().openNewTab(params, TabLaunchType.FROM_CHROME_UI);
                });

                TextView tvRefresh = findViewById(R.id.tv_refresh);
                tvRefresh.setOnClickListener(v -> {
                    Tab tab = getActivityTab();
                    if (tab != null) {
                        tab.reload();
                    }
                });


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
            mViewHolder.setFocusable(false);
            mViewHolder.initCompositor(getWindowAndroid(), new ArkCompositorViewHolder.Callback() {
                @Override
                public boolean openNewPage(@NonNull Tab current, @TabLaunchType int type, String url) {
                    return TabListManager.getInstance().openNewPage(current, type, url);
                }

                @Override
                public ITabGroup getTabList(@NonNull Tab current) {
                    return TabListManager.getInstance().getTabList(current.isIncognito());
                }

                @Override
                public void onPageAttached(@NonNull Tab page) {

                }

                @Override
                public void onPageDetached(@NonNull Tab page) {

                }

                @Override
                public void onShutDown() {
                    TabListManager.getInstance().onDestroy();
                }
            });
        }
    }

}
