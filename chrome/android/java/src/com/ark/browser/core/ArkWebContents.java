package com.ark.browser.core;

import android.graphics.Color;
import android.text.TextUtils;

import androidx.annotation.Keep;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import com.ark.browser.core.utils.ContentUtils;
import com.ark.browser.settings.AppConfig;
import com.ark.browser.tab.ArkTabImpl;
import com.ark.browser.tab.PageInfo;
import com.ark.browser.tab.PageSnapshotManager;
import com.ark.browser.tab.core.IPage;
import com.ark.browser.utils.ArkLogger;
import com.zpj.bus.EventLiveData;
import com.zpj.utils.PrefsHelper;

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
import org.chromium.content.browser.JavascriptInterface;
import org.chromium.content.browser.webcontents.ObserverProxyFactory;
import org.chromium.content.browser.webcontents.WebContentsImpl;
import org.chromium.content.browser.webcontents.WebContentsObserverProxy;
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.JavaScriptCallback;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.json.JSONArray;
import org.json.JSONException;

import java.util.ArrayList;
import java.util.List;

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

    /**
     * True while a page load is in progress.
     */
    private boolean mIsLoading;

    private boolean mStartLoad = false;
    private boolean mFinishLoad = false;

    EventLiveData<List<String>> imagesLiveData;

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
                            mIsLoading = true;
                            super.didStartLoading(url);
                            updateThemeColor();
                        }

                        @Override
                        public void didFailLoad(boolean isInPrimaryMainFrame, int errorCode, GURL failingUrl, int frameLifecycleState) {
                            mIsLoading = false;
                            super.didFailLoad(isInPrimaryMainFrame, errorCode, failingUrl, frameLifecycleState);
                            updateThemeColor();
                        }

                        @Override
                        public void didFinishLoad(GlobalRenderFrameHostId rfhId, GURL url, boolean isKnownValid, boolean isInPrimaryMainFrame, int rfhLifecycleState) {
                            mStartLoad = true;
                            mFinishLoad = true;
                            mIsLoading = false;
                            super.didFinishLoad(rfhId, url, isKnownValid, isInPrimaryMainFrame, rfhLifecycleState);
                            updateThemeColor();
                        }

                        @Override
                        public void didStopLoading(GURL url, boolean isKnownValid) {
                            mIsLoading = false;
                            super.didStopLoading(url, isKnownValid);
                        }

                        @Override
                        public void didFirstVisuallyNonEmptyPaint() {
                            super.didFirstVisuallyNonEmptyPaint();
                            updateThemeColor();
                        }

                        @Override
                        public void renderProcessGone() {
                            mIsLoading = false;
                            super.renderProcessGone();
                        }
                    };
                }
            });
        }
    }

    @Keep
    @JavascriptInterface
    public void hello() {
        ArkLogger.e(this, "hello");
    }

    @Keep
    @JavascriptInterface
    public void log(String s) {
        ArkLogger.e(this, "log s=" + s);
    }

    @Keep
    @JavascriptInterface
    public void setImgUrls(String urls) {
        ArkLogger.e(this, "setImgUrls urls=" + urls);
        if (imagesLiveData == null) {
            return;
        }
        try {
            JSONArray jsonArray = new JSONArray(urls);
            List<String> images = new ArrayList<>();
            for (int i = 0; i < jsonArray.length(); i++) {
                images.add(jsonArray.getString(i));
            }
            imagesLiveData.postValue(images);
        } catch (JSONException e) {
            e.printStackTrace();
            imagesLiveData = null;
        }
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
                            ArkWebContents.this.loadUrlInternal(params);
                        }
                    });
            return;
        }
        ContentUtils.setUserAgentOverride(mWebContents, UserAgentManager.getUserAgentByUrl(getUrl()));
        getWebContents().getNavigationController().reload(true);
    }

    public void reloadIgnoringCache() {
        //            switchUserAgentIfNeeded();
        ContentUtils.setUserAgentOverride(mWebContents, UserAgentManager.getUserAgentByUrl(getUrl()));
        mWebContents.getNavigationController().reloadBypassingCache(true);
    }

    private void loadUrl(LoadUrlParams params) {
        mIsLoading = true;
        mPageInfo.setUrl(params.getUrl());
        mWebContents.getNavigationController().loadUrl(params);
    }

    public boolean isLoading() {
        return mIsLoading;
    }

    public void evaluateJavaScript(String script, @Nullable JavaScriptCallback callback) {
        mWebContents.evaluateJavaScript(script, callback);
    }

    public EventLiveData<List<String>> getImagesLiveData() {
        if (imagesLiveData == null) {
            imagesLiveData = new EventLiveData<>(true);
        }
        return imagesLiveData;
    }

    public void loadImages() {

        if (imagesLiveData == null) {
            imagesLiveData = new EventLiveData<>(true);
        }

        String script = "javascript:" +
                "console.log('get imgs');" +
                "const imgs = [];" +
                "var els = document.getElementsByTagName('img');\n" +
                "if(els == null){\n" +
                "   ark_bridge.setImgUrls(JSON.stringify(imgs));\n" +
                "} else {\n" +
                "   for(var i = 0;i < els.length; i++) {\n" +
                "       if(els[i].width > 70 && els[i].height > 70){\n" +
                "           const src = els[i].src;\n" +
                "           if(src == null || src.length < 5){\n" +
                "               continue;\n" +
                "           }\n" +
                "           console.log('image, ', src);\n" +
                "           imgs.push(src);\n" +
                "       }\n" +
                "   }\n" +
                "   ark_bridge.setImgUrls(JSON.stringify(imgs));\n" +
                "}";
        evaluateJavaScript(script, null);
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
        ArkLogger.e(this, "loadIfNecessary id=" + mPageInfo.getId());
        mWebContents.getNavigationController().loadIfNecessary();
    }

    public void attach(ArkTabImpl tab) {
        ArkLogger.e(this, "attach pageInfo=" + mPageInfo);
        setImportance(ChildProcessImportance.MODERATE);
        ContentView cv = tab.getContentView();
        if (cv == null) {
            mWebContents.setOverscrollRefreshHandler(null);
        } else {
            mWebContents.setOverscrollRefreshHandler(tab.getWindowAndroid()
                    .getCompositorViewHolder().getSwipeRefreshHandler());
        }
        mWebContents.initialize(VersionInfo.getProductVersion(), tab.getViewAndroidDelegate(),
                cv, tab.getWindowAndroid(), WebContents.createDefaultInternalsHolder());

        updateThemeColor();

        // TODO
//        JavascriptInjector mInjector = JavascriptInjector.fromWebContents(mWebContents, true);
//        mInjector.addPossiblyUnsafeInterface(this, "ark_bridge", ArkTabImpl.JavascriptInterface.class);

        loadIfNecessary();
    }

    public void detach(ArkTabImpl tab) {
        ArkLogger.e(this, "detach pageInfo=" + mPageInfo);
        setImportance(ChildProcessImportance.NORMAL);
        WebContentsAccessibility accessibility = WebContentsAccessibility.get(mWebContents);
        if (accessibility != null) {
            accessibility.setObscuredByAnotherView(false);
        }
        mWebContents.setViewAndroidDelegate(ViewAndroidDelegate.createBasicDelegate(/* containerView */ null));
        mWebContents.setOverscrollRefreshHandler(null);
        mWebContents.setFocus(false);

//        JavascriptInjector mInjector = JavascriptInjector.fromWebContents(mWebContents, true);
//        mInjector.removeInterface("ark_bridge");
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


        GURL url = UrlFormatter.fixupUrl(params.getUrl());
        String replaceHost = PrefsHelper.with("site_redirect_manager")
                .getString(url.getHost(), null);
        ArkLogger.e(this, "loadUrlInternal host=" + url.getHost()
                + " replaceHost=" + replaceHost);
        if (replaceHost != null) {
            params.setUrl(url.getSpec().replace(url.getHost(), replaceHost));
        }
        ArkLogger.e(this, "loadUrlInternal params url=" + params.getUrl());

        GURL fixedUrl = UrlFormatter.fixupUrl(params.getUrl());
        if (!fixedUrl.isValid()) return Tab.TabLoadStatus.PAGE_LOAD_FAILED;

        if (TabJni.get().handleNonNavigationAboutURL(fixedUrl)) {
            mIsLoading = true;
            return Tab.TabLoadStatus.DEFAULT_PAGE_LOAD;
        }

        params.setUrl(fixedUrl.getSpec());
        ContentUtils.setUserAgentOverride(mWebContents, UserAgentManager.getUserAgentByUrl(fixedUrl));

        loadUrl(params);
        return Tab.TabLoadStatus.DEFAULT_PAGE_LOAD;
    }

    public int getDefaultThemeColor() {
        return AppConfig.isNightMode() ? Color.BLACK : Color.WHITE;
    }

    public void updateThemeColor() {
        PageSnapshotManager.getInstance().loadSnapshot(getId(), bitmap -> {
            int themeColor = bitmap == null
                    ? (mPageInfo.getThemeColor() == 0
                    ? getDefaultThemeColor() : mPageInfo.getThemeColor())
                    : bitmap.getPixel(1, 1);
            mPageInfo.setThemeColor(themeColor);
            mWebContents.notifyChangeThemeColor();
        });
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
