// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.RectF;

import org.chromium.base.MathUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.browser_ui.widget.animation.Interpolators;
import org.chromium.components.browser_ui.widget.gesture.SwipeGestureListener.ScrollDirection;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.resources.ResourceManager;

import java.util.ArrayList;
import java.util.List;

/**
 * Layout defining the animation and positioning of the tabs during the edge swipe effect.
 */
public class ToolbarSwipeLayout extends Layout {
    private static final boolean ANONYMIZE_NON_FOCUSED_TAB = true;

    // Unit is millisecond / screen.
    private static final float ANIMATION_SPEED_SCREEN_MS = 500.0f;

    // The time duration of the animation for switch to tab, Unit is millisecond.
    private static final long SWITCH_TO_TAB_DURATION_MS = 350;

    // This is the time step used to move the offset based on fling
    private static final float FLING_TIME_STEP = 1.0f / 30.0f;

    // This is the max contribution from fling in screen size percentage.
    private static final float FLING_MAX_CONTRIBUTION = 0.5f;

    private LayoutTab mLeftTab;
    private LayoutTab mRightTab;
    private LayoutTab mFromTab; // Set to either mLeftTab or mRightTab.
    private LayoutTab mToTab; // Set to mLeftTab or mRightTab or null if it is not determined.

    // Whether or not to show the toolbar.
    private boolean mMoveToolbar;

    // Offsets are in pixels [0, width].
    private float mOffsetStart;
    private float mOffset;
    private float mOffsetTarget;

    // These will be set from dimens.xml
    private final float mSpaceBetweenTabs;
    private final float mCommitDistanceFromEdge;

    private final BlackHoleEventFilter mBlackHoleEventFilter;
    private final TabListSceneLayer mSceneLayer;

    /**
     * @param context             The current Android's context.
     * @param updateHost          The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost          The {@link LayoutRenderHost} view for this layout.
     */
    public ToolbarSwipeLayout(
            Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost) {
        super(context, updateHost, renderHost);
        mBlackHoleEventFilter = new BlackHoleEventFilter(context);
        Resources res = context.getResources();
        final float pxToDp = 1.0f / res.getDisplayMetrics().density;
        mCommitDistanceFromEdge = res.getDimension(R.dimen.toolbar_swipe_commit_distance) * pxToDp;
        mSpaceBetweenTabs = res.getDimension(R.dimen.toolbar_swipe_space_between_tabs) * pxToDp;
        mSceneLayer = new TabListSceneLayer();
    }

    /**
     * @param moveToolbar Whether or not swiping this layout should also move the toolbar as well as
     *                    the content.
     */
    public void setMovesToolbar(boolean moveToolbar) {
        mMoveToolbar = moveToolbar;
    }

    @Override
    public @ViewportMode int getViewportMode() {
        // This seems counter-intuitive, but if the toolbar moves the android view is not showing.
        // That means the compositor has to draw it and therefore needs the fullscreen viewport.
        // Likewise, when the android view is showing, the compositor controls do not draw and the
        // content needs to pretend it does to draw correctly.
        // TODO(mdjones): Remove toolbar_impact_height from tab_layer.cc so this makes more sense.
        return mMoveToolbar ? ViewportMode.ALWAYS_FULLSCREEN
                            : ViewportMode.ALWAYS_SHOWING_BROWSER_CONTROLS;
    }

    @Override
    public boolean forceHideBrowserControlsAndroidView() {
        // If the toolbar moves, the android browser controls need to be hidden.
        return super.forceHideBrowserControlsAndroidView() || mMoveToolbar;
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);
        init();
        if (mTabModelSelector == null) return;
        Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null && tab.isNativePage()) mTabContentManager.cacheTabThumbnail(tab);

        TabModel model = mTabModelSelector.getCurrentModel();
        if (model == null) return;
        int fromTabId = mTabModelSelector.getCurrentTabId();
        if (fromTabId == TabModel.INVALID_TAB_INDEX) return;
        mFromTab = createLayoutTab(fromTabId, model.isIncognito(), NO_CLOSE_BUTTON, NEED_TITLE);
        prepareLayoutTabForSwipe(mFromTab, false);
    }

    public void swipeStarted(long time, @ScrollDirection int direction, float x, float y) {
        if (mTabModelSelector == null || mToTab != null || direction == ScrollDirection.DOWN) {
            return;
        }

        boolean dragFromLeftEdge = direction == ScrollDirection.RIGHT;
        // Finish off any other animations.
        forceAnimationToFinish();

        // Determine which tabs we're showing.
        TabModel model = mTabModelSelector.getCurrentModel();
        if (model == null) return;
        int fromIndex = model.index();
        if (fromIndex == TabModel.INVALID_TAB_INDEX) return;

        // On RTL, edge-dragging to the left is the next tab.
        int toIndex = (LocalizationUtils.isLayoutRtl() ^ dragFromLeftEdge) ? fromIndex - 1
                                                                           : fromIndex + 1;

        prepareSwipeTabAnimation(direction, fromIndex, toIndex);
    }

    /**
     * Prepare the tabs sliding animations. This method need to be called before
     * {@link #doTabSwitchAnimation(int, float, float, long)}.
     * @param direction The direction of the slide.
     * @param fromIndex The index of the tab which will be switched from.
     * @param toIndex The index of the tab which will be switched to.
     */
    private void prepareSwipeTabAnimation(
            @ScrollDirection int direction, int fromIndex, int toIndex) {
        boolean dragFromLeftEdge = direction == ScrollDirection.RIGHT;

        int leftIndex = dragFromLeftEdge ? toIndex : fromIndex;
        int rightIndex = !dragFromLeftEdge ? toIndex : fromIndex;
        int leftTabId = Tab.INVALID_TAB_ID;
        int rightTabId = Tab.INVALID_TAB_ID;

        TabModel model = mTabModelSelector.getCurrentModel();
        if (0 <= leftIndex && leftIndex < model.getCount()) {
            leftTabId = model.getTabAt(leftIndex).getId();
            mLeftTab = createLayoutTab(leftTabId, model.isIncognito(), NO_CLOSE_BUTTON, NEED_TITLE);
            prepareLayoutTabForSwipe(mLeftTab, leftIndex != fromIndex);
        }
        if (0 <= rightIndex && rightIndex < model.getCount()) {
            rightTabId = model.getTabAt(rightIndex).getId();
            mRightTab =
                    createLayoutTab(rightTabId, model.isIncognito(), NO_CLOSE_BUTTON, NEED_TITLE);
            prepareLayoutTabForSwipe(mRightTab, rightIndex != fromIndex);
        }
        // Prioritize toTabId because fromTabId likely has a live layer.
        int fromTabId = dragFromLeftEdge ? rightTabId : leftTabId;
        int toTabId = !dragFromLeftEdge ? rightTabId : leftTabId;
        List<Integer> visibleTabs = new ArrayList<Integer>();
        if (toTabId != Tab.INVALID_TAB_ID) visibleTabs.add(toTabId);
        if (fromTabId != Tab.INVALID_TAB_ID) visibleTabs.add(fromTabId);
        updateCacheVisibleIds(visibleTabs);

        mToTab = null;

        // Reset the tab offsets.
        mOffsetStart = dragFromLeftEdge ? 0 : getWidth();
        mOffset = 0;
        mOffsetTarget = 0;

        if (mLeftTab != null && mRightTab != null) {
            mLayoutTabs = new LayoutTab[] {mLeftTab, mRightTab};
        } else if (mLeftTab != null) {
            mLayoutTabs = new LayoutTab[] {mLeftTab};
        } else if (mRightTab != null) {
            mLayoutTabs = new LayoutTab[] {mRightTab};
        } else {
            mLayoutTabs = null;
        }

        requestUpdate();
    }

    private void prepareLayoutTabForSwipe(LayoutTab layoutTab, boolean anonymizeToolbar) {
        assert layoutTab != null;
        if (layoutTab.shouldStall()) layoutTab.setSaturation(0.0f);
        float heightDp = layoutTab.getOriginalContentHeight();
        layoutTab.setClipSize(layoutTab.getOriginalContentWidth(), heightDp);
        layoutTab.setScale(1.f);
        layoutTab.setBorderScale(1.f);
        layoutTab.setDecorationAlpha(0.f);
        layoutTab.setY(0.f);
        layoutTab.setShowToolbar(mMoveToolbar);
        layoutTab.setAnonymizeToolbar(anonymizeToolbar && ANONYMIZE_NON_FOCUSED_TAB);
    }

    public void swipeUpdated(long time, float x, float y, float dx, float dy, float tx, float ty) {
        mOffsetTarget = MathUtils.clamp(mOffsetStart + tx, 0, getWidth()) - mOffsetStart;
        requestUpdate();
    }

    public void swipeFlingOccurred(
            long time, float x, float y, float tx, float ty, float vx, float vy) {
        // Use the velocity to add on final step which simulate a fling.
        final float kickRangeX = getWidth() * FLING_MAX_CONTRIBUTION;
        final float kickRangeY = getHeight() * FLING_MAX_CONTRIBUTION;
        final float kickX = MathUtils.clamp(vx * FLING_TIME_STEP, -kickRangeX, kickRangeX);
        final float kickY = MathUtils.clamp(vy * FLING_TIME_STEP, -kickRangeY, kickRangeY);
        swipeUpdated(time, x, y, 0, 0, tx + kickX, ty + kickY);
    }

    public void swipeFinished(long time) {
        if (mFromTab == null || mTabModelSelector == null) return;

        // Figures out the tab to snap to and how to animate to it.
        float commitDistance = Math.min(mCommitDistanceFromEdge, getWidth() / 3);
        float offsetTo = 0;
        mToTab = mFromTab;
        if (mOffsetTarget > commitDistance && mLeftTab != null) {
            mToTab = mLeftTab;
            offsetTo += getWidth();
        } else if (mOffsetTarget < -commitDistance && mRightTab != null) {
            mToTab = mRightTab;
            offsetTo -= getWidth();
        }

        if (mToTab != mFromTab) {
            RecordUserAction.record("MobileSideSwipeFinished");
        }

        startHiding(mToTab.getId(), false);

        float start = mOffsetTarget;
        float end = offsetTo;
        long duration = (long) (ANIMATION_SPEED_SCREEN_MS * Math.abs(start - end) / getWidth());
        doTabSwitchAnimation(mToTab.getId(), start, end, duration);
    }

    /**
     * Perform the tabs sliding animations. {@link #prepareSwipeTabAnimation(int, int, int)} need to
     * be called before calling this method.
     * @param tabId The id of the tab which will be switched to.
     * @param start The start point of X coordinate for the animation.
     * @param end The end point of X coordinate for the animation.
     * @param duration The animation duration in millisecond.
     */
    private void doTabSwitchAnimation(int tabId, float start, float end, long duration) {
        // Animate gracefully the end of the swiping effect.
        forceAnimationToFinish();

        if (duration <= 0) return;

        CompositorAnimator offsetAnimation =
                CompositorAnimator.ofFloat(getAnimationHandler(), start, end, duration, null);
        offsetAnimation.addUpdateListener(animator -> {
            mOffset = animator.getAnimatedValue();
            mOffsetTarget = mOffset;
        });
        offsetAnimation.start();
    }

    public void swipeCancelled(long time) {
        swipeFinished(time);
    }

    @Override
    protected void updateLayout(long time, long dt) {
        super.updateLayout(time, dt);

        if (mFromTab == null) return;
        // In case the draw function get called before swipeStarted()
        if (mLeftTab == null && mRightTab == null) mRightTab = mFromTab;

        mOffset = smoothInput(mOffset, mOffsetTarget);
        boolean needUpdate = Math.abs(mOffset - mOffsetTarget) >= 0.1f;

        float rightX = 0.0f;
        float leftX = 0.0f;

        final boolean doEdge = mLeftTab != null ^ mRightTab != null;

        if (doEdge) {
            float progress = mOffset / getWidth();
            float direction = Math.signum(progress);
            float smoothedProgress =
                    Interpolators.DECELERATE_INTERPOLATOR.getInterpolation(Math.abs(progress));

            float maxSlide = getWidth() / 5.f;
            rightX = direction * smoothedProgress * maxSlide;
            leftX = rightX;
        } else {
            float progress = mOffset / getWidth();
            progress += mOffsetStart == 0.0f ? 0.0f : 1.0f;
            progress = MathUtils.clamp(progress, 0.0f, 1.0f);

            assert mLeftTab != null;
            assert mRightTab != null;
            rightX = MathUtils.interpolate(0.0f, getWidth() + mSpaceBetweenTabs, progress);
            // The left tab must be aligned on the right if the image is smaller than the screen.
            leftX = rightX - mSpaceBetweenTabs
                    - Math.min(getWidth(), mLeftTab.getOriginalContentWidth());
            // Compute final x post scale and ensure the tab's center point never passes the
            // center point of the screen.
            float screenCenterX = getWidth() / 2;
            rightX = Math.max(screenCenterX - mRightTab.getFinalContentWidth() / 2, rightX);
            leftX = Math.min(screenCenterX - mLeftTab.getFinalContentWidth() / 2, leftX);
        }

        if (mLeftTab != null) {
            mLeftTab.setX(leftX);
            needUpdate = updateSnap(dt, mLeftTab) || needUpdate;
        }

        if (mRightTab != null) {
            mRightTab.setX(rightX);
            needUpdate = updateSnap(dt, mRightTab) || needUpdate;
        }

        if (needUpdate) requestUpdate();
    }

    /**
     * Smoothes input signal. The definition of the input is lower than the
     * pixel density of the screen so we need to smooth the input to give the illusion of smooth
     * animation on screen from chunky inputs.
     * The combination of 30 pixels and 0.8f ensures that the output is not more than 6 pixels away
     * from the target.
     * TODO(dtrainor): This has nothing to do with time, just draw rate.
     *       Is this okay or do we want to have the interpolation based on the time elapsed?
     * @param current The current value of the signal.
     * @param input The raw input value.
     * @return The smoothed signal.
     */
    private float smoothInput(float current, float input) {
        current = MathUtils.clamp(current, input - 30, input + 30);
        return MathUtils.interpolate(current, input, 0.8f);
    }

    private void init() {
        mLayoutTabs = null;
        mFromTab = null;
        mLeftTab = null;
        mRightTab = null;
        mToTab = null;
        mOffsetStart = 0;
        mOffset = 0;
        mOffsetTarget = 0;
    }

    @Override
    protected EventFilter getEventFilter() {
        return mBlackHoleEventFilter;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            LayerTitleCache layerTitleCache, TabContentManager tabContentManager,
            ResourceManager resourceManager, BrowserControlsStateProvider browserControls) {
        super.updateSceneLayer(viewport, contentViewport, layerTitleCache, tabContentManager,
                resourceManager, browserControls);
        assert mSceneLayer != null;
        // contentViewport is intentionally passed for both parameters below.
        mSceneLayer.pushLayers(getContext(), contentViewport, contentViewport, this,
                layerTitleCache, tabContentManager, resourceManager, browserControls,
                SceneLayer.INVALID_RESOURCE_ID, 0, 0);
    }

    @Override
    public int getLayoutType() {
        return LayoutType.TOOLBAR_SWIPE;
    }

    /**
     * Perform the tabs sliding animations. If the new tab's index is smaller than the old one, new
     * tab slide in from left, and old one slide out to right, and vice versa.
     * @param toTabId The id of the next tab which will be switched to.
     * @param fromTabId The id of the previous tab which will be switched out.
     */
    public void switchToTab(int toTabId, int fromTabId) {
        int fromTabIndex =
                TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), fromTabId);
        int toTabIndex =
                TabModelUtils.getTabIndexById(mTabModelSelector.getCurrentModel(), toTabId);
        prepareSwipeTabAnimation(
                fromTabIndex < toTabIndex ? ScrollDirection.LEFT : ScrollDirection.RIGHT,
                fromTabIndex, toTabIndex);

        mToTab = fromTabIndex < toTabIndex ? mRightTab : mLeftTab;
        float end = fromTabIndex < toTabIndex ? -getWidth() : getWidth();
        startHiding(toTabId, false);
        doTabSwitchAnimation(toTabId, 0f, end, SWITCH_TO_TAB_DURATION_MS);
    }
}
