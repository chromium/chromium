// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.ephemeraltab;

import android.content.Context;
import android.graphics.RectF;
import android.text.TextUtils;
import android.view.MotionEvent;

import org.chromium.base.SysUtils;
import org.chromium.base.TimeUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator.AnimatorUpdateListener;
import org.chromium.chrome.browser.compositor.bottombar.OverlayContentDelegate;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanel;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelContent;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager;
import org.chromium.chrome.browser.compositor.bottombar.OverlayPanelManager.PanelPriority;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.OverlayPanelEventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.EphemeralTabSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.SceneOverlayLayer;
import org.chromium.chrome.browser.ntp.NewTabPage;
import org.chromium.chrome.browser.ssl.SecurityStateModel;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.tabmodel.TabSelectionType;
import org.chromium.components.url_formatter.UrlFormatter;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.PageTransition;
import org.chromium.ui.resources.ResourceManager;

/**
 * The panel containing an ephemeral tab.
 * TODO(jinsukkim): Write tests.
 *                  Add animation effect upon opening ephemeral tab.
 */
public class EphemeralTabPanel extends OverlayPanel {
    /** The compositor layer used for drawing the panel. */
    private EphemeralTabSceneLayer mSceneLayer;

    /** Remembers whether the panel was opened to the peeking state. */
    private boolean mDidRecordFirstPeek;

    /** The timestamp when the panel entered the peeking state for the first time. */
    private long mPanelPeekedNanoseconds;

    /** Remembers whether the panel was opened beyond the peeking state. */
    private boolean mDidRecordFirstOpen;

    /** The timestamp when the panel entered the opened state for the first time. */
    private long mPanelOpenedNanoseconds;

    /** True if the Tab from which the panel is opened is in incognito mode. */
    private boolean mIsIncognito;

    /** Url for which this epehemral tab was created. */
    private String mUrl;

    /** Observers detecting various signals indicating the panel needs closing. */
    private TabModelSelectorTabModelObserver mTabModelObserver;
    private TabModelSelectorTabObserver mTabModelSelectorTabObserver;

    /** Favicon opacity. Fully visible when 1.f. */
    private float mFaviconOpacity;

    /** Animation effect for favicon display. */
    private CompositorAnimator mFaviconAnimation;

    /** Animation effect for security icon display . */
    private CompositorAnimator mCaptionAnimation;

    private final AnimatorUpdateListener mFadeInAnimatorListener =
            animator -> mFaviconOpacity = animator.getAnimatedValue();
    private final AnimatorUpdateListener mFadeOutAnimatorListener =
            animator -> mFaviconOpacity = 1.f - animator.getAnimatedValue();
    private final AnimatorUpdateListener mSecurityIconAnimationListener = animator
            -> getBarControl().getCaptionControl().setIconOpacity(animator.getAnimatedValue());

    /**
     * Checks if this feature (a.k.a. "Preview page/image") is supported.
     * @return {@code true} if the feature is enabled.
     */
    public static boolean isSupported() {
        return ChromeFeatureList.isEnabled(ChromeFeatureList.EPHEMERAL_TAB)
                && !SysUtils.isLowEndDevice();
    }

    /**
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} used to request updates in the Layout.
     * @param panelManager The {@link OverlayPanelManager} used to control panel show/hide.
     */
    public EphemeralTabPanel(
            Context context, LayoutUpdateHost updateHost, OverlayPanelManager panelManager) {
        super(context, updateHost, panelManager);
        mSceneLayer =
                new EphemeralTabSceneLayer(mContext.getResources().getDisplayMetrics().density,
                        mContext.getResources().getDimensionPixelSize(
                                R.dimen.compositor_tab_title_favicon_size));
        mEventFilter = new OverlayPanelEventFilter(mContext, this) {
            @Override
            public boolean onInterceptTouchEventInternal(MotionEvent e, boolean isKeyboardShowing) {
                OverlayPanel panel = EphemeralTabPanel.this;
                if (panel.isShowing() && panel.isPeeking()
                        && panel.isCoordinateInsideBar(e.getX() * mPxToDp, e.getY() * mPxToDp)) {
                    // Events go to base panel in peeked mode to scroll base page.
                    return super.onInterceptTouchEventInternal(e, isKeyboardShowing);
                }
                if (panel.isShowing() && panel.isMaximized()) return true;
                return false;
            }
        };
    }

    @Override
    public void destroy() {
        stopListeningForCloseConditions();
    }

    private class EphemeralTabPanelContentDelegate extends OverlayContentDelegate {
        /** Whether the currently loaded page is an error (interstitial) page. */
        private boolean mIsOnErrorPage;

        @Override
        public void onMainFrameLoadStarted(String url, boolean isExternalUrl) {
            if (TextUtils.equals(mUrl, url)) return;

            if (mIsOnErrorPage && NewTabPage.isNTPUrl(url)) {
                // "Back to safety" on interstitial page leads to NTP.
                // We just close the panel in response.
                closePanel(StateChangeReason.NAVIGATION, true);
                mUrl = null;
                return;
            }
            mUrl = url;

            // Resets to default icon if favicon may need updating.
            startFaviconAnimation(false);
        }

        @Override
        public void onMainFrameNavigation(
                String url, boolean isExternalUrl, boolean isFailure, boolean isError) {
            updateCaption();
            mIsOnErrorPage = isError;
        }

        @Override
        public void onTitleUpdated(String title) {
            getBarControl().setBarText(title);
        }

        @Override
        public void onSSLStateUpdated() {
            if (isNewLayout()) updateCaption();
        }

        @Override
        public void onOpenNewTabRequested(String url) {
            // We never open a separate tab when navigating in an overlay.
            getWebContents().getNavigationController().loadUrl(new LoadUrlParams(url));
            requestPanelShow(StateChangeReason.CLICK);
        }
    }

    @Override
    public OverlayPanelContent createNewOverlayPanelContent() {
        if (mTabModelObserver == null) startListeningForCloseConditions();
        return new OverlayPanelContent(new EphemeralTabPanelContentDelegate(),
                new PanelProgressObserver(), mActivity, mIsIncognito, getBarHeight());
    }

    private void startListeningForCloseConditions() {
        TabModelSelector selector = mActivity.getTabModelSelector();
        mTabModelObserver = new TabModelSelectorTabModelObserver(selector) {
            @Override
            public void didSelectTab(Tab tab, @TabSelectionType int type, int lastId) {
                closeTab();
            }

            @Override
            public void didAddTab(Tab tab, @TabLaunchType int type) {
                closeTab();
            }
        };
        mTabModelSelectorTabObserver = new TabModelSelectorTabObserver(selector) {
            @Override
            public void onPageLoadStarted(Tab tab, String url) {
                // Hides the panel if the base page navigates.
                closeTab();
            }

            @Override
            public void onCrash(Tab tab) {
                // Hides the panel if the foreground tab crashed
                if (SadTab.isShowing(tab)) closeTab();
            }

            @Override
            public void onClosingStateChanged(Tab tab, boolean closing) {
                if (closing) closeTab();
            }
        };
    }

    private void stopListeningForCloseConditions() {
        if (mTabModelObserver == null) return;
        mTabModelObserver.destroy();
        mTabModelSelectorTabObserver.destroy();
        mTabModelObserver = null;
        mTabModelSelectorTabObserver = null;
    }

    private void closeTab() {
        closePanel(StateChangeReason.UNKNOWN, false);
    }

    @Override
    protected float getPeekedHeight() {
        return getBarHeight() * 1.5f;
    }

    @Override
    protected float getMaximizedHeight() {
        // Max height does not cover the entire content screen.
        return getTabHeight() * 0.9f;
    }

    @Override
    public boolean isPanelOpened() {
        return getHeight() > getPeekedHeight();
    }

    @Override
    public float getProgressBarOpacity() {
        return 1.0f;
    }

    @Override
    public void setPanelState(@PanelState int toState, @StateChangeReason int reason) {
        super.setPanelState(toState, reason);
        if (toState == PanelState.PEEKED) {
            recordMetricsForPeeked();
        } else if (toState == PanelState.CLOSED) {
            recordMetricsForClosed(reason);
        } else if (toState == PanelState.EXPANDED || toState == PanelState.MAXIMIZED) {
            recordMetricsForOpened();
        }
    }

    // Scene Overlay

    @Override
    public SceneOverlayLayer getUpdatedSceneOverlayTree(RectF viewport, RectF visibleViewport,
            LayerTitleCache layerTitleCache, ResourceManager resourceManager, float yOffset) {
        mSceneLayer.update(resourceManager, this, getBarControl(),
                getBarControl().getTitleControl(), getBarControl().getCaptionControl());
        return mSceneLayer;
    }

    @Override
    public boolean updateOverlay(long time, long dt) {
        // Allow WebContents to size itself appropriately (includes browser controls height).
        updateBrowserControlsState();
        return super.updateOverlay(time, dt);
    }

    /**
     * Starts animation effect on the favicon.
     * @param showFavicon If {@code true} show the favicon with fade-in effect. Otherwise
     *         fade out the favicon to show the default one.
     */
    public void startFaviconAnimation(boolean showFavicon) {
        if (mFaviconAnimation != null) mFaviconAnimation.cancel();
        mFaviconAnimation = new CompositorAnimator(getAnimationHandler());
        mFaviconAnimation.setDuration(BASE_ANIMATION_DURATION_MS);
        mFaviconAnimation.removeAllListeners();
        mFaviconAnimation.addUpdateListener(
                showFavicon ? mFadeInAnimatorListener : mFadeOutAnimatorListener);
        mFaviconOpacity = showFavicon ? 0.f : 1.f;
        mFaviconAnimation.start();
    }

    private void updateCaption() {
        if (mCaptionAnimation != null) mCaptionAnimation.cancel();
        int securityLevel = SecurityStateModel.getSecurityLevelForWebContents(getWebContents());
        EphemeralTabCaptionControl caption = getBarControl().getCaptionControl();
        caption.setCaptionText(UrlFormatter.formatUrlForSecurityDisplayOmitScheme(mUrl));
        caption.setSecurityIcon(EphemeralTabCoordinator.getSecurityIconResource(securityLevel));

        mCaptionAnimation = new CompositorAnimator(getAnimationHandler());
        mCaptionAnimation.setDuration(BASE_ANIMATION_DURATION_MS);
        mCaptionAnimation.removeAllListeners();
        mCaptionAnimation.addUpdateListener(mSecurityIconAnimationListener);
        mCaptionAnimation.start();
    }

    /**
     * @return Snaptshot value of the favicon opacity.
     */
    public float getFaviconOpacity() {
        return mFaviconOpacity;
    }

    // Generic Event Handling

    @Override
    public void handleBarClick(float x, float y) {
        super.handleBarClick(x, y);

        // TODO(donnd): Remove one of these cases when experiment is resolved.
        if (ChromeFeatureList.isEnabled(ChromeFeatureList.OVERLAY_NEW_LAYOUT)) {
            if (isCoordinateInsideCloseButton(x)) {
                closePanel(StateChangeReason.CLOSE_BUTTON, true);
            } else if (isCoordinateInsideOpenTabButton(x)) {
                if (canPromoteToNewTab() && mUrl != null) {
                    closePanel(StateChangeReason.TAB_PROMOTION, false);
                    mActivity.getCurrentTabCreator().createNewTab(
                            new LoadUrlParams(mUrl, PageTransition.LINK), TabLaunchType.FROM_LINK,
                            mActivity.getActivityTabProvider().get());
                }
            } else if (isPeeking()) {
                maximizePanel(StateChangeReason.SEARCH_BAR_TAP);
            }
        } else {
            // To keep things simple for now we just have both cases verbatim (without optimizing).
            if (isCoordinateInsideCloseButton(x)) {
                closePanel(StateChangeReason.CLOSE_BUTTON, true);
            } else {
                if (isPeeking()) {
                    maximizePanel(StateChangeReason.SEARCH_BAR_TAP);
                } else if (canPromoteToNewTab() && mUrl != null) {
                    closePanel(StateChangeReason.TAB_PROMOTION, false);
                    mActivity.getCurrentTabCreator().createNewTab(
                            new LoadUrlParams(mUrl, PageTransition.LINK), TabLaunchType.FROM_LINK,
                            mActivity.getActivityTabProvider().get());
                }
            }
        }
    }

    /**
     * @return Whether the panel content can be displayed in a new tab.
     */
    public boolean canPromoteToNewTab() {
        return !mActivity.isCustomTab();
    }

    /**
     * @return URL of the page to open in the panel.
     */
    public String getUrl() {
        return mUrl;
    }

    // Panel base methods

    @Override
    public void destroyComponents() {
        super.destroyComponents();
        destroyBarControl();
    }

    @Override
    public @PanelPriority int getPriority() {
        return PanelPriority.HIGH;
    }

    @Override
    protected boolean isSupportedState(@PanelState int state) {
        return state != PanelState.EXPANDED;
    }

    @Override
    protected void onClosed(@StateChangeReason int reason) {
        super.onClosed(reason);
        mFaviconOpacity = 0.f;
        if (mSceneLayer != null) mSceneLayer.hideTree();
    }

    @Override
    protected void updatePanelForCloseOrPeek(float percentage) {
        super.updatePanelForCloseOrPeek(percentage);
        getBarControl().updateForCloseOrPeek(percentage);
    }

    @Override
    protected void updatePanelForMaximization(float percentage) {
        super.updatePanelForMaximization(percentage);
        getBarControl().updateForMaximize(percentage);
    }

    /**
     * Request opening the ephemeral tab panel when triggered from context menu.
     * @param url URL of the content to open in the panel
     * @param text Link text which will appear on the tab bar.
     * @param isIncognito {@link True} if the panel is opened from an incognito tab.
     */
    public void requestOpenPanel(String url, String text, boolean isIncognito) {
        if (isShowing()) closePanel(StateChangeReason.RESET, false);
        mIsIncognito = isIncognito;
        mUrl = url;
        loadUrlInPanel(url);
        WebContents panelWebContents = getWebContents();
        if (panelWebContents != null) panelWebContents.onShow();
        getBarControl().setBarText(text);
        requestPanelShow(StateChangeReason.CLICK);
    }

    @Override
    public void onLayoutChanged(float width, float height, float visibleViewportOffsetY) {
        if (width != getWidth()) destroyBarControl();
        super.onLayoutChanged(width, height, visibleViewportOffsetY);
    }

    private EphemeralTabBarControl mEphemeralTabBarControl;

    /**
     * Creates the EphemeralTabBarControl, if needed. The Views are set to INVISIBLE, because
     * they won't actually be displayed on the screen (their snapshots will be displayed instead).
     */
    private EphemeralTabBarControl getBarControl() {
        assert mContainerView != null;
        assert mResourceLoader != null;
        if (mEphemeralTabBarControl == null) {
            mEphemeralTabBarControl =
                    new EphemeralTabBarControl(this, mContext, mContainerView, mResourceLoader);
        }
        assert mEphemeralTabBarControl != null;
        return mEphemeralTabBarControl;
    }

    /**
     * Destroys the EphemeralTabBarControl.
     */
    private void destroyBarControl() {
        if (mEphemeralTabBarControl != null) {
            mEphemeralTabBarControl.destroy();
            mEphemeralTabBarControl = null;
        }
    }

    //--------
    // METRICS
    //--------
    /** Records metrics for the peeked panel state. */
    private void recordMetricsForPeeked() {
        startPeekTimer();
        // Could be returning to Peek from Open.
        finishOpenTimer();
    }

    /** Records metrics when the panel has been fully opened. */
    private void recordMetricsForOpened() {
        startOpenTimer();
        finishPeekTimer();
    }

    /** Records metrics when the panel has been closed. */
    private void recordMetricsForClosed(@StateChangeReason int stateChangeReason) {
        finishPeekTimer();
        finishOpenTimer();
        RecordHistogram.recordBooleanHistogram("EphemeralTab.Ctr", mDidRecordFirstOpen);
        RecordHistogram.recordEnumeratedHistogram(
                "EphemeralTab.CloseReason", stateChangeReason, StateChangeReason.MAX_VALUE + 1);
        resetTimers();
    }

    /** Resets the metrics used by the timers. */
    private void resetTimers() {
        mDidRecordFirstPeek = false;
        mPanelPeekedNanoseconds = 0;
        mDidRecordFirstOpen = false;
        mPanelOpenedNanoseconds = 0;
    }

    /** Starts timing the peek state if it's not already been started. */
    private void startPeekTimer() {
        if (mPanelPeekedNanoseconds == 0) mPanelPeekedNanoseconds = System.nanoTime();
    }

    /** Finishes timing metrics for the first peek state, unless that has already been done. */
    private void finishPeekTimer() {
        if (!mDidRecordFirstPeek && mPanelPeekedNanoseconds != 0) {
            mDidRecordFirstPeek = true;
            long durationPeeking = (System.nanoTime() - mPanelPeekedNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "EphemeralTab.DurationPeeked", durationPeeking);
        }
    }

    /** Starts timing the open state if it's not already been started. */
    private void startOpenTimer() {
        if (mPanelOpenedNanoseconds == 0) mPanelOpenedNanoseconds = System.nanoTime();
    }

    /** Finishes timing metrics for the first open state, unless that has already been done. */
    private void finishOpenTimer() {
        if (!mDidRecordFirstOpen && mPanelOpenedNanoseconds != 0) {
            mDidRecordFirstOpen = true;
            long durationOpened = (System.nanoTime() - mPanelOpenedNanoseconds)
                    / TimeUtils.NANOSECONDS_PER_MILLISECOND;
            RecordHistogram.recordMediumTimesHistogram(
                    "EphemeralTab.DurationOpened", durationOpened);
        }
    }
}
