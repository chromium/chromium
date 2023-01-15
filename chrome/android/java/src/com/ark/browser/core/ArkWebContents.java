package com.ark.browser.core;

import android.text.TextUtils;
import android.util.SparseArray;
import android.view.View;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.core.utils.ContentUtils;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.ArkTabViewAndroidDelegate;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.core.IPage;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.WarmupManager;
import org.chromium.chrome.browser.WebContentsFactory;
import org.chromium.chrome.browser.incognito.IncognitoUtils;
import org.chromium.chrome.browser.offlinepages.OfflinePageUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabCreationState;
import org.chromium.chrome.browser.tab.TabJni;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabState;
import org.chromium.chrome.browser.tab.WebContentsStateBridge;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.content.browser.webcontents.ObserverProxyFactory;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsObserverProxy;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;

public class ArkWebContents {

    private final PageInfo mPageInfo;

    /** {@link WebContents} showing the current page, or {@code null} if the tab is frozen. */
    @NonNull
    private final WebContents mWebContents;

//    /** The parent view of the ContentView and the InfoBarContainer. */
//    private ContentView mContentView;

    /**
     * Importance of the WebContents currently attached to this tab. Note the key difference from
     * |mIsHidden| is that a tab is hidden when the application is hidden, but the importance is
     * not affected by this signal.
     */
    @ChildProcessImportance
    private int mImportance = ChildProcessImportance.NORMAL;

    /**
     * URL load to be performed lazily when the Tab is next shown.
     */
    private LoadUrlParams mPendingLoadParams;

    private boolean mStartLoad = false;
    private boolean mFinishLoad = false;

    public ArkWebContents(PageInfo pageInfo, @NonNull WebContents webContents) {
        mPageInfo = pageInfo;
        mWebContents = webContents;
        if (mWebContents instanceof WebContentsImpl) {
            ((WebContentsImpl) mWebContents).setObserverFactory(new ObserverProxyFactory() {
                @Override
                public WebContentsObserverProxy create(WebContentsImpl webContents) {
                    return new WebContentsObserverProxy(webContents) {

                        @Override
                        public void titleWasSet(String title) {
                            if (!TextUtils.isEmpty(title) && !TextUtils.equals(mPageInfo.getTitle(), title)) {
                                mPageInfo.setTitle(title);
                            }
                            super.titleWasSet(title);
                        }

                        @Override
                        public void didStartLoading(GURL url) {
                            mFinishLoad = false;
                            mStartLoad = true;
                            super.didStartLoading(url);
                        }

                        @Override
                        public void didFinishLoad(GlobalRenderFrameHostId rfhId, GURL url, boolean isKnownValid, boolean isInPrimaryMainFrame, int rfhLifecycleState) {
                            mStartLoad = true;
                            mFinishLoad = true;
                            super.didFinishLoad(rfhId, url, isKnownValid, isInPrimaryMainFrame, rfhLifecycleState);
                        }
                    };
                }
            });
        }
//        mWebContents.addObserver(new WebContentsObserver() {
//
//            @Override
//            public void titleWasSet(String title) {
//                if (!TextUtils.equals(mPageInfo.getTitle(), title)) {
//                    mPageInfo.setTitle(title);
//                }
//            }
//
//            @Override
//            public void didStartLoading(GURL url) {
//                mFinishLoad = false;
//                mStartLoad = true;
//            }
//
//            @Override
//            public void didFinishLoad(GlobalRenderFrameHostId rfhId, GURL url, boolean isKnownValid, boolean isInPrimaryMainFrame, int rfhLifecycleState) {
//                mStartLoad = true;
//                mFinishLoad = true;
//            }
//        });
    }

    public PageInfo getPageInfo() {
        return mPageInfo;
    }

    public int getId() {
        return mPageInfo.getId();
    }

    public boolean isIncognito() {
        return mPageInfo.isIncognito();
    }

    public boolean isStartLoad() {
        return mStartLoad;
    }

    public boolean isFinishLoad() {
        return mFinishLoad;
    }

    public void setTopLevelNativeWindow(WindowAndroid windowAndroid) {
        mWebContents.setTopLevelNativeWindow(windowAndroid);
    }

    @NonNull
    public WebContents getWebContents() {
        return mWebContents;
    }

    public Profile getProfile() {
        return Profile.fromWebContents(mWebContents);
    }

    public void reload() {

        if (OfflinePageUtils.isOfflinePage(mWebContents)) {
            // If current page is an offline page, reload it with custom behavior defined in extra
            // header respected.
            OfflinePageUtils.reload(getWebContents(),
                    new OfflinePageUtils.OfflinePageLoadUrlDelegate() {
                        @Override
                        public void loadUrl(LoadUrlParams params) {
                            ArkWebContents.this.loadUrl(params);
                        }
                    });
            return;
        }
        getWebContents().getNavigationController().reload(true);
    }

    public void reloadIgnoringCache() {
        //            switchUserAgentIfNeeded();
        mWebContents.getNavigationController().reloadBypassingCache(true);
    }

    public void loadUrl(LoadUrlParams params) {
        mPageInfo.setUrl(params.getUrl());
        mWebContents.getNavigationController().loadUrl(params);
    }

    public void evaluateJavaScript(String script, @Nullable JavaScriptCallback callback) {
        mWebContents.evaluateJavaScript(script, callback);
    }

    public boolean canGoBack() {
        return mWebContents.getNavigationController().canGoBack();
    }

    public boolean canGoForward() {
        return mWebContents.getNavigationController().canGoForward();
    }

    /**
     * Goes to the first non-skippable navigation entry before the current.
     */
    public void goBack() {
        mWebContents.getNavigationController().goBack();
    }

    /**
     * Goes to the first non-skippable navigation entry following the current.
     */
    public void goForward() {
        mWebContents.getNavigationController().goForward();
    }

    public GURL getUrl() {
        GURL url = mWebContents.getVisibleUrl();
        if (!url.isEmpty()) {
            mPageInfo.setUrl(url.getSpec());
        }
        return url;
    }

    public String getTitle() {
        String title = mPageInfo.getTitle();
        if (TextUtils.isEmpty(title)) {
            title = mWebContents.getTitle();
        }

        if (TextUtils.isEmpty(title)) {
            title = getUrl().getSpec();
        }
        return title;
    }

    public void setPendingLoadParams(LoadUrlParams loadUrlParams) {
        mPendingLoadParams = loadUrlParams;
    }

    public LoadUrlParams getPendingLoadParams() {
        return mPendingLoadParams;
    }

    public final void setImportance(@ChildProcessImportance int importance) {
        if (mImportance == importance) return;
        mImportance = importance;
        mWebContents.setImportance(mImportance);
    }

    public boolean needsReload() {
        return mWebContents.getNavigationController().needsReload();
    }

    public void loadIfNecessary() {
        mWebContents.getNavigationController().loadIfNecessary();
    }

    public void attach(ArkTabImpl tab) {
        ContentView cv = tab.getContentView();
        ViewAndroidDelegate delegate;
        if (cv == null) {
            delegate = ViewAndroidDelegate.createBasicDelegate(/* containerView */ null);
            mWebContents.setOverscrollRefreshHandler(null);
        } else {
            delegate = new ArkTabViewAndroidDelegate(tab, cv);
            mWebContents.setOverscrollRefreshHandler(tab.getWindowAndroid()
                    .getCompositorViewHolder().getSwipeRefreshHandler());
        }
        mWebContents.initialize(VersionInfo.getProductVersion(),
                delegate, cv, tab.getWindowAndroid(), WebContents.createDefaultInternalsHolder());

        mWebContents.setImportance(mImportance);
    }

    public void detach(ArkTabImpl tab) {
        setImportance(ChildProcessImportance.NORMAL);
        WebContentsAccessibility.fromWebContents(mWebContents).setObscuredByAnotherView(false);
        mWebContents.setOverscrollRefreshHandler(null);
    }

//    public void destroy() {
//        // TODO
////        ArkWebManager.getInstance().remove()
//    }

    public boolean isDestroyed() {
        return mWebContents.isDestroyed();
    }

    @Tab.TabLoadStatus
    public int loadUrlInternal(LoadUrlParams params) {
        params.setOverrideUserAgent(org.chromium.content_public.browser.navigation_controller.UserAgentOverrideOption.TRUE);
        // TODO(https://crbug.com/783819): Don't fix up all URLs. Documentation on
        // FixupURL explicitly says not to use it on URLs coming from untrustworthy
        // sources, like other apps. Once migrations of Java code to GURL are complete
        // and incoming URLs are converted to GURLs at their source, we can make
        // decisions of whether or not to fix up GURLs on a case-by-case basis based
        // on trustworthiness of the incoming URL.
        GURL fixedUrl = UrlFormatter.fixupUrl(params.getUrl());
        if (!fixedUrl.isValid()) return Tab.TabLoadStatus.PAGE_LOAD_FAILED;

        if (TabJni.get().handleNonNavigationAboutURL(fixedUrl)) {
            return Tab.TabLoadStatus.DEFAULT_PAGE_LOAD;
        }

        params.setUrl(fixedUrl.getSpec());
        ContentUtils.setUserAgentOverride(mWebContents, UserAgentManager.getUserAgentByUrl(fixedUrl));

        loadUrl(params);
        return Tab.TabLoadStatus.DEFAULT_PAGE_LOAD;
    }

//    public void addOnAttachStateChangeListener(View.OnAttachStateChangeListener listener) {
//        if (mContentView != null) {
//            mContentView.addOnAttachStateChangeListener(listener);
//        }
//    }

    /**
     * Notify that web preferences needs update for various properties.
     */
    public void notifyRendererPreferenceUpdate() {
        mWebContents.notifyRendererPreferenceUpdate();
    }

    @Nullable
    public ViewAndroidDelegate getViewAndroidDelegate() {
        return mWebContents.getViewAndroidDelegate();
    }

    public void reset() {
        mWebContents.clearJavaWebContentsObservers();
        mWebContents.initialize(VersionInfo.getProductVersion(),
                ViewAndroidDelegate.createBasicDelegate(/* containerView */ null),
                /* accessDelegate */ null, /* windowAndroid */ null,
                WebContents.createDefaultInternalsHolder());
    }

    public static class Builder {

        private Integer mLaunchType;
        private Integer mCreationType;
        private boolean mFromFrozenState;
        private LoadUrlParams mLoadUrlParams;

        private boolean mInitiallyHidden;
        private TabState mTabState;

        private final PageInfo mPageInfo;

        public Builder(PageInfo pageInfo) {
            mPageInfo = pageInfo;
        }

        public Builder(IPage page) {
            mPageInfo = page.getPageInfo();
        }

        /**
         * Sets a flag indicating how this tab is launched (from a link, external app, etc).
         * @param type Launch type.
         * @return {@link Builder} creating the Tab.
         */
        public Builder setLaunchType(@TabLaunchType int type) {
            mLaunchType = type;
            return this;
        }

        /**
         * Sets a flag indicating whether the Tab should start as hidden. Only used if
         * {@code webContents} is {@code null}.
         * @param initiallyHidden {@code true} if the newly created {@link WebContents} will be hidden.
         * @return {@link Builder} creating the Tab.
         */
        public Builder setInitiallyHidden(boolean initiallyHidden) {
            mInitiallyHidden = initiallyHidden;
            return this;
        }

        /**
         * Sets a {@link TabState} object containing information about this Tab, if it was persisted.
         * @param tabState State object.
         * @return {@link Builder} creating the Tab.
         */
        public Builder setTabState(TabState tabState) {
            mTabState = tabState;
            return this;
        }

        public ArkWebContents build() {
            // Pre-condition check
            if (mCreationType != null) {
                if (!mFromFrozenState) {
                    assert mCreationType != TabCreationState.FROZEN_ON_RESTORE;
                } else {
                    assert mLaunchType == TabLaunchType.FROM_RESTORE
                            && mCreationType == TabCreationState.FROZEN_ON_RESTORE;
                }
            } else {
                if (mFromFrozenState) assert mLaunchType == TabLaunchType.FROM_RESTORE;
            }

            if (mLaunchType == null) {
                mLaunchType = TabLaunchType.FROM_CHROME_UI;
            }

            if (mTabState != null) {
                String url = mTabState.contentsState.getVirtualUrlFromState();
                mPageInfo.setUrl(url);
                mPageInfo.setTitle(mTabState.contentsState.getDisplayTitleFromState());
            } else if (mLoadUrlParams != null) {
                mPageInfo.setUrl(mLoadUrlParams.getUrl());
            }

            ArkWebContents arkWeb = ArkWebManager.get(mPageInfo.pageId);
            if (arkWeb == null || arkWeb.isDestroyed()) {
                WebContents webContents = null;
                if (mTabState != null) {
                    webContents = WebContentsStateBridge.restoreContentsFromByteBuffer(
                            mTabState.contentsState, mInitiallyHidden);
                }

                if (webContents == null) {
                    webContents = WarmupManager.getInstance().takeSpareWebContents(
                            mPageInfo.isIncognito, mInitiallyHidden, false);
                }
                if (webContents == null) {
                    Profile profile = IncognitoUtils.getProfileFromWindowAndroid(
                            null, mPageInfo.isIncognito);
                    webContents = WebContentsFactory.createWebContents(profile, mInitiallyHidden);
                }

                arkWeb = new ArkWebContents(mPageInfo, webContents);
                ArkWebManager.put(mPageInfo.pageId, arkWeb);
            }
            arkWeb.setPendingLoadParams(mLoadUrlParams);
            return arkWeb;
        }

        private Builder setCreationType(@TabCreationState int type) {
            mCreationType = type;
            return this;
        }

        private Builder setFromFrozenState(boolean frozenState) {
            mFromFrozenState = frozenState;
            return this;
        }

        public Builder setLoadUrlParams(LoadUrlParams loadUrlParams) {
            mLoadUrlParams = loadUrlParams;
            return this;
        }

        /**
         * Creates a TabBuilder for a new, "frozen" tab from a saved state. This can be used for
         * background tabs restored on cold start that should be loaded when switched to. initialize()
         * needs to be called afterwards to complete the second level initialization.
         */
        public static Builder createFromFrozenState(PageInfo pageInfo) {
            return new Builder(pageInfo)
                    .setLaunchType(TabLaunchType.FROM_RESTORE)
                    .setCreationType(TabCreationState.FROZEN_ON_RESTORE)
                    .setFromFrozenState(true);
        }

//    /**
//     * Creates a TabBuilder for a new tab to be loaded lazily. This can be used for tabs opened
//     * in the background that should be loaded when switched to. initialize() needs to be called
//     * afterwards to complete the second level initialization.
//     * @param loadUrlParams Params specifying the conditions for loading url.
//     */
//    public static ArkTabBuilder createForLazyLoad(LoadUrlParams loadUrlParams) {
//        return new ArkTabBuilder()
//                .setLoadUrlParams(loadUrlParams)
//                .setCreationType(TabCreationState.FROZEN_FOR_LAZY_LOAD);
//    }

        /**
         * Creates a TabBuilder for a fresh tab. initialize() needs to be called afterwards to
         * complete the second level initialization.
         * @param initiallyHidden true iff the tab being created is initially in background
         */
        public static Builder createLiveTab(PageInfo pageInfo, boolean initiallyHidden) {
            return new Builder(pageInfo)
                    .setCreationType(initiallyHidden
                            ? TabCreationState.LIVE_IN_BACKGROUND
                            : TabCreationState.LIVE_IN_FOREGROUND);
        }
    }


}
