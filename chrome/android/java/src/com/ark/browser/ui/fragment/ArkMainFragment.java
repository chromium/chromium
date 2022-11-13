package com.ark.browser.ui.fragment;

import android.content.Context;
import android.os.Bundle;
import android.view.View;
import android.view.ViewGroup;
import android.view.inputmethod.EditorInfo;
import android.widget.EditText;
import android.widget.ImageView;
import android.widget.ProgressBar;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.ArkBrowserActivity;
import com.ark.browser.ArkCompositorViewHolder;
import com.ark.browser.ArkNavigationHandler;
import com.ark.browser.ArkWindowAndroid;
import com.ark.browser.core.utils.NavigationPredictorBridge;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.ui.dialog.MainMenuDialog;
import com.ark.browser.ui.fragment.base.BaseFragment;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;
import com.zpj.toast.ZToast;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

public class ArkMainFragment extends BaseFragment implements
        PauseResumeWithNativeObserver, StartStopWithNativeObserver {

    private static final String TAG = "ArkMainFragment";

    private ArkCompositorViewHolder mViewHolder;
    private ProgressBar mProgressBar;
    private EditText mUrlBar;

    private boolean isViewCreated = false;
    private Runnable mOpenPage;

    public ArkWindowAndroid createWindowAndroid(Context context) {
        return new ArkWindowAndroid(context) {

            @Override
            public TabDelegateFactory getTabDelegateFactory() {
                return getCompositorViewHolder().getTabDelegateFactory();
            }

            @Override
            public ArkCompositorViewHolder getCompositorViewHolder() {
                return mViewHolder;
            }

            @Override
            public void destroy() {
                if (mViewHolder != null) {
                    mViewHolder.shutDown();
                    mViewHolder = null;
                }
                super.destroy();
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

    @Override
    protected int getLayoutId() {
        return R.layout.fragment_main;
    }

    @Override
    public void onCreate(@Nullable Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        TabListManager.getInstance().restore(getWindowAndroid(), new Callback<Void>() {
            @Override
            public void onResult(Void result) {
                Runnable runnable = () -> {
                    ITabGroup tabGroup = TabListManager.getInstance().getTabList(false);
                    int count = tabGroup.getCount();
                    ArkLogger.e(TAG, "restore count=" + count);

                    if (count > 0) {
                        ArkLogger.e(TAG, "restore selectTabAt " + tabGroup.getIndex());
                        tabGroup.selectTabAt(tabGroup.getIndex());
                    } else {
                        ArkLogger.e(TAG, "restore openNewTab");
                        LoadUrlParams params = new LoadUrlParams("www.baidu.com", PageTransition.LINK);
                        TabListManager.getInstance().openNewTab(params, TabLaunchType.FROM_CHROME_UI);
                    }
                };
                ThreadPool.postOnUIThread(() -> {
                    if (isViewCreated) {
                        runnable.run();
                    } else {
                        mOpenPage = runnable;
                    }
                });
            }
        });
    }

    @Override
    protected void initView(View view, @Nullable Bundle savedInstanceState) {

        mViewHolder = findViewById(R.id.compositor_view_holder);
        ViewGroup rootView = (ViewGroup) _mActivity.getWindow().getDecorView().getRootView();
        // If the UI was inflated on a background thread, then the CompositorView may not have been
        // fully initialized yet as that may require the creation of a handler which is not allowed
        // outside the UI thread. This call should fully initialize the CompositorView if it hasn't
        // been yet.
        mViewHolder.setRootView(rootView);

        getWindowAndroid().setAnimationPlaceholderView(mViewHolder.getCompositorView());

        ImageView btnBack = findViewById(R.id.btn_back);
        ImageView btnForward = findViewById(R.id.btn_forward);

        btnBack.setOnClickListener(v -> {
            if (TabListManager.getInstance().getCurrentTabList().goBack()) {
                return;
            }
            ZToast.warning("cant go back!");
        });

        btnForward.setOnClickListener(v -> {
            if (TabListManager.getInstance().getCurrentTabList().goForward()) {
                return;
            }
            ZToast.warning("cant go forward!");
        });

        mProgressBar = findViewById(R.id.progress_bar);
        mProgressBar.setMax(100);

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
        btnMenu.setOnClickListener(v -> MainMenuDialog.show((ArkBrowserActivity)_mActivity));
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);


        initCompositor();

        isViewCreated = true;
        if (mOpenPage != null) {
            mOpenPage.run();
            mOpenPage = null;
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
        Tab tab = getActivityTab();
        if (tab != null) {
            tab.hide(TabHidingType.ACTIVITY_HIDDEN);
        }
    }

    @Override
    public void onStartWithNative() {
        ChromeActivitySessionTracker.getInstance().onStartWithNative();
        ChromeCachedFlags.getInstance().cacheNativeFlags();
    }

    @Override
    public void onStopWithNative() {

    }

    @Override
    public void onResumeWithNative() {
        Tab tab = getActivityTab();
        if (tab != null) {
            WebContents webContents = tab.getWebContents();

            // For picture-in-picture mode / auto-darken web contents.
            if (webContents != null) webContents.notifyRendererPreferenceUpdate();
        }
    }

    @Override
    public void onPauseWithNative() {
        NavigationPredictorBridge.onPause();
    }

    private void initCompositor() {
        ArkLogger.e(TAG, "initCompositor");
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
        mViewHolder.onStart();
    }

    public Tab getActivityTab() {
        IPage page = TabListManager.getInstance().getCurrentPage();
        if (page != null) {
            return page.getNativePage();
        }
        return null;
    }


    public ArkCompositorViewHolder getViewHolder() {
        return mViewHolder;
    }
}
