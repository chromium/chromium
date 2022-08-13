package com.ark.browser;

import android.graphics.drawable.ColorDrawable;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.TextView;
import android.widget.Toast;

import androidx.annotation.NonNull;

import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.ITabGroup;

import org.chromium.base.StrictModeContext;
import org.chromium.base.SysUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.compositor.layouts.content.ContentOffsetProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.init.AsyncInitializationActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.SelectionPopupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.base.PageTransition;

public class BrowserActivity extends AsyncInitializationActivity {

    private ArkCompositorViewHolder mViewHolder;
    private ViewGroup mContentContainer;
    private TabContentManager mTabContentManager;

    @Override
    public boolean shouldStartGpuProcess() {
        return true;
    }

    @Override
    public void onStart() {
        super.onStart();
        if (mViewHolder != null) {
            mViewHolder.onStart();
        }
    }

    @Override
    public void onStop() {
        super.onStop();
        if (mViewHolder != null) {
            mViewHolder.onStop();
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
    protected void performPreInflationStartup() {
        super.performPreInflationStartup();
        getWindow().setBackgroundDrawable(new ColorDrawable(getResources().getColor(R.color.light_background_color)));
    }

    @Override
    protected void performPostInflationStartup() {
        super.performPostInflationStartup();
        getWindowAndroid().setAnimationPlaceholderView(mViewHolder.getCompositorView());

        mTabContentManager = new TabContentManager(
                this,
                new ContentOffsetProvider() {
                    @Override
                    public float getOverlayTranslateY() {
                        return 0;
                    }
                },
                !SysUtils.isLowEndDevice(),
                new TabContentManager.TabFinder() {
                    @Override
                    public Tab getTabById(int id) {
                        // TODO
                        return null;
                    }
                }
        );

        mViewHolder.onStart();
    }

    @Override
    protected void triggerLayoutInflation() {
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
    }

    @Override
    public void startNativeInitialization() {
        try (TraceEvent e = TraceEvent.scoped("BrowserActivity.startNativeInitialization")) {
            // This is on the critical path so don't delay.
            setupCompositorContentPostNative();

            PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::finishNativeInitialization);

        }
    }

    @Override
    public void initializeCompositor() {
        mTabContentManager.initWithNative();
        mViewHolder.onNativeLibraryReady(getWindowAndroid(), mTabContentManager);
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
                setContentView(R.layout.activity_browser);


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
                TextView tvGo = findViewById(R.id.tv_go);
                tvGo.setOnClickListener(v -> {
                    Tab tab = getActivityTab();
                    if (tab != null) {
                        LoadUrlParams params = new LoadUrlParams(etUrl.getText().toString()
                                , PageTransition.LINK);
                        tab.loadUrl(params);
                    }
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
        return null;
    }



    private void setupCompositorContentPostNative() {
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
