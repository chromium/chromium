// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar;

import android.app.Activity;
import android.text.TextUtils;
import android.view.View;
import android.view.View.MeasureSpec;
import android.view.ViewGroup;
import android.view.ViewGroup.MarginLayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.supplier.Supplier;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.content.ContentUtils;
import org.chromium.chrome.browser.content.WebContentsFactory;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchManager;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.embedder_support.delegate.WebContentsDelegateAndroid;
import org.chromium.components.embedder_support.view.ContentView;
import org.chromium.components.external_intents.ExternalNavigationHandler;
import org.chromium.components.navigation_interception.InterceptNavigationDelegate;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.base.ViewAndroidDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.url.GURL;
import org.chromium.url.Origin;

/**
 * Content container for an OverlayPanel. This class is responsible for the management of the
 * WebContents displayed inside of a panel and exposes a simple API relevant to actions a
 * panel has.
 */
public class OverlayPanelContent {
    /** The {@link CompositorViewHolder} for the current activity, used to add/remove views. */
    private final ViewGroup mCompositorViewHolder;

    /** The {@link WindowAndroid} for the current activity. */
    private final WindowAndroid mWindowAndroid;

    /** Supplies the current activity {@link Tab}. */
    private final Supplier<Tab> mCurrentTabSupplier;

    /** Used for progress bar events. */
    private final WebContentsDelegateAndroid mWebContentsDelegate;

    /** The Profile this OverlayPanel is associated with. */
    private final Profile mProfile;

    /** The WebContents that this panel will display. */
    private WebContents mWebContents;

    /** The container view that this panel uses. */
    private ViewGroup mContainerView;

    /** The pointer to the native version of this class. */
    private long mNativeOverlayPanelContentPtr;

    /** The activity that this content is contained in. */
    private Activity mActivity;

    /** Observer used for tracking loading and navigation. */
    private WebContentsObserver mWebContentsObserver;

    /** The URL that was directly loaded using the {@link #loadUrl(String)} method. */
    private String mLoadedUrl;

    /** Whether the content has started loading a URL. */
    private boolean mDidStartLoadingUrl;

    /**
     * Whether we should reuse any existing WebContents instead of deleting and recreating.
     * See crbug.com/682953 for details.
     */
    private boolean mShouldReuseWebContents;

    /**
     * Whether the WebContents is processing a pending navigation.
     * NOTE(pedrosimonetti): This is being used to prevent redirections on the SERP to be
     * interpreted as a regular navigation, which should cause the Contextual Search Panel
     * to be promoted as a Tab. This was added to work around a server bug that has been fixed.
     * Just checking for whether the Content has been touched is enough to determine whether a
     * navigation should be promoted (assuming it was caused by the touch), as done in
     * {@link ContextualSearchManager#shouldPromoteSearchNavigation()}.
     * For more details, see crbug.com/441048
     * TODO(pedrosimonetti): remove this from M48 or move it to Contextual Search Panel.
     */
    private boolean mIsProcessingPendingNavigation;

    /** Whether the content view is currently being displayed. */
    private boolean mIsContentViewShowing;

    /** The observer used by this object to inform implementers of different events. */
    private OverlayPanelContentDelegate mContentDelegate;

    /** Used to observe progress bar events. */
    private OverlayPanelContentProgressObserver mProgressObserver;

    /** If a URL is set to delayed load (load on user interaction), it will be stored here. */
    private String mPendingUrl;

    // http://crbug.com/522266 : An instance of InterceptNavigationDelegateImpl should be kept in
    // java layer. Otherwise, the instance could be garbage-collected unexpectedly.
    private InterceptNavigationDelegate mInterceptNavigationDelegate;

    /** The desired size of the {@link ContentView} associated with this panel content. */
    private int mContentViewWidth;

    private int mContentViewHeight;
    private boolean mSubtractBarHeight;

    /** The height of the bar at the top of the OverlayPanel in pixels. */
    private final int mBarHeightPx;

    /** Sets the top offset of the overlay panel in pixel. 0 when fully expanded. */
    private int mPanelTopOffsetPx;

    private class OverlayViewDelegate extends ViewAndroidDelegate {
        public OverlayViewDelegate(ViewGroup v) {
            super(v);
        }

        @Override
        public void setViewPosition(
                View view,
                float x,
                float y,
                float width,
                float height,
                int leftMargin,
                int topMargin) {
            super.setViewPosition(view, x, y, width, height, leftMargin, topMargin);

            // Applies top offset depending on the overlay panel state.
            MarginLayoutParams lp = (MarginLayoutParams) view.getLayoutParams();
            lp.topMargin += mPanelTopOffsetPx + mBarHeightPx;
        }
    }

    // ============================================================================================
    // InterceptNavigationDelegateImpl
    // ============================================================================================

    // Used to intercept intent navigations.
    // TODO(jeremycho): Consider creating a Tab with the Panel's WebContents.
    // which would also handle functionality like long-press-to-paste.
    private class InterceptNavigationDelegateImpl extends InterceptNavigationDelegate {
        final ExternalNavigationHandler mExternalNavHandler;

        public InterceptNavigationDelegateImpl() {
            Tab tab = mCurrentTabSupplier.get();
            mExternalNavHandler =
                    (tab != null && tab.getWebContents() != null)
                            ? new ExternalNavigationHandler(new ExternalNavigationDelegateImpl(tab))
                            : null;
        }

        @Override
        public boolean shouldIgnoreNavigation(
                NavigationHandle navigationHandle,
                GURL escapedUrl,
                boolean hiddenCrossFrame,
                boolean isSandboxedFrame) {
            // If either of the required params for the delegate are null, do not call the
            // delegate and ignore the navigation.
            if (mExternalNavHandler == null || navigationHandle == null) return true;
            return !mContentDelegate.shouldInterceptNavigation(
                    mExternalNavHandler,
                    escapedUrl,
                    navigationHandle.pageTransition(),
                    navigationHandle.isRedirect(),
                    navigationHandle.hasUserGesture(),
                    navigationHandle.isRendererInitiated(),
                    navigationHandle.getReferrerUrl(),
                    navigationHandle.isInPrimaryMainFrame(),
                    navigationHandle.isExternalProtocol());
        }

        @Override
        public GURL handleSubframeExternalProtocol(
                GURL escapedUrl,
                @PageTransition int transition,
                boolean hasUserGesture,
                Origin initiatorOrigin) {
            mContentDelegate.shouldInterceptNavigation(
                    mExternalNavHandler,
                    escapedUrl,
                    transition,
                    /* isRedirect= */ false,
                    hasUserGesture,
                    /* isRendererInitiated= */ true,
                    GURL.emptyGURL()
                    /* referrerUrl= */ ,
                    /* isInPrimaryMainFrame= */ false,
                    /* isExternalProtocol= */ true);
            return null;
        }
    }

    // ============================================================================================
    // Constructor
    // ============================================================================================

    /**
     * @param contentDelegate An observer for events that occur on this content. If null is passed
     *     for this parameter, the default one will be used.
     * @param progressObserver An observer for progress related events.
     * @param activity The {@link Activity} that contains this object.
     * @param profile The Profile associated with the OverlayPanel.
     * @param barHeight The height of the bar at the top of the OverlayPanel in dp.
     * @param compositorViewHolder The {@link CompositorViewHolder} for the current activity.
     * @param windowAndroid The {@link WindowAndroid} for the current activity.
     * @param currentTabSupplier Supplies the current activity {@link Tab}.
     */
    public OverlayPanelContent(
            @NonNull OverlayPanelContentDelegate contentDelegate,
            @NonNull OverlayPanelContentProgressObserver progressObserver,
            @NonNull Activity activity,
            @NonNull Profile profile,
            float barHeight,
            @NonNull ViewGroup compositorViewHolder,
            @NonNull WindowAndroid windowAndroid,
            @NonNull Supplier<Tab> currentTabSupplier) {
        mNativeOverlayPanelContentPtr = OverlayPanelContentJni.get().init(OverlayPanelContent.this);
        mContentDelegate = contentDelegate;
        mProgressObserver = progressObserver;
        mActivity = activity;
        mProfile = profile;
        mBarHeightPx = (int) (barHeight * mActivity.getResources().getDisplayMetrics().density);
        mCompositorViewHolder = compositorViewHolder;
        mWindowAndroid = windowAndroid;
        mCurrentTabSupplier = currentTabSupplier;

        mWebContentsDelegate =
                new WebContentsDelegateAndroid() {
                    private boolean mIsFullscreen;

                    @Override
                    public void loadingStateChanged(boolean shouldShowLoadingUI) {
                        boolean isLoading = mWebContents != null && mWebContents.isLoading();
                        if (isLoading) {
                            mProgressObserver.onProgressBarStarted();
                        } else {
                            mProgressObserver.onProgressBarFinished();
                        }
                    }

                    @Override
                    public void visibleSSLStateChanged() {
                        mContentDelegate.onSSLStateUpdated();
                    }

                    @Override
                    public void enterFullscreenModeForTab(
                            boolean prefersNavigationBar, boolean prefersStatusBar) {
                        mIsFullscreen = true;
                    }

                    @Override
                    public void exitFullscreenModeForTab() {
                        mIsFullscreen = false;
                    }

                    @Override
                    public boolean isFullscreenForTabOrPending() {
                        return mIsFullscreen;
                    }

                    @Override
                    public boolean shouldCreateWebContents(GURL targetUrl) {
                        return false;
                    }

                    @Override
                    public int getTopControlsHeight() {
                        return (int) (mBarHeightPx / mWindowAndroid.getDisplay().getDipScale());
                    }

                    @Override
                    public int getBottomControlsHeight() {
                        return 0;
                    }
                };
    }

    // ============================================================================================
    // WebContents related
    // ============================================================================================

    /**
     * Load a URL; this will trigger creation of a new WebContents if being loaded immediately,
     * otherwise one is created when the panel's content becomes visible.
     * @param url The URL that should be loaded.
     * @param shouldLoadImmediately If a URL should be loaded immediately or wait until visibility
     *        changes.
     */
    public void loadUrl(String url, boolean shouldLoadImmediately) {
        mPendingUrl = null;

        if (!shouldLoadImmediately) {
            mPendingUrl = url;
        } else {
            createNewWebContents();
            mLoadedUrl = url;
            mDidStartLoadingUrl = true;
            mIsProcessingPendingNavigation = true;
            mWebContents.getNavigationController().loadUrl(new LoadUrlParams(url));
        }
    }

    /**
     * Whether we should reuse any existing WebContents instead of deleting and recreating.
     * @param reuse {@code true} if we want to reuse the WebContents.
     */
    public void setReuseWebContents(boolean reuse) {
        mShouldReuseWebContents = reuse;
    }

    /**
     * Call this when a loadUrl request has failed to notify the panel that the WebContents can
     * be reused.  See crbug.com/682953 for details.
     */
    void onLoadUrlFailed() {
        setReuseWebContents(true);
    }

    /**
     * Set the desired size of the underlying {@link ContentView}. This is determined
     * by the {@link OverlayPanel} before the creation of the content view.
     * @param width The width of the content view.
     * @param height The height of the content view.
     * @param subtractBarHeight if {@code true} view height should be smaller by {@code mBarHeight}.
     */
    void setContentViewSize(int width, int height, boolean subtractBarHeight) {
        mContentViewWidth = width;
        mContentViewHeight = height;
        mSubtractBarHeight = subtractBarHeight;
    }

    /** Makes the content visible, causing it to be rendered. */
    public void showContent() {
        setVisibility(true);
    }

    /**
     * Sets the top offset of the overlay panel that varies as the panel state changes.
     * @param offset Top offset in pixel.
     */
    public void setPanelTopOffset(int offset) {
        mPanelTopOffsetPx = offset;
    }

    /** Create a new WebContents that will be managed by this panel. */
    private void createNewWebContents() {
        if (mWebContents != null) {
            // If the WebContents has already been created, but never used,
            // then there's no need to create a new one.
            if (!mDidStartLoadingUrl || mShouldReuseWebContents) return;

            destroyWebContents();
        }

        // Creates an initially hidden WebContents which gets shown when the panel is opened.
        mWebContents = WebContentsFactory.createWebContents(mProfile, true, false);

        ContentView cv = ContentView.createContentView(mActivity, mWebContents);
        if (mContentViewWidth != 0 || mContentViewHeight != 0) {
            int width =
                    mContentViewWidth == 0
                            ? ContentView.DEFAULT_MEASURE_SPEC
                            : MeasureSpec.makeMeasureSpec(mContentViewWidth, MeasureSpec.EXACTLY);
            int height =
                    mContentViewHeight == 0
                            ? ContentView.DEFAULT_MEASURE_SPEC
                            : MeasureSpec.makeMeasureSpec(mContentViewHeight, MeasureSpec.EXACTLY);
            cv.setDesiredMeasureSpec(width, height);
        }

        OverlayViewDelegate delegate = new OverlayViewDelegate(cv);
        mWebContents.setDelegates(
                VersionInfo.getProductVersion(),
                delegate,
                cv,
                mWindowAndroid,
                WebContents.createDefaultInternalsHolder());
        ContentUtils.setUserAgentOverride(mWebContents, /* overrideInNewTabs= */ false);

        // Transfers the ownership of the WebContents to the native OverlayPanelContent.
        OverlayPanelContentJni.get()
                .setWebContents(mNativeOverlayPanelContentPtr, mWebContents, mWebContentsDelegate);

        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    @Override
                    public void didStartLoading(GURL url) {
                        mContentDelegate.onContentLoadStarted();
                    }

                    @Override
                    public void loadProgressChanged(float progress) {
                        mProgressObserver.onProgressBarUpdated(progress);
                    }

                    @Override
                    public void navigationEntryCommitted(LoadCommittedDetails details) {
                        mContentDelegate.onNavigationEntryCommitted();
                    }

                    @Override
                    public void didStartNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        if (!navigation.isSameDocument()) {
                            String url = navigation.getUrl().getSpec();
                            mContentDelegate.onMainFrameLoadStarted(
                                    url, !TextUtils.equals(url, mLoadedUrl));
                        }
                    }

                    @Override
                    public void titleWasSet(String title) {
                        mContentDelegate.onTitleUpdated(title);
                    }

                    @Override
                    public void didFinishNavigationInPrimaryMainFrame(NavigationHandle navigation) {
                        if (navigation.hasCommitted()) {
                            mIsProcessingPendingNavigation = false;
                            mContentDelegate.onMainFrameNavigation(
                                    navigation.getUrl().getSpec(),
                                    !TextUtils.equals(navigation.getUrl().getSpec(), mLoadedUrl),
                                    isHttpFailureCode(navigation.httpStatusCode()),
                                    navigation.isErrorPage());
                        }
                    }

                    @Override
                    public void didFirstVisuallyNonEmptyPaint() {
                        mContentDelegate.onFirstNonEmptyPaint();
                    }
                };

        mContainerView = cv;
        mInterceptNavigationDelegate = new InterceptNavigationDelegateImpl();
        OverlayPanelContentJni.get()
                .setInterceptNavigationDelegate(
                        mNativeOverlayPanelContentPtr, mInterceptNavigationDelegate, mWebContents);

        mContentDelegate.onContentViewCreated();
        resizePanelContentView();
        mCompositorViewHolder.addView(mContainerView, 1);
    }

    /** Destroy this panel's WebContents. */
    private void destroyWebContents() {
        if (mWebContents != null) {
            mCompositorViewHolder.removeView(mContainerView);

            // Native destroy will call up to destroy the Java WebContents.
            OverlayPanelContentJni.get().destroyWebContents(mNativeOverlayPanelContentPtr);
            mWebContents = null;
            if (mWebContentsObserver != null) {
                mWebContentsObserver.destroy();
                mWebContentsObserver = null;
            }

            mDidStartLoadingUrl = false;
            mIsProcessingPendingNavigation = false;
            mShouldReuseWebContents = false;
        }
    }

    // ============================================================================================
    // Utilities
    // ============================================================================================

    /**
     * Calls updateBrowserControlsState on the WebContents.
     *
     * @param areControlsHidden Whether the browser controls are hidden for the web contents. If
     *     false, the web contents viewport always accounts for the controls. Otherwise the web
     *     contents never accounts for them.
     */
    public void updateBrowserControlsState(boolean areControlsHidden) {
        OverlayPanelContentJni.get()
                .updateBrowserControlsState(mNativeOverlayPanelContentPtr, areControlsHidden);
    }

    /**
     * @return Whether a pending navigation if being processed.
     */
    public boolean isProcessingPendingNavigation() {
        return mIsProcessingPendingNavigation;
    }

    /** Reset the content's scroll position to (0, 0). */
    public void resetContentViewScroll() {
        if (mWebContents != null) {
            mWebContents.getEventForwarder().scrollTo(0, 0);
        }
    }

    /**
     * @return The Y scroll position.
     */
    public float getContentVerticalScroll() {
        return mWebContents != null
                ? RenderCoordinates.fromWebContents(mWebContents).getScrollYPixInt()
                : -1.f;
    }

    /**
     * Sets the visibility of the Search Content View.
     * @param isVisible True to make it visible.
     */
    private void setVisibility(boolean isVisible) {
        if (mIsContentViewShowing == isVisible) return;

        mIsContentViewShowing = isVisible;

        if (isVisible) {
            // If the last call to loadUrl was specified to be delayed, load it now.
            if (!TextUtils.isEmpty(mPendingUrl)) loadUrl(mPendingUrl, true);

            // The WebContents is created with the search request, but if none was made we'll need
            // one in order to display an empty panel.
            if (mWebContents == null) createNewWebContents();

            // NOTE(pedrosimonetti): Calling updateWebContentsVisibility() on the WebContents will
            // cause the page to be rendered. This has a side effect of causing the page to be
            // included in your Web History (if enabled). For this reason,
            // updateWebContentsVisibility() should only be called when we know for sure the page
            // will be seen by the user.
            if (mWebContents != null) mWebContents.updateWebContentsVisibility(Visibility.VISIBLE);

            mContentDelegate.onContentViewSeen();
        } else {
            if (mWebContents != null) mWebContents.updateWebContentsVisibility(Visibility.HIDDEN);
        }

        mContentDelegate.onVisibilityChanged(isVisible);
    }

    /**
     * @return Whether the given HTTP result code represents a failure or not.
     */
    private static boolean isHttpFailureCode(int httpResultCode) {
        return httpResultCode <= 0 || httpResultCode >= 400;
    }

    /**
     * @return true if the content is visible on the page.
     */
    public boolean isContentShowing() {
        return mIsContentViewShowing;
    }

    // ============================================================================================
    // Methods for managing this panel's WebContents.
    // ============================================================================================

    /** Reset this object's native pointer to 0; */
    @CalledByNative
    private void clearNativePanelContentPtr() {
        assert mNativeOverlayPanelContentPtr != 0;
        mNativeOverlayPanelContentPtr = 0;
    }

    /**
     * @return The associated {@link WebContents}.
     */
    public WebContents getWebContents() {
        return mWebContents;
    }

    /**
     * @return The associated {@link ContentView}.
     */
    public ViewGroup getContainerView() {
        return mContainerView;
    }

    void resizePanelContentView() {
        WebContents webContents = getWebContents();
        if (webContents == null) return;
        int viewHeight = mContentViewHeight - (mSubtractBarHeight ? mBarHeightPx : 0);
        OverlayPanelContentJni.get()
                .onPhysicalBackingSizeChanged(
                        mNativeOverlayPanelContentPtr, webContents, mContentViewWidth, viewHeight);
        mWebContents.setSize(mContentViewWidth, viewHeight);
    }

    /** Destroy the native component of this class. */
    @VisibleForTesting
    public void destroy() {
        if (mWebContents != null) destroyWebContents();

        // Tests will not create the native pointer, so we need to check if it's not zero
        // otherwise calling OverlayPanelContentJni.get().destroy with zero will make Chrome crash.
        if (mNativeOverlayPanelContentPtr != 0L) {
            OverlayPanelContentJni.get().destroy(mNativeOverlayPanelContentPtr);
        }
    }

    public InterceptNavigationDelegate getInterceptNavigationDelegateForTesting() {
        return mInterceptNavigationDelegate;
    }

    @NativeMethods
    interface Natives {
        // Native calls.
        long init(OverlayPanelContent caller);

        void destroy(long nativeOverlayPanelContent);

        void onPhysicalBackingSizeChanged(
                long nativeOverlayPanelContent,
                @JniType("content::WebContents*") WebContents webContents,
                int width,
                int height);

        void setWebContents(
                long nativeOverlayPanelContent,
                @JniType("content::WebContents*") WebContents webContents,
                WebContentsDelegateAndroid delegate);

        void destroyWebContents(long nativeOverlayPanelContent);

        void setInterceptNavigationDelegate(
                long nativeOverlayPanelContent,
                InterceptNavigationDelegate delegate,
                @JniType("content::WebContents*") WebContents webContents);

        void updateBrowserControlsState(long nativeOverlayPanelContent, boolean areControlsHidden);
    }
}
