package com.ark.browser.ui.fragment;

import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_CONTROLS;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_ICONS;
import static android.view.accessibility.AccessibilityManager.FLAG_CONTENT_TEXT;

import android.app.Activity;
import android.content.Context;
import android.graphics.Rect;
import android.os.Build;
import android.os.Bundle;
import android.text.format.DateUtils;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import com.ark.browser.core.ArkCompositorViewHolder;
import com.ark.browser.core.ArkNavigationHandler;
import com.ark.browser.core.ArkWindowAndroid;
import com.ark.browser.core.ArkWebManager;
import com.ark.browser.core.utils.NavigationPredictorBridge;
import com.ark.browser.event.LoadUrlEvent;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.TabCacheManager;
import com.ark.browser.tab.TabListManager;
import com.ark.browser.tab.core.ITab;
import com.ark.browser.tab.core.ITabGroup;
import com.ark.browser.ui.fragment.base.BaseFragment;
import com.ark.browser.ui.fragment.dialog.DownloadDialog;
import com.ark.browser.ui.widget.BottomController;
import com.ark.browser.utils.ArkLogger;
import com.ark.browser.utils.ThreadPool;
import com.zpj.bus.ZBus;
import com.zpj.fragmentation.dialog.ZDialog;
import com.zpj.utils.FileUtils;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivitySessionTracker;
import org.chromium.chrome.browser.app.flags.ChromeCachedFlags;
import org.chromium.chrome.browser.download.DownloadDialogBridge;
import org.chromium.chrome.browser.download.DownloadLocationDialogType;
import org.chromium.chrome.browser.download.DownloadManagerService;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabDelegateFactory;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.widget.InsetObserverView;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageAutodismissDurationProvider;
import org.chromium.components.messages.MessageContainer;
import org.chromium.components.messages.MessageDispatcherBridge;
import org.chromium.components.messages.MessageIdentifier;
import org.chromium.components.messages.MessageQueueDelegate;
import org.chromium.components.messages.MessagesFactory;
import org.chromium.components.messages.MessagesMetrics;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.PageTransition;
import org.chromium.url.GURL;

public class ArkMainFragment extends BaseFragment implements
        PauseResumeWithNativeObserver, StartStopWithNativeObserver, InsetObserverView.WindowInsetObserver {

    private static final String TAG = "ArkMainFragment";

    private ArkCompositorViewHolder mViewHolder;
//    private ProgressBar mProgressBar;
//    private EditText mUrlBar;

    private BottomController mBottomController;

    private boolean isViewCreated = false;
    private Runnable mOpenPage;

    @Nullable
    protected ManagedMessageDispatcher mMessageDispatcher;


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
                if (mMessageDispatcher != null) {
                    // TODO 移除
                    MessageDispatcherBridge.setWindowAndroid(null);
                    mMessageDispatcher.dismissAllMessages(DismissReason.ACTIVITY_DESTROYED);
                    MessagesFactory.detachMessageDispatcher(mMessageDispatcher);
                    mMessageDispatcher = null;
                }
                if (mViewHolder != null) {
                    mViewHolder.shutDown();
                    mViewHolder = null;
                }
                TabCacheManager.getInstance().destroy();
                ArkWebManager.destroy();
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

        DownloadDialogBridge.setDownloadDialogFactory(new DownloadDialogBridge.DownloadDialogFactory() {
            @Override
            public void showDialog(DownloadDialogBridge downloadDialogBridge,
                                   @NonNull Activity activity,
                                   String downloadUrl,
                                   long totalBytes,
                                   @ConnectionType int connectionType,
                                   @DownloadLocationDialogType int dialogType,
                                   String suggestedPath,
                                   boolean supportsLaterDialog,
                                   boolean isIncognito) {
                if (dialogType == DownloadLocationDialogType.DANGEROUS) {
                    ZDialog.alert()
                            .setTitle("Download Dangerous File?")
                            .setContent("Are you sure to download this dangerous file? ")
                            .setPositiveButton(R.string.ok, (dialog, which) -> {
                                downloadDialogBridge.onComplete(suggestedPath);
                                dialog.dismiss();
                            })
                            .setNegativeButton(R.string.cancel, (dialog, which) -> {
                                downloadDialogBridge.onCancel();
                                dialog.dismiss();
                            })
                            .setOnCancelListener(dialog -> downloadDialogBridge.onCancel())
                            .show(activity);
                } else {
                    new DownloadDialog()
                            .setFileName(FileUtils.getFileName(suggestedPath))
                            .setUrl(downloadUrl)
                            .setDownloadDialogBridge(downloadDialogBridge)
                            .setFileSize(FileUtils.formatFileSize(totalBytes))
                            .setDownloadPath(suggestedPath)
                            .show(activity);
                }
            }
        });

        ZBus.with(this)
                .observe(LoadUrlEvent.class)
                .doOnChange(this::onSearchEvent)
                .subscribe();

        TabListManager.getInstance().restore(new Callback<Void>() {
            @Override
            public void onResult(Void result) {
                Runnable runnable = () -> {
                    ITabGroup tabGroup = TabListManager.getInstance().getTabGroup(false);
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
//        ViewGroup rootView = (ViewGroup) _mActivity.getWindow().getDecorView().getRootView();
        // If the UI was inflated on a background thread, then the CompositorView may not have been
        // fully initialized yet as that may require the creation of a handler which is not allowed
        // outside the UI thread. This call should fully initialize the CompositorView if it hasn't
        // been yet.
        mViewHolder.setRootView(view);

        getWindowAndroid().setAnimationPlaceholderView(mViewHolder.getCompositorView());

        mBottomController = new BottomController(view);

//        ImageView btnBack = findViewById(R.id.btn_back);
//        ImageView btnForward = findViewById(R.id.btn_forward);
//
//        btnBack.setOnClickListener(v -> {
//            if (TabListManager.getInstance().getCurrentTabList().goBack()) {
//                return;
//            }
//            ZToast.warning("cant go back!");
//        });
//
//        btnForward.setOnClickListener(v -> {
//            if (TabListManager.getInstance().getCurrentTabList().goForward()) {
//                return;
//            }
//            ZToast.warning("cant go forward!");
//        });
//
//        mProgressBar = findViewById(R.id.progress_bar);
//        mProgressBar.setMax(100);
//
//        mUrlBar = findViewById(R.id.et_url);
//        mUrlBar.setOnEditorActionListener((v, actionId, event) -> {
//            if (actionId == EditorInfo.IME_ACTION_SEARCH) {
//                LoadUrlParams params = new LoadUrlParams(mUrlBar.getText().toString()
//                        , PageTransition.LINK);
//                TabListManager.getInstance().openNewTab(params, TabLaunchType.FROM_CHROME_UI);
//            }
//            return false;
//        });
//
//        ImageView btnRefresh = findViewById(R.id.btn_refresh);
//        btnRefresh.setOnClickListener(v -> {
//            Tab tab = getActivityTab();
//            if (tab != null) {
//                tab.reload();
//            }
//        });
//
//        ImageView btnMenu = findViewById(R.id.btn_menu);
//        btnMenu.setOnClickListener(v -> {
////            MainMenuDialog.show((ArkBrowserActivity)_mActivity)
//            start(new MainMenuDialog());
//        });
    }

    @Override
    public void onViewCreated(@NonNull View view, @Nullable Bundle savedInstanceState) {
        super.onViewCreated(view, savedInstanceState);


        ViewGroup rootView = (ViewGroup) _mActivity.getWindow().getDecorView().getRootView();

        // Setting fitsSystemWindows to false ensures that the root view doesn't consume the
        // insets.
        rootView.setFitsSystemWindows(false);

        // Add a custom view right after the root view that stores the insets to access later.
        // WebContents needs the insets to determine the portion of the screen obscured by
        // non-content displaying things such as the OSK.
        InsetObserverView insetObserverView = InsetObserverView.create(context);
        rootView.addView(insetObserverView, 0);

        mViewHolder.setInsetObserverView(insetObserverView);

        initMessageDispatcher();
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
        DownloadManagerService.getDownloadManagerService().initForBackgroundTask();
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
//                    ArkLogger.d(TAG, "onLoadProgressChanged tab=" + tab.getId() + " progress=" + progress);
//                    if (progress >= 1f) {
//                        mProgressBar.setVisibility(View.GONE);
//                    } else {
//                        mProgressBar.setVisibility(View.VISIBLE);
//                        mProgressBar.setProgress((int) (progress * 100));
//                    }
                }

                @Override
                public void onUrlUpdated(Tab tab) {
                    super.onUrlUpdated(tab);
//                    if (tab == null) {
//                        return;
//                    }
//                    mUrlBar.setText(tab.getUrl().getSpec());
                }

                @Override
                public void onLoadStarted(Tab tab, boolean toDifferentDocument) {
                    super.onLoadStarted(tab, toDifferentDocument);
//                    ArkLogger.d(TAG, "onLoadStarted tab=" + tab.getId());
//                    mProgressBar.setVisibility(View.VISIBLE);
//                    mProgressBar.setProgress(0);
                }

                @Override
                public void onLoadStopped(Tab tab, boolean toDifferentDocument) {
//                    mProgressBar.setVisibility(View.GONE);
                }

                @Override
                public void onPageLoadStarted(Tab tab, GURL url) {
                    super.onPageLoadStarted(tab, url);
//                    ArkLogger.d(TAG, "onPageLoadStarted tab=" + tab.getId());
//                    mProgressBar.setVisibility(View.VISIBLE);
//                    mProgressBar.setProgress(0);
                }

                @Override
                public void onPageLoadFinished(Tab tab, GURL url) {
//                    mProgressBar.setVisibility(View.GONE);
                }
            };


//            @Override
//            public boolean openNewPage(@NonNull Tab current, @TabLaunchType int type, String url) {
//                return TabListManager.getInstance().openNewPage(current, type, url);
//            }

            @Override
            public ITabGroup getTabList(Tab current) {
                if (current == null) {
                    return TabListManager.getInstance().getCurrentTabList();
                }
                return TabListManager.getInstance().getTabGroup(current.isIncognito());
            }

            @Override
            public void onPageAttached(@NonNull Tab page) {
                ArkLogger.e(TAG, "onPageAttached");
//                page.addObserver(observer);
//                if (!page.isLoading()) {
//                    mProgressBar.setVisibility(View.GONE);
//                }
//                mUrlBar.setText(page.getUrl().getSpec());
                mBottomController.onPageAttached(page);
            }

            @Override
            public void onPageDetached(@NonNull Tab page) {
//                page.removeObserver(observer);
//                mProgressBar.setVisibility(View.GONE);
                mBottomController.onPageDetached(page);
            }

            @Override
            public void onShutDown() {
                TabListManager.getInstance().onDestroy();
            }
        });
        mViewHolder.onStart();
    }

    private void initMessageDispatcher() {
        MessageContainer container = findViewById(R.id.message_container);

        // TODO 移除
        MessageDispatcherBridge.setWindowAndroid(getWindowAndroid());
        mMessageDispatcher = MessagesFactory.createMessageDispatcher(container,
                new Supplier<Integer>() {
                    @NonNull
                    @Override
                    public Integer get() {
                        return container.getMessageBannerHeight() + container.getMessageShadowTopMargin();
                    }
                },
                new ChromeMessageAutodismissDurationProvider(),
                getWindowAndroid()::startAnimationOverContent, getWindowAndroid());
        mMessageDispatcher.setDelegate(new MessageQueueDelegate() {

            private Runnable mCallback;

            @Override
            public void onStartShowing(Runnable callback) {
                container.setVisibility(View.VISIBLE);
                if (mCallback != null) {
                    ThreadPool.removeCallbacks(mCallback);
                }
                mCallback = callback;
                ThreadPool.postDelayed(mCallback, 2000);
            }

            @Override
            public void onFinishHiding() {
                if (mCallback != null) {
                    ThreadPool.removeCallbacks(mCallback);
                    mCallback = null;
                }
                container.setVisibility(View.GONE);
            }
        });
        MessagesFactory.attachMessageDispatcher(getWindowAndroid(), mMessageDispatcher);
    }

    public Tab getActivityTab() {
        ITab tab = TabListManager.getInstance().getCurrentTab();
        if (tab == null) {
            return null;
        }
        return TabCacheManager.getInstance().findTab(tab.getId());
    }


    public ArkCompositorViewHolder getViewHolder() {
        return mViewHolder;
    }

    @Override
    public void onInsetChanged(int left, int top, int right, int bottom) {
        if (mViewHolder != null) {
            mViewHolder.onInsetChanged(left, top, right, bottom);
        }
    }

    @Override
    public void onSafeAreaChanged(Rect area) {
        if (mViewHolder != null) {
            mViewHolder.onSafeAreaChanged(area);
        }
    }

    public void onSearchEvent(LoadUrlEvent event) {
        popTo(ArkMainFragment.class, false);
        for (int i = 0; i < getChildFragmentManager().getBackStackEntryCount(); i++) {
            popChild();
        }
        if (event.isNewTab() || getActivityTab() == null) {
            loadUrlInNewTab(event.getPageInfo(), event.getLoadUrlParams(), event.isIncognito());
            return;
        }

        LoadUrlParams loadUrlParams = event.getLoadUrlParams();
        loadUrlParams.setTransitionType(PageTransition.GENERATED);
        if (getActivityTab() != null) {
            loadUrl(loadUrlParams);
        } else {
            loadUrlInNewTab(event.getPageInfo(), loadUrlParams);
        }
    }

    public void loadUrl(String url) {
        loadUrl(new LoadUrlParams(url));
    }

    public void loadUrl(LoadUrlParams params) {
        loadUrl(params, TabListManager.getInstance().isIncognitoSelected());
    }

    public void loadUrl(String url, boolean incognito) {
        loadUrl(new LoadUrlParams(url), incognito);
    }

    public void loadUrl(LoadUrlParams params, boolean incognito) {
//        mLauncherManager.goToBrowser();
        TabListManager.getInstance().openNewTab(
                TabListManager.getInstance().getCurrentPageInfo(),
                params, TabLaunchType.FROM_CHROME_UI, incognito);
    }

    public void loadUrlInNewTab(String url, boolean incognito) {
        loadUrlInNewTab(null, url, incognito);
    }

    public void loadUrlInNewTab(PageInfo pageInfo, String url, boolean incognito) {
        loadUrlInNewTab(pageInfo, new LoadUrlParams(url), incognito);
    }

    public void loadUrlInNewTab(LoadUrlParams params) {
        loadUrlInNewTab(null, params);
    }

    public void loadUrlInNewTab(PageInfo pageInfo, LoadUrlParams params) {
        loadUrlInNewTab(pageInfo, params, TabListManager.getInstance().isIncognitoSelected());
    }

    public void loadUrlInNewTab(PageInfo pageInfo, LoadUrlParams params, boolean incognito) {
//        mLauncherManager.goToBrowser();
        TabListManager.getInstance().openNewTab(pageInfo, params, TabLaunchType.FROM_CHROME_UI, incognito);
    }


    /**
     * Implementation of {@link MessageAutodismissDurationProvider}.
     * <p>
     * Use finch parameter "autodismiss_duration_ms_{@link MessageIdentifier}" to customize through
     * finch config, such as "autodismiss_duration_ms_SyncError" within the feature {@code
     * ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE}. The duration configured in this way will
     * take the highest priority over clients' configuration in code.
     */
    public static class ChromeMessageAutodismissDurationProvider
            implements MessageAutodismissDurationProvider {
        @VisibleForTesting
        static final String FEATURE_SPECIFIC_FINCH_CONTROLLED_DURATION_PREFIX =
                "autodismiss_duration_ms_";

        private long mAutodismissDurationMs;
        private long mAutodismissDurationWithA11yMs;

        public ChromeMessageAutodismissDurationProvider() {
            mAutodismissDurationMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE, "autodismiss_duration_ms",
                    10 * (int) DateUtils.SECOND_IN_MILLIS);

            mAutodismissDurationWithA11yMs = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                    "autodismiss_duration_with_a11y_ms", 30 * (int) DateUtils.SECOND_IN_MILLIS);
        }

        @Override
        public long get(@MessageIdentifier int messageIdentifier, long customDuration) {
            long nonA11yDuration = Math.max(mAutodismissDurationMs, customDuration);
            long finchControlledDuration = ChromeFeatureList.getFieldTrialParamByFeatureAsInt(
                    ChromeFeatureList.MESSAGES_FOR_ANDROID_INFRASTRUCTURE,
                    FEATURE_SPECIFIC_FINCH_CONTROLLED_DURATION_PREFIX
                            + MessagesMetrics.messageIdentifierToHistogramSuffix(messageIdentifier),
                    -1);
            if (finchControlledDuration > 0) {
                nonA11yDuration = Math.max(finchControlledDuration, nonA11yDuration);
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                    && ChromeAccessibilityUtil.get().isAccessibilityEnabled()) {
                // crbug.com/1312548: To have a minimum duration even if the system has a default value.
                return Math.max(mAutodismissDurationWithA11yMs,
                        ChromeAccessibilityUtil.get().getRecommendedTimeoutMillis((int) nonA11yDuration,
                                FLAG_CONTENT_ICONS | FLAG_CONTENT_CONTROLS | FLAG_CONTENT_TEXT));
            }
            return ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                    ? Math.max(mAutodismissDurationWithA11yMs, nonA11yDuration)
                    : nonA11yDuration;
        }

        @VisibleForTesting
        public void setDefaultAutodismissDurationMsForTesting(long duration) {
            mAutodismissDurationMs = duration;
        }

        public void setDefaultAutodismissDurationWithA11yMsForTesting(long duration) {
            mAutodismissDurationWithA11yMs = duration;
        }
    }


}
