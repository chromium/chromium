// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.layouts.phone;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.RectEvaluator;
import android.animation.ValueAnimator;
import android.content.Context;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Build;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.eventfilter.BlackHoleEventFilter;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.NewTabAnimationUtils;
import org.chromium.chrome.browser.hub.RoundedCornerAnimatorUtil;
import org.chromium.chrome.browser.hub.ShrinkExpandAnimator;
import org.chromium.chrome.browser.hub.ShrinkExpandImageView;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.sensitive_content.SensitiveContentClient;
import org.chromium.components.sensitive_content.SensitiveContentFeatures;
import org.chromium.ui.base.LocalizationUtils;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.resources.ResourceManager;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;

/**
 * Layout for showing animations when new tabs are created. This is a drop-in replacement for the
 * {@link SimpleAnimationLayout} that uses Android animators rather than compositor animations and
 * uses modern UX designs.
 */
public class NewTabAnimationLayout extends Layout {
    private static final int FOREGROUND_ANIMATION_DURATION = 300;
    private final ViewGroup mAnimationHostView;
    private final CompositorViewHolder mCompositorViewHolder;
    private final BlackHoleEventFilter mBlackHoleEventFilter;

    private @Nullable StaticTabSceneLayer mSceneLayer;
    private AnimatorSet mTabCreatedForegroundAnimation;
    private int mNextTabId = Tab.INVALID_TAB_ID;

    /**
     * Creates an instance of the {@link NewTabAnimationLayout}.
     *
     * @param context The current Android's context.
     * @param updateHost The {@link LayoutUpdateHost} view for this layout.
     * @param renderHost The {@link LayoutRenderHost} view for this layout.
     * @param animationHostView The host view to use for animations and content sensitivity.
     * @param compositorViewHolderSupplier Supplier to the {@link CompositorViewHolder} instance.
     */
    public NewTabAnimationLayout(
            Context context,
            LayoutUpdateHost updateHost,
            LayoutRenderHost renderHost,
            ViewGroup animationHostView,
            ObservableSupplier<CompositorViewHolder> compositorViewHolderSupplier) {
        super(context, updateHost, renderHost);
        mAnimationHostView = animationHostView;
        mCompositorViewHolder = compositorViewHolderSupplier.get();
        mBlackHoleEventFilter = new BlackHoleEventFilter(context);
    }

    @Override
    public void onFinishNativeInitialization() {
        ensureSceneLayerExists();
    }

    @Override
    public void destroy() {
        if (mSceneLayer != null) {
            mSceneLayer.destroy();
            mSceneLayer = null;
        }
    }

    @Override
    public void setTabContentManager(TabContentManager tabContentManager) {
        super.setTabContentManager(tabContentManager);
        if (mSceneLayer != null && tabContentManager != null) {
            mSceneLayer.setTabContentManager(tabContentManager);
        }
    }

    @Override
    public @ViewportMode int getViewportMode() {
        return ViewportMode.USE_PREVIOUS_BROWSER_CONTROLS_STATE;
    }

    @Override
    public boolean handlesTabCreating() {
        return true;
    }

    @Override
    public boolean handlesTabClosing() {
        return false;
    }

    @Override
    protected EventFilter getEventFilter() {
        return mBlackHoleEventFilter;
    }

    @Override
    public SceneLayer getSceneLayer() {
        return mSceneLayer;
    }

    @Override
    public int getLayoutType() {
        return LayoutType.SIMPLE_ANIMATION;
    }

    @Override
    public void show(long time, boolean animate) {
        super.show(time, animate);

        mNextTabId = Tab.INVALID_TAB_ID;
        reset();

        if (mTabModelSelector == null || mTabContentManager == null) return;

        @Nullable Tab tab = mTabModelSelector.getCurrentTab();
        if (tab != null && tab.isNativePage()) {
            mTabContentManager.cacheTabThumbnail(tab);
        }
    }

    @Override
    public void doneHiding() {
        TabModelUtils.selectTabById(mTabModelSelector, mNextTabId, TabSelectionType.FROM_USER);
        super.doneHiding();
        updateAnimationHostViewSensitivity(Tab.INVALID_TAB_ID);
    }

    @Override
    protected void forceAnimationToFinish() {
        if (mTabCreatedForegroundAnimation != null) mTabCreatedForegroundAnimation.end();
        // TODO(crbug.com/40282469): Implement this for Background Animation.
    }

    @Override
    public void onTabCreating(int sourceTabId) {
        reset();

        ensureSourceTabCreated(sourceTabId);
        updateAnimationHostViewSensitivity(sourceTabId);
    }

    @Override
    public void onTabCreated(
            long time,
            int id,
            int index,
            int sourceId,
            boolean newIsIncognito,
            boolean background,
            float originX,
            float originY) {
        assert mTabModelSelector != null;
        Tab newTab = mTabModelSelector.getModel(newIsIncognito).getTabById(id);
        if (newTab != null
                && newTab.getLaunchType() == TabLaunchType.FROM_COLLABORATION_BACKGROUND_IN_GROUP) {
            // Tab selection will no-op for Tab.INVALID_TAB_ID. This operation should not change
            // the current tab. If for some reason this is the last tab it will be automatically
            // selected.
            mNextTabId = Tab.INVALID_TAB_ID;
            startHiding();
            return;
        }

        ensureSourceTabCreated(sourceId);
        updateAnimationHostViewSensitivity(sourceId);

        // TODO(crbug.com/40282469): Implement background animation
        if (!background) {
            tabCreatedInForeground(id, sourceId, newIsIncognito);
        }
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayerExists();
        if (!hasLayoutTab()) return;

        boolean needUpdate = updateSnap(dt, getLayoutTab());
        if (needUpdate) requestUpdate();
    }

    @Override
    protected void updateSceneLayer(
            RectF viewport,
            RectF contentViewport,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        ensureSceneLayerExists();

        LayoutTab layoutTab = getLayoutTab();
        layoutTab.set(LayoutTab.IS_ACTIVE_LAYOUT_SUPPLIER, this::isActive);
        layoutTab.set(LayoutTab.CONTENT_OFFSET, browserControls.getContentOffset());
        mSceneLayer.update(layoutTab);
    }

    @Override
    public boolean isRunningAnimations() {
        // TODO(crbug.com/40282469): Check background animation once it is implemented.
        return mTabCreatedForegroundAnimation != null && mTabCreatedForegroundAnimation.isRunning();
    }

    private void reset() {
        mLayoutTabs = null;
    }

    private boolean hasLayoutTab() {
        return mLayoutTabs != null && mLayoutTabs.length > 0;
    }

    private LayoutTab getLayoutTab() {
        assert hasLayoutTab();
        return mLayoutTabs[0];
    }

    private void ensureSceneLayerExists() {
        if (mSceneLayer != null) return;

        mSceneLayer = new StaticTabSceneLayer();
        if (mTabContentManager == null) return;

        mSceneLayer.setTabContentManager(mTabContentManager);
    }

    private void ensureSourceTabCreated(int sourceTabId) {
        if (hasLayoutTab() && mLayoutTabs[0].getId() == sourceTabId) return;

        @Nullable Tab tab = mTabModelSelector.getTabById(sourceTabId);
        if (tab == null) return;
        LayoutTab sourceLayoutTab = createLayoutTab(sourceTabId, tab.isIncognitoBranded());

        mLayoutTabs = new LayoutTab[] {sourceLayoutTab};
        updateCacheVisibleIds(Collections.singletonList(sourceTabId));
    }

    private void updateAnimationHostViewSensitivity(int sourceTabId) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.VANILLA_ICE_CREAM
                || !ChromeFeatureList.isEnabled(SensitiveContentFeatures.SENSITIVE_CONTENT)
                || !ChromeFeatureList.isEnabled(
                        SensitiveContentFeatures.SENSITIVE_CONTENT_WHILE_SWITCHING_TABS)) {
            return;
        }

        if (sourceTabId != TabModel.INVALID_TAB_INDEX) {
            // This code can be reached from both {@link NewTabAnimationLayout#onTabCreating}
            // and {@link NewTabAnimationLayout#onTabCreated}. If the content container is
            // already sensitive, there is no need to mark it as sensitive again.
            if (mAnimationHostView.getContentSensitivity() == View.CONTENT_SENSITIVITY_SENSITIVE) {
                return;
            }
            @Nullable Tab tab = mTabModelSelector.getTabById(sourceTabId);
            if (tab == null || !tab.getTabHasSensitiveContent()) {
                return;
            }
            mAnimationHostView.setContentSensitivity(View.CONTENT_SENSITIVITY_SENSITIVE);
            RecordHistogram.recordEnumeratedHistogram(
                    "SensitiveContent.SensitiveTabSwitchingAnimations",
                    SensitiveContentClient.TabSwitchingAnimation.NEW_TAB_IN_BACKGROUND,
                    SensitiveContentClient.TabSwitchingAnimation.COUNT);
        } else {
            mAnimationHostView.setContentSensitivity(View.CONTENT_SENSITIVITY_NOT_SENSITIVE);
        }
    }

    protected void setNextTabIdForTesting(int nextTabId) {
        mNextTabId = nextTabId;
    }

    @VisibleForTesting
    AnimatorSet getForegroundAnimatorSet() {
        return mTabCreatedForegroundAnimation;
    }

    /**
     * Animate opening a tab in the foreground.
     *
     * @param id The id of the new tab to animate.
     * @param sourceId The id of the tab that spawned this new tab.
     * @param newIsIncognito true if the new tab is an incognito tab.
     */
    private void tabCreatedInForeground(int id, int sourceId, boolean newIsIncognito) {
        LayoutTab newLayoutTab = createLayoutTab(id, newIsIncognito);
        assert mLayoutTabs.length == 1;
        mLayoutTabs = new LayoutTab[] {mLayoutTabs[0], newLayoutTab};
        updateCacheVisibleIds(new ArrayList<>(Arrays.asList(id, sourceId)));
        forceAnimationToFinish();

        // TODO(crbug.com/40933120): Investigate why the old tab flickers when switching to the new
        // tab.
        requestUpdate();

        ShrinkExpandImageView rectView = new ShrinkExpandImageView(getContext());
        @ColorInt
        int backgroundColor = NewTabAnimationUtils.getBackgroundColor(getContext(), newIsIncognito);
        rectView.setRoundedFillColor(backgroundColor);

        // TODO(crbug.com/40933120): Investigate why {@link
        // RoundedCornerImageView#setRoundedCorners} sometimes incorrectly detects the view as LTR
        // during the animation.
        boolean isRtl = LocalizationUtils.isLayoutRtl();
        rectView.setLayoutDirection(isRtl ? View.LAYOUT_DIRECTION_RTL : View.LAYOUT_DIRECTION_LTR);

        Rect initialRect = new Rect();
        Rect finalRect = new Rect();
        RectF compositorViewportRectf = new RectF();
        mCompositorViewHolder.getVisibleViewport(compositorViewportRectf);
        compositorViewportRectf.round(finalRect);
        mCompositorViewHolder.getWindowViewport(compositorViewportRectf);
        finalRect.bottom = Math.round(compositorViewportRectf.bottom);
        // Without this code, the upper corner shows a bit of blinking when running the
        // animation. This ensures the {@link ShrinkExpandImageView} fully covers the upper
        // corner.
        if (isRtl) {
            finalRect.right += 1;
        } else {
            finalRect.left -= 1;
        }
        finalRect.top -= 1;

        // TODO(crbug.com/40933120): Implement animation for bottom toolbar.
        NewTabAnimationUtils.updateRects(initialRect, finalRect, isRtl, /* isTopAligned= */ true);

        ShrinkExpandAnimator shrinkExpandAnimator =
                new ShrinkExpandAnimator(
                        rectView, initialRect, finalRect, /* searchBoxHeight= */ 0);
        ObjectAnimator rectAnimator =
                ObjectAnimator.ofObject(
                        shrinkExpandAnimator,
                        ShrinkExpandAnimator.RECT,
                        new RectEvaluator(),
                        initialRect,
                        finalRect);
        int radius =
                getContext()
                        .getResources()
                        .getDimensionPixelSize(R.dimen.new_tab_animation_rect_corner_radius);
        int[] startRadii = new int[] {0, radius, radius, radius};
        float scaleFactor = (float) initialRect.width() / finalRect.width();
        int endRadius = Math.round(radius * scaleFactor);
        int[] endRadii = new int[] {0, endRadius, endRadius, endRadius};
        rectView.setRoundedCorners(startRadii[0], startRadii[1], startRadii[2], startRadii[3]);
        ValueAnimator cornerAnimator =
                RoundedCornerAnimatorUtil.createRoundedCornerAnimator(
                        rectView, startRadii, endRadii);

        mTabCreatedForegroundAnimation = new AnimatorSet();
        mTabCreatedForegroundAnimation.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);
        mTabCreatedForegroundAnimation.setDuration(FOREGROUND_ANIMATION_DURATION);
        // TODO(crbug.com/40933120): Add fade animator and ensure {@link #doneHiding} gets called
        // properly in the animation cycle.
        mTabCreatedForegroundAnimation.playTogether(rectAnimator, cornerAnimator);
        mTabCreatedForegroundAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        startHiding();
                        mTabModelSelector.selectModel(newIsIncognito);
                        mNextTabId = id;
                        mAnimationHostView.removeView(rectView);
                        mTabCreatedForegroundAnimation = null;
                    }
                });
        mAnimationHostView.addView(rectView);
        rectView.reset(initialRect);
        rectView.post(mTabCreatedForegroundAnimation::start);
    }
}
