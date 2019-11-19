// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.RectF;

import org.chromium.chrome.browser.compositor.LayerTitleCache;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.compositor.animation.CompositorAnimator;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.EventFilter;
import org.chromium.chrome.browser.compositor.layouts.phone.stack.Stack;
import org.chromium.chrome.browser.compositor.scene_layer.SceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.ui.interpolators.BakedBezierInterpolator;
import org.chromium.ui.resources.ResourceManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collection;
import java.util.LinkedList;

/**
 * This class handles animating the opening of new tabs.
 */
public class SimpleAnimationLayout extends Layout {
    /** Animation for discarding a tab. */
    private CompositorAnimator mDiscardAnimator;

    /** The animation for a tab being created in the foreground. */
    private AnimatorSet mTabCreatedForegroundAnimation;

    /** The animation for a tab being created in the background. */
    private AnimatorSet mTabCreatedBackgroundAnimation;

    /** Fraction to scale tabs by during animation. */
    public static final float SCALE_FRACTION = 0.90f;

    /** Duration of the first step of the background animation: zooming out, rotating in */
    private static final long BACKGROUND_STEP1_DURATION = 300;
    /** Duration of the second step of the background animation: pause */
    private static final long BACKGROUND_STEP2_DURATION = 150;
    /** Duration of the third step of the background animation: zooming in, sliding out */
    private static final long BACKGROUND_STEP3_DURATION = 300;
    /** Percentage of the screen covered by the new tab */
    private static final float BACKGROUND_COVER_PCTG = 0.5f;

    /** The time duration of the animation */
    protected static final int FOREGROUND_ANIMATION_DURATION = 300;

    /** The time duration of the animation */
    protected static final int TAB_CLOSED_ANIMATION_DURATION = 250;

    /**
     * A cached {@link LayoutTab} representation of the currently closing tab. If it's not
     * null, it means tabClosing() has been called to start animation setup but
     * tabClosed() has not yet been called to finish animation startup
     */
    private LayoutTab mClosedTab;

    private LayoutTab mAnimatedTab;
    private final TabListSceneLayer mSceneLayer;
    private final BlackHoleEventFilter mBlackHoleEventFilter;

    /**
     * Creates an instance of the {@link SimpleAnimationLayout}.
     * @param context     The current Android's context.
     * @param updateHost  The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost  The {@link LayoutRenderHost} view for this layout.
     */
    public SimpleAnimationLayout(
            Context context, LayoutUpdateHost updateHost, LayoutRenderHost renderHost) {
        super(context, updateHost, renderHost);
        mBlackHoleEventFilter = new BlackHoleEventFilter(context);
        mSceneLayer = new TabListSceneLayer();
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE;
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        if (mTabModelSelector != null && mTabContentManager != null) {
            Tab tab = mTabModelSelector.getCurrentTab();
            if (tab != null && tab.isNativePage()) mTabContentManager.cacheTabThumbnail(tab);
        }

        reset();
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesTabClosing() {
        return true;
    }

    @Override
    protected void updateLayout(long time, long dt) {
        super.updateLayout(time, dt);
        if (mLayoutTabs == null) return;
        boolean needUpdate = false;
        for (int i = mLayoutTabs.length - 1; i >= 0; i--) {
            needUpdate = mLayoutTabs[i].updateSnap(dt) || needUpdate;
        }
        if (needUpdate) requestUpdate();
    }

    @Override
    public void onTabCreating(int sourceTabId) {
        super.onTabCreating(sourceTabId);
        reset();

        // Make sure any currently running animations can't influence tab if we are reusing it.
        forceAnimationToFinish();

        ensureSourceTabCreated(sourceTabId);
    }

    private void ensureSourceTabCreated(int sourceTabId) {
        if (mLayoutTabs != null && mLayoutTabs.length == 1
                && mLayoutTabs[0].getId() == sourceTabId) {
            return;
        }
        // Just draw the source tab on the screen.
        TabModel sourceModel = mTabModelSelector.getModelForTabId(sourceTabId);
        if (sourceModel == null) return;
        LayoutTab sourceLayoutTab =
                createLayoutTab(sourceTabId, sourceModel.isIncognito(), NO_CLOSE_BUTTON, NO_TITLE);
        sourceLayoutTab.setBorderAlpha(0.0f);

        mLayoutTabs = new LayoutTab[] {sourceLayoutTab};
        updateCacheVisibleIds(new LinkedList<Integer>(Arrays.asList(sourceTabId)));
    }

    @Override
    public void onTabCreated(long time, int id, int index, int sourceId, boolean newIsIncognito,
            boolean background, float originX, float originY) {
        super.onTabCreated(time, id, index, sourceId, newIsIncognito, background, originX, originY);
        ensureSourceTabCreated(sourceId);
        if (background && mLayoutTabs != null && mLayoutTabs.length > 0) {
            tabCreatedInBackground(id, sourceId, newIsIncognito, originX, originY);
        } else {
            tabCreatedInForeground(id, sourceId, newIsIncognito, originX, originY);
        }
    }

    /**
     * Animate opening a tab in the foreground.
     *
     * @param id             The id of the new tab to animate.
     * @param sourceId       The id of the tab that spawned this new tab.
     * @param newIsIncognito true if the new tab is an incognito tab.
     * @param originX        The X coordinate of the last touch down event that spawned this tab.
     * @param originY        The Y coordinate of the last touch down event that spawned this tab.
     */
    private void tabCreatedInForeground(
            int id, int sourceId, boolean newIsIncognito, float originX, float originY) {
        LayoutTab newLayoutTab = createLayoutTab(id, newIsIncognito, NO_CLOSE_BUTTON, NO_TITLE);
        if (mLayoutTabs == null || mLayoutTabs.length == 0) {
            mLayoutTabs = new LayoutTab[] {newLayoutTab};
        } else {
            mLayoutTabs = new LayoutTab[] {mLayoutTabs[0], newLayoutTab};
        }
        updateCacheVisibleIds(new LinkedList<Integer>(Arrays.asList(id, sourceId)));

        newLayoutTab.setBorderAlpha(0.0f);
        newLayoutTab.setStaticToViewBlend(1.f);

        forceAnimationToFinish();

        CompositorAnimationHandler handler = getAnimationHandler();
        CompositorAnimator scaleAnimation = CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.SCALE, 0f, 1f, FOREGROUND_ANIMATION_DURATION);
        CompositorAnimator alphaAnimation = CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.ALPHA, 0f, 1f, FOREGROUND_ANIMATION_DURATION);
        CompositorAnimator xAnimation = CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.X, originX, 0f, FOREGROUND_ANIMATION_DURATION);
        CompositorAnimator yAnimation = CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.Y, originY, 0f, FOREGROUND_ANIMATION_DURATION);

        mTabCreatedForegroundAnimation = new AnimatorSet();
        mTabCreatedForegroundAnimation.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        mTabCreatedForegroundAnimation.playTogether(
                scaleAnimation, alphaAnimation, xAnimation, yAnimation);
        mTabCreatedForegroundAnimation.start();

        mTabModelSelector.selectModel(newIsIncognito);
        startHiding(id, false);
    }

    /**
     * Animate opening a tab in the background.
     *
     * @param id             The id of the new tab to animate.
     * @param sourceId       The id of the tab that spawned this new tab.
     * @param newIsIncognito true if the new tab is an incognito tab.
     * @param originX        The X screen coordinate in dp of the last touch down event that spawned
     *                       this tab.
     * @param originY        The Y screen coordinate in dp of the last touch down event that spawned
     *                       this tab.
     */
    private void tabCreatedInBackground(
            int id, int sourceId, boolean newIsIncognito, float originX, float originY) {
        LayoutTab newLayoutTab = createLayoutTab(id, newIsIncognito, NO_CLOSE_BUTTON, NEED_TITLE);
        // mLayoutTabs should already have the source tab from tabCreating().
        assert mLayoutTabs.length == 1;
        LayoutTab sourceLayoutTab = mLayoutTabs[0];
        mLayoutTabs = new LayoutTab[] {sourceLayoutTab, newLayoutTab};
        updateCacheVisibleIds(new LinkedList<Integer>(Arrays.asList(id, sourceId)));

        forceAnimationToFinish();

        newLayoutTab.setBorderAlpha(0.0f);
        final float scale = SCALE_FRACTION;
        final float margin = Math.min(getWidth(), getHeight()) * (1.0f - scale) / 2.0f;

        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // Step 1: zoom out the source tab and bring in the new tab
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.SCALE, 1f, scale, BACKGROUND_STEP1_DURATION));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.X, 0f, margin, BACKGROUND_STEP1_DURATION));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, sourceLayoutTab, LayoutTab.Y, 0f, margin, BACKGROUND_STEP1_DURATION));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.BORDER_SCALE, 1f / scale, 1f, BACKGROUND_STEP1_DURATION));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.BORDER_ALPHA, 0f, 1f, BACKGROUND_STEP1_DURATION));

        AnimatorSet step1Source = new AnimatorSet();
        step1Source.setInterpolator(BakedBezierInterpolator.TRANSFORM_CURVE);
        step1Source.playTogether(animationList);

        float pauseX = margin;
        float pauseY = margin;
        if (getOrientation() == Orientation.PORTRAIT) {
            pauseY = BACKGROUND_COVER_PCTG * getHeight();
        } else {
            pauseX = BACKGROUND_COVER_PCTG * getWidth();
        }

        animationList = new ArrayList<>(4);

        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.ALPHA, 0f, 1f, BACKGROUND_STEP1_DURATION / 2));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.SCALE, 0f, scale, BACKGROUND_STEP1_DURATION));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.X, originX, pauseX, BACKGROUND_STEP1_DURATION));
        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.Y, originY, pauseY, BACKGROUND_STEP1_DURATION));

        AnimatorSet step1New = new AnimatorSet();
        step1New.setInterpolator(BakedBezierInterpolator.FADE_IN_CURVE);
        step1New.playTogether(animationList);

        AnimatorSet step1 = new AnimatorSet();
        step1.playTogether(step1New, step1Source);

        // step 2: pause and admire the nice tabs

        // step 3: zoom in the source tab and slide down the new tab
        animationList = new ArrayList<>(7);
        animationList.add(
                CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab, LayoutTab.SCALE, scale,
                        1f, BACKGROUND_STEP3_DURATION, BakedBezierInterpolator.TRANSFORM_CURVE));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab, LayoutTab.X,
                margin, 0f, BACKGROUND_STEP3_DURATION, BakedBezierInterpolator.TRANSFORM_CURVE));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab, LayoutTab.Y,
                margin, 0f, BACKGROUND_STEP3_DURATION, BakedBezierInterpolator.TRANSFORM_CURVE));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.BORDER_SCALE, 1f, 1f / scale, BACKGROUND_STEP3_DURATION,
                BakedBezierInterpolator.TRANSFORM_CURVE));
        animationList.add(CompositorAnimator.ofFloatProperty(handler, sourceLayoutTab,
                LayoutTab.BORDER_ALPHA, 1f, 0f, BACKGROUND_STEP3_DURATION,
                BakedBezierInterpolator.TRANSFORM_CURVE));

        animationList.add(CompositorAnimator.ofFloatProperty(
                handler, newLayoutTab, LayoutTab.ALPHA, 1f, 0f, BACKGROUND_STEP3_DURATION));
        if (getOrientation() == Orientation.PORTRAIT) {
            animationList.add(CompositorAnimator.ofFloatProperty(handler, newLayoutTab, LayoutTab.Y,
                    pauseY, getHeight(), BACKGROUND_STEP3_DURATION,
                    BakedBezierInterpolator.FADE_OUT_CURVE));
        } else {
            animationList.add(CompositorAnimator.ofFloatProperty(handler, newLayoutTab, LayoutTab.X,
                    pauseX, getWidth(), BACKGROUND_STEP3_DURATION,
                    BakedBezierInterpolator.FADE_OUT_CURVE));
        }

        AnimatorSet step3 = new AnimatorSet();
        step3.setStartDelay(BACKGROUND_STEP2_DURATION);
        step3.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationEnd(Animator animation) {
                // Once the animation has finished, we can switch layouts.
                startHiding(sourceId, false);
            }
        });
        step3.playTogether(animationList);

        mTabCreatedBackgroundAnimation = new AnimatorSet();
        mTabCreatedBackgroundAnimation.playSequentially(step1, step3);
        mTabCreatedBackgroundAnimation.start();

        mTabModelSelector.selectModel(newIsIncognito);
    }

    /**
     * Set up for the tab closing animation
     */
    @Override
    public void onTabClosing(long time, int id) {
        reset();

        // Make sure any currently running animations can't influence tab if we are reusing it.
        forceAnimationToFinish();

        // Create the {@link LayoutTab} for the tab before it is destroyed.
        TabModel model = mTabModelSelector.getModelForTabId(id);
        if (model != null) {
            mClosedTab = createLayoutTab(id, model.isIncognito(), NO_CLOSE_BUTTON, NO_TITLE);
            mClosedTab.setBorderAlpha(0.0f);
            mLayoutTabs = new LayoutTab[] {mClosedTab};
            updateCacheVisibleIds(new LinkedList<Integer>(Arrays.asList(id)));
        } else {
            mLayoutTabs = null;
            mClosedTab = null;
        }
        // Only close the id at the end when we are done querying the model.
        super.onTabClosing(time, id);
    }

    /**
     * Animate the closing of a tab
     */
    @Override
    public void onTabClosed(long time, int id, int nextId, boolean incognito) {
        super.onTabClosed(time, id, nextId, incognito);

        if (mClosedTab != null) {
            TabModel nextModel = mTabModelSelector.getModelForTabId(nextId);
            if (nextModel != null) {
                LayoutTab nextLayoutTab =
                        createLayoutTab(nextId, nextModel.isIncognito(), NO_CLOSE_BUTTON, NO_TITLE);
                nextLayoutTab.setDrawDecoration(false);

                mLayoutTabs = new LayoutTab[] {nextLayoutTab, mClosedTab};
                updateCacheVisibleIds(
                        new LinkedList<Integer>(Arrays.asList(nextId, mClosedTab.getId())));
            } else {
                mLayoutTabs = new LayoutTab[] {mClosedTab};
            }

            forceAnimationToFinish();
            mAnimatedTab = mClosedTab;
            mDiscardAnimator = CompositorAnimator.ofFloat(getAnimationHandler(), 0,
                    getDiscardRange(), TAB_CLOSED_ANIMATION_DURATION,
                    (CompositorAnimator a) -> setDiscardAmount(a.getAnimatedValue()));
            mDiscardAnimator.setInterpolator(BakedBezierInterpolator.FADE_OUT_CURVE);
            mDiscardAnimator.start();

            mClosedTab = null;
            if (nextModel != null) {
                mTabModelSelector.selectModel(nextModel.isIncognito());
            }
        }
        startHiding(nextId, false);
    }

    /**
     * Updates the position, scale, rotation and alpha values of mAnimatedTab.
     *
     * @param discard The value that specify how far along are we in the discard animation. 0 is
     *                filling the screen. Valid values are [-range .. range] where range is
     *                computed by {@link SimpleAnimationLayout#getDiscardRange()}.
     */
    private void setDiscardAmount(float discard) {
        if (mAnimatedTab != null) {
            final float range = getDiscardRange();
            final float scale = Stack.computeDiscardScale(discard, range, true);

            final float deltaX = mAnimatedTab.getOriginalContentWidth();
            final float deltaY = mAnimatedTab.getOriginalContentHeight() / 2.f;
            mAnimatedTab.setX(deltaX * (1.f - scale));
            mAnimatedTab.setY(deltaY * (1.f - scale));
            mAnimatedTab.setScale(scale);
            mAnimatedTab.setBorderScale(scale);
            mAnimatedTab.setAlpha(Stack.computeDiscardAlpha(discard, range));
        }
    }

    /**
     * @return The range of the discard amount.
     */
    private float getDiscardRange() {
        return Math.min(getWidth(), getHeight()) * Stack.DISCARD_RANGE_SCREEN;
    }

    @Override
    protected void forceAnimationToFinish() {
        super.forceAnimationToFinish();
        if (mDiscardAnimator != null) mDiscardAnimator.end();
        if (mTabCreatedForegroundAnimation != null) mTabCreatedForegroundAnimation.end();
        if (mTabCreatedBackgroundAnimation != null) mTabCreatedBackgroundAnimation.end();
    }

    /**
     * Resets the internal state.
     */
    private void reset() {
        mLayoutTabs = null;
        mAnimatedTab = null;
        mClosedTab = null;
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
            ResourceManager resourceManager, ChromeFullscreenManager fullscreenManager) {
        super.updateSceneLayer(viewport, contentViewport, layerTitleCache, tabContentManager,
                resourceManager, fullscreenManager);
        assert mSceneLayer != null;
        // The content viewport is intentionally sent as both params below.
        mSceneLayer.pushLayers(getContext(), contentViewport, contentViewport, this,
                layerTitleCache, tabContentManager, resourceManager, fullscreenManager,
                SceneLayer.INVALID_RESOURCE_ID, 0);
    }
}
