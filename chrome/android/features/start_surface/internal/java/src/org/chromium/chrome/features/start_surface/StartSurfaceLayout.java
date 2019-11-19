// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.Supplier;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFeatureUtilities;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.chrome.browser.ui.widget.animation.Interpolators;
import org.chromium.ui.resources.ResourceManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedList;
import java.util.List;
import java.util.Locale;

/**
 * A {@link Layout} that shows all tabs in one grid or carousel view.
 */
public class StartSurfaceLayout extends Layout implements StartSurface.OverviewModeObserver {
    private static final String TAG = "SSLayout";

    // Duration of the transition animation
    public static final long ZOOMING_DURATION = 300;
    private static final int BACKGROUND_FADING_DURATION_MS = 150;

    // Field trial parameter for whether skipping slow zooming animation.
    private static final String SKIP_SLOW_ZOOMING_PARAM = "skip-slow-zooming";
    private static final boolean DEFAULT_SKIP_SLOW_ZOOMING = true;

    // The transition animation from a tab to the tab switcher.
    private AnimatorSet mTabToSwitcherAnimation;
    private boolean mIsAnimating;

    private final TabListSceneLayer mSceneLayer = new TabListSceneLayer();
    private final StartSurface mStartSurface;
    private final StartSurface.Controller mController;
    private final TabSwitcher.TabListDelegate mTabListDelegate;
    // To force Toolbar finishes its animation when this Layout finished hiding.
    private final LayoutTab mDummyLayoutTab;

    private float mBackgroundAlpha;

    private int mFrameCount;
    private long mStartTime;
    private long mLastFrameTime;
    private long mMaxFrameInterval;
    private int mStartFrame;

    interface PerfListener {
        void onAnimationDone(
                int frameRendered, long elapsedMs, long maxFrameInterval, int dirtySpan);
    }

    private PerfListener mPerfListenerForTesting;

    public StartSurfaceLayout(Context context, LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost, StartSurface startSurface) {
        super(context, updateHost, renderHost);
        mDummyLayoutTab = createLayoutTab(Tab.INVALID_TAB_ID, false, false, false);
        mDummyLayoutTab.setShowToolbar(true);
        mStartSurface = startSurface;
        mStartSurface.setOnTabSelectingListener(this::onTabSelecting);
        mController = mStartSurface.getController();
        mController.addOverviewModeObserver(this);
        mTabListDelegate = mStartSurface.getTabListDelegate();
    }

    // StartSurface.OverviewModeObserver implementation.
    @Override
    public void startedShowing() {}

    @Override
    public void finishedShowing() {
        doneShowing();
        // The Tab-to-GTS animation is done, and it's time to renew the thumbnail without causing
        // janky frames.
        // When animation is off, the thumbnail is already updated when showing the GTS.
        if (TabFeatureUtilities.isTabToGtsAnimationEnabled()) {
            // Delay thumbnail taking a bit more to make it less likely to happen before the
            // thumbnail taking triggered by ThumbnailFetcher. See crbug.com/996385 for details.
            new Handler().postDelayed(() -> {
                Tab currentTab = mTabModelSelector.getCurrentTab();
                if (currentTab != null) mTabContentManager.cacheTabThumbnail(currentTab);
            }, ZOOMING_DURATION);
        }
    }

    @Override
    public void startedHiding() {}

    @Override
    public void finishedHiding() {
        // The Android View version of GTS overview is hidden.
        // If not doing GTS-to-Tab transition animation, we show the fade-out instead, which was
        // already done.
        if (!TabFeatureUtilities.isTabToGtsAnimationEnabled()) {
            postHiding();
            return;
        }
        // If we are doing GTS-to-Tab transition animation, we start showing the Bitmap version of
        // the GTS overview in the background while expanding the thumbnail to the viewport.
        expandTab(mTabListDelegate.getThumbnailLocationOfCurrentTab(true));
    }

    // Layout implementation.
    @Override
    public void setTabModelSelector(TabModelSelector modelSelector, TabContentManager manager) {
        super.setTabModelSelector(modelSelector, manager);
        mSceneLayer.setTabModelSelector(modelSelector);
    }

    @Override
    public LayoutTab getLayoutTab(int id) {
        return mDummyLayoutTab;
    }

    @Override
    public void destroy() {
        if (mController != null) {
            mController.removeOverviewModeObserver(this);
        }
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        boolean showShrinkingAnimation =
                animate && TabFeatureUtilities.isTabToGtsAnimationEnabled();
        boolean quick = mTabListDelegate.prepareOverview();
        Log.d(TAG, "SkipSlowZooming = " + getSkipSlowZooming());
        if (getSkipSlowZooming()) {
            showShrinkingAnimation &= quick;
        }

        // Keep the current tab in mLayoutTabs even if we are not going to show the shrinking
        // animation so that thumbnail taking is not blocked.
        LayoutTab sourceLayoutTab = createLayoutTab(mTabModelSelector.getCurrentTabId(),
                mTabModelSelector.isIncognitoSelected(), NO_CLOSE_BUTTON, NO_TITLE);
        sourceLayoutTab.setDecorationAlpha(0);

        mLayoutTabs = new LayoutTab[] {sourceLayoutTab};

        if (!showShrinkingAnimation) {
            mController.showOverview(animate);
            return;
        }

        shrinkTab(() -> mTabListDelegate.getThumbnailLocationOfCurrentTab(false));
    }

    @Override
    protected void updateLayout(long time, long dt) {
        super.updateLayout(time, dt);
        if (mLayoutTabs == null) return;

        assert mLayoutTabs.length >= 1;
        boolean needUpdate = mLayoutTabs[0].updateSnap(dt);
        if (needUpdate) requestUpdate();
    }

    @Override
    public void startHiding(int nextId, boolean hintAtTabSelection) {
        super.startHiding(nextId, hintAtTabSelection);

        int sourceTabId = nextId;
        if (sourceTabId == Tab.INVALID_TAB_ID) sourceTabId = mTabModelSelector.getCurrentTabId();
        LayoutTab sourceLayoutTab = createLayoutTab(
                sourceTabId, mTabModelSelector.isIncognitoSelected(), NO_CLOSE_BUTTON, NO_TITLE);
        sourceLayoutTab.setDecorationAlpha(0);

        List<LayoutTab> layoutTabs = new ArrayList<>();
        layoutTabs.add(sourceLayoutTab);

        if (sourceTabId != mTabModelSelector.getCurrentTabId()) {
            // Keep the original tab in mLayoutTabs to unblock thumbnail taking at the end of the
            // animation.
            LayoutTab originalTab = createLayoutTab(mTabModelSelector.getCurrentTabId(),
                    mTabModelSelector.isIncognitoSelected(), NO_CLOSE_BUTTON, NO_TITLE);
            originalTab.setScale(0);
            originalTab.setDecorationAlpha(0);
            layoutTabs.add(originalTab);
        }
        mLayoutTabs = layoutTabs.toArray(new LayoutTab[0]);

        updateCacheVisibleIds(new LinkedList<>(Arrays.asList(sourceTabId)));

        mIsAnimating = true;
        mController.hideOverview(!TabFeatureUtilities.isTabToGtsAnimationEnabled());
    }

    @Override
    public void doneHiding() {
        super.doneHiding();
        RecordUserAction.record("MobileExitStackView");
    }

    @Override
    public boolean onBackPressed() {
        if (mTabModelSelector.getCurrentModel().getCount() == 0) return false;
        return mController.onBackPressed();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public boolean handlesTabClosing() {
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesCloseAll() {
        return false;
    }

    @Override
    protected void forceAnimationToFinish() {
        super.forceAnimationToFinish();
        if (mTabToSwitcherAnimation != null) {
            if (mTabToSwitcherAnimation.isRunning()) mTabToSwitcherAnimation.end();
        }
    }

    /**
     * Animate shrinking a tab to a target {@link Rect} area.
     * @param target The target {@link Rect} area.
     */
    private void shrinkTab(Supplier<Rect> target) {
        forceAnimationToFinish();

        LayoutTab sourceLayoutTab = mLayoutTabs[0];
        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // Step 1: zoom out the source tab
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.SCALE, () -> 1f, () -> {
                    return target.get().width() / (getWidth() * mDpToPx);
                }, ZOOMING_DURATION, Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.X, () -> 0f, () -> {
                    return target.get().left / mDpToPx;
                }, ZOOMING_DURATION, Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.Y, () -> 0f, () -> {
                    return target.get().top / mDpToPx;
                }, ZOOMING_DURATION, Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        // TODO(crbug.com/964406): when shrinking to the bottom row, bottom of the tab goes up and
        // down, making the "create group" visible for a while.
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.MAX_CONTENT_HEIGHT, sourceLayoutTab.getUnclampedOriginalContentHeight(),
                getWidth(), ZOOMING_DURATION, Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));

        CompositorAnimator backgroundAlpha =
                CompositorAnimator.ofFloat(handler, 0f, 1f, BACKGROUND_FADING_DURATION_MS,
                        animator -> mBackgroundAlpha = animator.getAnimatedValue());
        backgroundAlpha.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animationList.add(backgroundAlpha);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.playTogether(animationList);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                // Step 2: fade in the real GTS RecyclerView.
                mController.showOverview(true);

                reportAnimationPerf(true);
            }
        });
        mStartFrame = mFrameCount;
        mStartTime = SystemClock.elapsedRealtime();
        mLastFrameTime = SystemClock.elapsedRealtime();
        mMaxFrameInterval = 0;
        mTabToSwitcherAnimation.start();
    }

    /**
     * Animate expanding a tab from a source {@link Rect} area.
     * @param source The source {@link Rect} area.
     */
    private void expandTab(Rect source) {
        LayoutTab sourceLayoutTab = mLayoutTabs[0];

        forceAnimationToFinish();
        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // Zoom in the source tab
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.SCALE, source.width() / (getWidth() * mDpToPx), 1, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab, LayoutTab.X,
                source.left / mDpToPx, 0f, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab, LayoutTab.Y,
                source.top / mDpToPx, 0f, ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));
        // TODO(crbug.com/964406): when shrinking to the bottom row, bottom of the tab goes up and
        // down, making the "create group" visible for a while.
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.MAX_CONTENT_HEIGHT, getWidth(),
                sourceLayoutTab.getUnclampedOriginalContentHeight(), ZOOMING_DURATION,
                Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));

        CompositorAnimator backgroundAlpha =
                CompositorAnimator.ofFloat(handler, 1f, 0f, BACKGROUND_FADING_DURATION_MS,
                        animator -> mBackgroundAlpha = animator.getAnimatedValue());
        backgroundAlpha.setInterpolator(Interpolators.FAST_OUT_LINEAR_IN_INTERPOLATOR);
        animationList.add(backgroundAlpha);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.playTogether(animationList);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                postHiding();

                reportAnimationPerf(false);
            }
        });
        mStartFrame = mFrameCount;
        mStartTime = SystemClock.elapsedRealtime();
        mLastFrameTime = SystemClock.elapsedRealtime();
        mMaxFrameInterval = 0;
        mTabToSwitcherAnimation.start();
    }

    private void postHiding() {
        mTabListDelegate.postHiding();
        mIsAnimating = false;
        doneHiding();
    }

    @VisibleForTesting
    void setPerfListenerForTesting(PerfListener perfListener) {
        mPerfListenerForTesting = perfListener;
    }

    @VisibleForTesting
    public StartSurface getStartSurfaceForTesting() {
        return mStartSurface;
    }

    private void reportAnimationPerf(boolean isShrinking) {
        int frameRendered = mFrameCount - mStartFrame;
        long elapsedMs = SystemClock.elapsedRealtime() - mStartTime;
        long lastDirty = mTabListDelegate.getLastDirtyTimeForTesting();
        int dirtySpan = (int) (lastDirty - mStartTime);
        float fps = 1000.f * frameRendered / elapsedMs;
        String message = String.format(Locale.US,
                "fps = %.2f (%d / %dms), maxFrameInterval = %d, dirtySpan = %d", fps, frameRendered,
                elapsedMs, mMaxFrameInterval, dirtySpan);

        // TODO(crbug.com/964406): stop logging it after this feature stabilizes.
        if (!ChromeVersionInfo.isStableBuild()) {
            Log.i(TAG, message);
        }

        String suffix;
        if (isShrinking) {
            suffix = ".Shrink";
        } else {
            suffix = ".Expand";
        }

        // TODO(crbug.com/982018): Separate histograms for carousel tab switcher.
        RecordHistogram.recordCount100Histogram(
                "GridTabSwitcher.FramePerSecond" + suffix, (int) fps);
        RecordHistogram.recordTimesHistogram(
                "GridTabSwitcher.MaxFrameInterval" + suffix, mMaxFrameInterval);
        RecordHistogram.recordTimesHistogram("GridTabSwitcher.DirtySpan" + suffix, dirtySpan);

        if (mPerfListenerForTesting != null) {
            mPerfListenerForTesting.onAnimationDone(
                    frameRendered, elapsedMs, mMaxFrameInterval, dirtySpan);
        }
    }

    private boolean getSkipSlowZooming() {
        String skip = ChromeFeatureList.getFieldTrialParamByFeature(
                ChromeFeatureList.TAB_TO_GTS_ANIMATION, SKIP_SLOW_ZOOMING_PARAM);
        if (TextUtils.equals(skip, "")) return DEFAULT_SKIP_SLOW_ZOOMING;
        return Boolean.valueOf(skip);
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, ChromeFullscreenManager fullscreenManager) {
        super.updateSceneLayer(viewport, contentViewport, layerTitleCache, tabContentManager,
                resourceManager, fullscreenManager);
        assert mSceneLayer != null;
        // The content viewport is intentionally sent as both params below.
        mSceneLayer.pushLayers(getContext(), contentViewport, contentViewport, this,
                layerTitleCache, tabContentManager, resourceManager, fullscreenManager,
                TabFeatureUtilities.isTabToGtsAnimationEnabled() ? mTabListDelegate.getResourceId()
                                                                 : 0,
                mBackgroundAlpha);
        mFrameCount++;
        if (mLastFrameTime != 0) {
            long elapsed = SystemClock.elapsedRealtime() - mLastFrameTime;
            mMaxFrameInterval = Math.max(mMaxFrameInterval, elapsed);
        }
        mLastFrameTime = SystemClock.elapsedRealtime();
    }

    @Override
    public boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        return mTabToSwitcherAnimation == null && !mIsAnimating;
    }
}
