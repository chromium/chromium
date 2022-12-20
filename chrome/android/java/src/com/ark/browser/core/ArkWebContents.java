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
import org.chromium.content_public.browser.ChildProcessImportance;
import org.chromium.content_public.browser.GlobalRenderFrameHostId;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsAccessibility;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.url.GURL;

public class ArkWebContents {

    private final PageInfo mPageInfo;

    /** {@link WebContents} showing the current page, or {@code null} if the tab is frozen. */
    @NonNull
    private final WebContents mWebContents;

    /** The parent view of the ContentView and the InfoBarContainer. */
    private ContentView mContentView;

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
        mWebContents.addObserver(new WebContentsObserver() {

            @Override
            public void titleWasSet(String title) {
                if (!TextUtils.equals(mPageInfo.getTitle(), title)) {
                    mPageInfo.setTitle(title);
                }
            }

            @Override
            public void didStartLoading(GURL url) {
                mFinishLoad = false;
                mStartLoad = true;
            }

            @Override
            public void didFinishLoad(GlobalRenderFrameHostId rfhId, GURL url, boolean isKnownValid, boolean isInPrimaryMainFrame, int rfhLifecycleState) {
                mStartLoad = true;
                mFinishLoad = true;
            }
        });
    }

    public boolean isStartLoad() {
        return mStartLoad;
    }

    public boolean isFinishLoad() {
        return mFinishLoad;
    }

    @NonNull
    public WebContents getWebContents() {
        return mWebContents;
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
        return mWebContents.getVisibleUrl();
    }

    public ContentView getContentView() {
        return mContentView;
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

//    public boolean loadIfNeeded(ArkTabImpl tab) {
//        if (mPendingLoadParams != null) {
//            tab.initWebContents(this, tab.getWindowAndroid());
//            tab.loadUrl(mPendingLoadParams);
//            mPendingLoadParams = null;
//            return true;
//        }
//
//        restoreIfNeeded(tab);
//        return true;
//    }

    public boolean needsReload() {
        return mWebContents.getNavigationController().needsReload();
    }

//    public void restoreIfNeeded(ArkTabImpl tab) {
//
//        try {
//            TraceEvent.begin("Tab.restoreIfNeeded");
//            // Restore is needed for a tab that is loaded for the first time. WebContents will
//            // be restored from a saved state.
//            if ((tab.isFrozen() && CriticalPersistedTabData.from(tab).getWebContentsState() != null
//                    && !unfreezeContents(tab.getWindowAndroid()))
//                    || !needsReload()) {
//                return;
//            }
//
//            loadIfNecessary();
//            mIsBeingRestored = true;
//            ObserverList.RewindableIterator<TabObserver> it = tab.getTabObservers();
//            while (it.hasNext()) {
//                TabObserver observer = it.next();
//                observer.onRestoreStarted(tab);
//            }
//        } finally {
//            TraceEvent.end("Tab.restoreIfNeeded");
//        }
//    }
//
//    /**
//     * Restores the WebContents from its saved state.  This should only be called if the tab is
//     * frozen with a saved TabState, and NOT if it was frozen for a lazy load.
//     * @return Whether or not the restoration was successful.
//     */
//    private boolean unfreezeContents(ArkTabImpl tab) {
//        boolean restored = true;
//        try {
//            TraceEvent.begin("Tab.unfreezeContents");
//            WebContentsState webContentsState =
//                    CriticalPersistedTabData.from(tab).getWebContentsState();
//            assert webContentsState != null;
//
//            WebContents webContents = WebContentsStateBridge.restoreContentsFromByteBuffer(
//                    webContentsState, tab.isHidden());
//            if (webContents == null) {
//                // State restore failed, just create a new empty web contents as that is the best
//                // that can be done at this point. TODO(jcivelli) http://b/5910521 - we should show
//                // an error page instead of a blank page in that case (and the last loaded URL).
//                Profile profile =
//                        IncognitoUtils.getProfileFromWindowAndroid(windowAndroid, isIncognito());
//                webContents = WebContentsFactory.createWebContents(profile, isHidden());
//                for (TabObserver observer : mObservers) observer.onRestoreFailed(this);
//                restored = false;
//            }
//
//            View compositorView = windowAndroid.getCompositorViewHolder();
//            if (compositorView != null) {
//                webContents.setSize(compositorView.getWidth(), compositorView.getHeight());
//            }
//
//
//            CriticalPersistedTabData.from(this).setWebContentsState(null);
//            initWebContents(new ArkWebContents(webContents), windowAndroid);
//
//            if (!restored) {
//                String url = CriticalPersistedTabData.from(this).getUrl().getSpec().isEmpty()
//                        ? UrlConstants.NTP_URL
//                        : CriticalPersistedTabData.from(this).getUrl().getSpec();
//                loadUrl(new LoadUrlParams(url, PageTransition.GENERATED));
//            }
//        } finally {
//            TraceEvent.end("Tab.unfreezeContents");
//        }
//        return restored;
//    }


    public void loadIfNecessary() {
        mWebContents.getNavigationController().loadIfNecessary();
    }

    public void attach(ArkTabImpl tab) {
        ContentView cv = ContentView.createContentView(
                ContextUtils.getApplicationContext(), null /* eventOffsetHandler */, mWebContents);
        cv.setContentDescription(ContextUtils.getApplicationContext().getResources().getString(
                org.chromium.chrome.R.string.accessibility_content_view));
        mContentView = cv;
        mWebContents.initialize(VersionInfo.getProductVersion(), new ArkTabViewAndroidDelegate(tab, cv), cv,
                tab.getWindowAndroid(), WebContents.createDefaultInternalsHolder());

        mWebContents.setImportance(mImportance);
        mContentView.addOnAttachStateChangeListener(tab.mAttachStateChangeListener);
    }

    public void detach(ArkTabImpl tab) {
        setImportance(ChildProcessImportance.NORMAL);
        WebContentsAccessibility.fromWebContents(mWebContents).setObscuredByAnotherView(false);
        if (mContentView != null) {
            mContentView.removeOnAttachStateChangeListener(tab.mAttachStateChangeListener);
            mContentView = null;
        }
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

        mPageInfo.setUrl(params.getUrl());
        mWebContents.getNavigationController().loadUrl(params);
        return Tab.TabLoadStatus.DEFAULT_PAGE_LOAD;
    }

    public void addOnAttachStateChangeListener(View.OnAttachStateChangeListener listener) {
        if (mContentView != null) {
            mContentView.addOnAttachStateChangeListener(listener);
        }
    }

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

    private static final SparseArray<ArkWebContents> TAB_CACHE = new SparseArray<>();

    public static ArkWebContents remove(int id) {
        ArkWebContents arkWeb = get(id);
        if (arkWeb != null) {
            TAB_CACHE.remove(id);
            if (arkWeb.isDestroyed()) {
                arkWeb = null;
            }
        }
        return arkWeb;
    }

    public static void destroy() {
        for (int i = 0; i < TAB_CACHE.size(); i++) {
            ArkWebContents web = TAB_CACHE.valueAt(i);
            if (web != null && !web.isDestroyed()) {
                web.getWebContents().destroy();
            }
        }
        TAB_CACHE.clear();
    }

    public static ArkWebContents get(int id) {
        return TAB_CACHE.get(id, null);
    }

    public static void put(int id, ArkWebContents arkWeb) {
        TAB_CACHE.put(id, arkWeb);
    }

    public static class Builder {

        private Tab mParent;
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
         * Sets the tab from which the new one is opened.
         * @param parent The parent Tab.
         * @return {@link Builder} creating the Tab.
         */
        public Builder setParent(Tab parent) {
            mParent = parent;
            return this;
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

            ArkWebContents arkWeb = ArkWebContents.remove(mPageInfo.pageId);
            if (arkWeb == null) {
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
                ArkWebContents.put(mPageInfo.pageId, arkWeb);
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
