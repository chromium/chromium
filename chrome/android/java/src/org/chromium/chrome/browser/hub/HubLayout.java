// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.animation.AnimatorSet;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.RectF;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.Layout.ViewportMode;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.resources.ResourceManager;

import java.util.Collections;

/**
 * A {@link Layout} for Hub that has an empty or single tab {@link SceneLayer}. Android UI for
 * a toolbar and panes will be rendered atop this layout.
 *
 * This implementation is a heavily modified fork of {@link TabSwitcherLayout} that will delegate
 * animations to the current pane.
 *
 * Normally, this layout will show an empty {@link SceneLayer}. However, to facilitate thumbnail
 * capture and animations it may transiently host a {@link StaticTabSceneLayer}.
 */
public class HubLayout extends Layout {
    // Copied from TabSwitcherLayout.
    private static final long TRANSLATE_DURATION_MS = 300L;
    private static final long FADE_DURATION_MS = 325L;
    private static final long TIMEOUT_MS = 250L;

    private SceneLayer mCurrentSceneLayer;
    /** Scene layer to facilitate thumbnail capture prior to starting a transition animation. */
    private StaticTabSceneLayer mTabSceneLayer;
    /** An empty scene layer used to avoid drawing anything. */
    private SceneLayer mEmptySceneLayer;

    private final LayoutStateProvider mLayoutStateProvider;
    private final ViewGroup mRootView;
    private final HubController mHubController;
    private final PaneManager mPaneManager;
    private final HubLayoutScrimController mScrimController;

    private HubLayoutAnimationRunner mCurrentAnimationRunner;

    /**
     * The previous {@link LayoutType}, valid between {@link #show(long, boolean)} and
     * {@link #doneShowing()}.
     */
    private @LayoutType int mPreviousLayoutType;

    /**
     * Create the {@link Layout} to show the Hub on.
     *
     * @param context The current Android {@link Context}.
     * @param updateHost The {@link LayoutUpdateHost} for the {@link LayoutManager}.
     * @param renderHost The {@link LayoutRenderHost} for the {@link LayoutManager}.
     * @param layoutStateProvider The {@link LayoutStateProvider} for the {@link LayoutManager}.
     * @param dependencyHolder The {@link HubLayoutDependencyHolder} that holds dependencies for
     *     HubLayout.
     */
    public HubLayout(
            @NonNull Context context,
            @NonNull LayoutUpdateHost updateHost,
            @NonNull LayoutRenderHost renderHost,
            @NonNull LayoutStateProvider layoutStateProvider,
            @NonNull HubLayoutDependencyHolder dependencyHolder) {
        super(context, updateHost, renderHost);
        mLayoutStateProvider = layoutStateProvider;
        mRootView = dependencyHolder.getHubRootView();
        HubManager hubManager = dependencyHolder.getHubManager();
        mHubController = hubManager.getHubController();
        mPaneManager = hubManager.getPaneManager();
        mScrimController = dependencyHolder.getScrimController();
    }

    /** Returns the current {@link HubLayoutAnimationType}. */
    @HubLayoutAnimationType
    int getCurrentAnimationType() {
        return mCurrentAnimationRunner != null
                ? mCurrentAnimationRunner.getAnimationType()
                : HubLayoutAnimationType.NONE;
    }

    // Layout.java Implementation:

    @Override
    public void onFinishNativeInitialization() {
        super.onFinishNativeInitialization();
        ensureSceneLayersExist();
    }

    @Override
    public void setTabContentManager(TabContentManager tabContentManager) {
        super.setTabContentManager(tabContentManager);
        if (mTabSceneLayer != null && mTabContentManager != null) {
            mTabSceneLayer.setTabContentManager(mTabContentManager);
        }
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mTabSceneLayer != null) {
            mTabSceneLayer.destroy();
            mTabSceneLayer = null;
        }
        if (mEmptySceneLayer != null) {
            mEmptySceneLayer.destroy();
            mEmptySceneLayer = null;
        }
        mCurrentSceneLayer = null;
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayersExist();
        super.updateLayout(time, dt);
        if (!hasLayoutTab()) return;

        boolean needUpdate = updateSnap(dt, getLayoutTab());
        if (needUpdate) requestUpdate();
    }

    @Override
    public void contextChanged(Context context) {
        super.contextChanged(context);
        // This is called before show() and before getActiveLayoutType() changes so we can know what
        // layout the Hub is showing from.
        mPreviousLayoutType = mLayoutStateProvider.getActiveLayoutType();
    }

    @Override
    public @ViewportMode int getViewportMode() {
        // Hub has its own toolbar.
        // TODO(crbug/1487209): Confirm this doesn't cause the toolbar to disappear too early or
        // without animation.
        return ViewportMode.ALWAYS_FULLSCREEN;
    }

    @Override
    public void show(long time, boolean animate) {
        if (isStartingToShow()) return;

        super.show(time, animate);

        forceAnimationToFinish();

        mHubController.onHubLayoutShow();

        HubContainerView containerView = mHubController.getContainerView();
        HubLayoutAnimatorProvider animatorProvider = createShowAnimatorProvider(containerView);

        Callback<Bitmap> thumbnailCallback = animatorProvider.getThumbnailCallback();
        if (mPreviousLayoutType == LayoutType.BROWSING) {
            final Tab currentTab = mTabModelSelector.getCurrentTab();
            createLayoutTabsForTab(currentTab);
            mCurrentSceneLayer = mTabSceneLayer;
            captureTabThumbnail(currentTab, thumbnailCallback);
        } else {
            mCurrentSceneLayer = mEmptySceneLayer;
            Callback.runNullSafe(thumbnailCallback, null);
        }

        assert mCurrentAnimationRunner == null;
        mCurrentAnimationRunner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);
        mCurrentAnimationRunner.addListener(
                new HubLayoutAnimationListener() {
                    @Override
                    public void onEnd(boolean wasForcedToFinish) {
                        doneShowing();
                        if (!wasForcedToFinish) {
                            // We don't want to hide the tab if the animation was forced to finish
                            // since that means another layout is going to show and hiding the tab
                            // could leave the tab in a bad state.
                            hideCurrentTab();
                        }
                    }
                });

        containerView.setVisibility(View.INVISIBLE);
        mRootView.addView(
                containerView,
                new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        containerView.runOnNextLayout(this::queueAnimation);
    }

    @Override
    public void doneShowing() {
        super.doneShowing();
        mCurrentSceneLayer = mEmptySceneLayer;
        mPreviousLayoutType = LayoutType.NONE;
        mCurrentAnimationRunner = null;
        resetLayoutTabs();
    }

    @Override
    public void startHiding(int nextTabId, boolean hintAtTabSelection) {
        if (isStartingToHide()) return;

        super.startHiding(nextTabId, hintAtTabSelection);

        // Use the NEW_TAB animation if it is already prepared.
        if (getCurrentAnimationType() == HubLayoutAnimationType.NEW_TAB) {
            PostTask.postTask(TaskTraits.UI_DEFAULT, this::queueAnimation);
            return;
        }

        forceAnimationToFinish();

        mCurrentSceneLayer = mEmptySceneLayer;

        @LayoutType
        int nextLayoutType = mLayoutStateProvider.getNextLayoutType();
        HubContainerView containerView = mHubController.getContainerView();
        HubLayoutAnimatorProvider animatorProvider =
                createHideAnimatorProvider(containerView, nextLayoutType);

        Callback<Bitmap> thumbnailCallback = animatorProvider.getThumbnailCallback();
        if (thumbnailCallback != null) {
            // TODO(crbug/1495121): Remove the need for this logic if feasible and just get the
            // value from TabModelSelector.
            int tabId =
                    nextTabId != Tab.INVALID_TAB_ID
                            ? nextTabId
                            : mTabModelSelector.getCurrentTabId();
            if (nextLayoutType == LayoutType.BROWSING
                    && mTabContentManager != null
                    && tabId != Tab.INVALID_TAB_ID) {
                mTabContentManager.getEtc1TabThumbnailWithCallback(tabId, thumbnailCallback);
            } else {
                thumbnailCallback.onResult(null);
            }
        }

        assert mCurrentAnimationRunner == null;
        mCurrentAnimationRunner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);
        mCurrentAnimationRunner.addListener(
                new HubLayoutAnimationListener() {
                    @Override
                    public void onEnd(boolean wasForcedToFinish) {
                        doneHiding();
                    }
                });

        PostTask.postTask(TaskTraits.UI_DEFAULT, this::queueAnimation);
    }

    @Override
    public void doneHiding() {
        super.doneHiding();
        HubContainerView containerView = mHubController.getContainerView();
        mRootView.removeView(containerView);
        containerView.setVisibility(View.INVISIBLE);
        mCurrentAnimationRunner = null;
        mHubController.onHubLayoutDoneHiding();
    }

    @Override
    protected void forceAnimationToFinish() {
        if (mCurrentAnimationRunner == null) return;

        // Immediately start any pending animations.
        mHubController.getContainerView().runOnNextLayoutRunnables();

        // Force the animation to run to completion.
        mCurrentAnimationRunner.forceAnimationToFinish();
        mCurrentAnimationRunner = null;

        if (mScrimController != null) {
            mScrimController.forceAnimationToFinish();
        }
    }

    @Override
    public boolean onBackPressed() {
        // TODO(crbug/1487209): Forward this to the HubManager. Legacy backpress handler soon to be
        // obsolete.
        return false;
    }

    @Override
    public void onTabCreated(long time, int tabId, int tabIndex, int sourceTabId,
            boolean newIsIncognito, boolean background, float originX, float originY) {
        super.onTabCreated(
                time, tabId, tabIndex, sourceTabId, newIsIncognito, background, originX, originY);

        // Tablet Hub doesn't handle new tab animations.
        if (background || DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            return;
        }

        forceAnimationToFinish();

        mCurrentSceneLayer = mEmptySceneLayer;

        // TODO(crbug/1487209): Replace this with a real animation.
        HubLayoutAnimator newTabAnimator =
                new HubLayoutAnimator(HubLayoutAnimationType.NEW_TAB, new AnimatorSet(), null);
        HubLayoutAnimatorProvider animatorProvider =
                new PresetHubLayoutAnimatorProvider(newTabAnimator);
        mCurrentAnimationRunner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);
        mCurrentAnimationRunner.addListener(
                new HubLayoutAnimationListener() {
                    @Override
                    public void onEnd(boolean wasForcedToFinish) {
                        doneHiding();
                    }
                });
        // The animation will run as part of startHiding which will be called soon.
    }

    @Override
    protected boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        // Return whether an animation is running. Ignore the inputs like TabSwitcherLayout.
        return mCurrentAnimationRunner != null;
    }

    @Override
    public boolean handlesTabClosing() {
        // Tabs can be closed from the Tab Switcher panes without changing layout.
        return true;
    }

    @Override
    public boolean handlesTabCreating() {
        // For the new tab animation.
        return true;
    }

    @Override
    public boolean canHostBeFocusable() {
        // TODO(crbug/1487209): Consider returning false here so that the omnibox doesn't steal
        // focus.
        return super.canHostBeFocusable();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        return mCurrentSceneLayer;
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            TabContentManager tabContentManager, ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        ensureSceneLayersExist();
        super.updateSceneLayer(
                viewport, contentViewport, tabContentManager, resourceManager, browserControls);

        if (mCurrentSceneLayer != mTabSceneLayer) return;

        LayoutTab layoutTab = getLayoutTab();
        layoutTab.set(LayoutTab.IS_ACTIVE_LAYOUT_SUPPLIER, this::isActive);
        layoutTab.set(LayoutTab.CONTENT_OFFSET, browserControls.getContentOffset());
        mTabSceneLayer.update(layoutTab);
    }

    @Override
    public @LayoutType int getLayoutType() {
        // Pretend to be the TAB_SWITCHER for initial development to minimize churn outside of
        // LayoutManager.
        return LayoutType.TAB_SWITCHER;
    }

    @Override
    public boolean isRunningAnimations() {
        return mCurrentAnimationRunner != null;
    }

    // Visible for testing or spying

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    HubLayoutAnimatorProvider createShowAnimatorProvider(@NonNull HubContainerView containerView) {
        @Nullable Pane pane = mPaneManager.getFocusedPaneSupplier().get();

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            return TranslateHubLayoutAnimationFactory.createTranslateUpAnimatorProvider(
                    containerView, mScrimController, TRANSLATE_DURATION_MS);
        } else if (mPreviousLayoutType == LayoutType.START_SURFACE || pane == null) {
            return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                    containerView, FADE_DURATION_MS);
        }
        return pane.createShowHubLayoutAnimatorProvider(containerView);
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    HubLayoutAnimatorProvider createHideAnimatorProvider(
            @NonNull HubContainerView containerView, @LayoutType int nextLayoutType) {
        @Nullable Pane pane = mPaneManager.getFocusedPaneSupplier().get();

        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            return TranslateHubLayoutAnimationFactory.createTranslateDownAnimatorProvider(
                    containerView, mScrimController, TRANSLATE_DURATION_MS);
        } else if (nextLayoutType == LayoutType.START_SURFACE || pane == null) {
            return FadeHubLayoutAnimationFactory.createFadeOutAnimatorProvider(
                    containerView, FADE_DURATION_MS);
        }
        return pane.createHideHubLayoutAnimatorProvider(containerView);
    }

    // Internal helpers

    private void queueAnimation() {
        if (mCurrentAnimationRunner == null) return;

        mCurrentAnimationRunner.runWithWaitForAnimatorTimeout(TIMEOUT_MS);
    }

    private void ensureSceneLayersExist() {
        if (mTabSceneLayer == null) {
            mTabSceneLayer = new StaticTabSceneLayer();
            if (mTabContentManager != null) {
                mTabSceneLayer.setTabContentManager(mTabContentManager);
            }
        }
        if (mEmptySceneLayer == null) {
            mEmptySceneLayer = new SceneLayer();
        }
        if (mCurrentSceneLayer == null && mEmptySceneLayer != null) {
            mCurrentSceneLayer = mEmptySceneLayer;
        }
    }

    private boolean hasLayoutTab() {
        return mLayoutTabs != null && mLayoutTabs.length > 0;
    }

    private LayoutTab getLayoutTab() {
        assert hasLayoutTab();
        return mLayoutTabs[0];
    }

    private void createLayoutTabsForTab(@Nullable Tab tab) {
        int tabId = getIdForTab(tab);
        LayoutTab layoutTab = createLayoutTab(tabId, mTabModelSelector.isIncognitoSelected());
        mLayoutTabs = new LayoutTab[] {layoutTab};
        updateCacheVisibleIds(Collections.singletonList(tabId));
    }

    private void resetLayoutTabs() {
        // Clear the visible IDs as once mLayoutTabs is empty tabs thumbnails cannot be captured.
        // This prevents thumbnail requests from waiting indefinitely.
        updateCacheVisibleIds(Collections.emptyList());

        // mLayoutTabs is used in conjunction with mTabSceneLayer to capture a tab thumbnail for
        // the last visible Tab prior to transitioning to the Hub. This should be nulled once
        // the capture is completed.
        mLayoutTabs = null;
    }

    private void hideCurrentTab() {
        Tab currentTab = mTabModelSelector.getCurrentTab();
        if (currentTab != null) {
            currentTab.hide(TabHidingType.TAB_SWITCHER_SHOWN);
        }
    }

    private void captureTabThumbnail(
            @Nullable Tab currentTab, @Nullable Callback<Bitmap> thumbnailCallback) {
        if (currentTab == null) {
            Callback.runNullSafe(thumbnailCallback, null);
            return;
        }

        if (thumbnailCallback == null) {
            mTabContentManager.cacheTabThumbnail(currentTab);
            return;
        }

        mTabContentManager.cacheTabThumbnailWithCallback(
                currentTab,
                /* returnBitmap= */ true,
                (bitmap) -> {
                    if (bitmap != null || !currentTab.isNativePage()) {
                        thumbnailCallback.onResult(bitmap);
                        return;
                    }

                    // NativePage may not produce a new bitmap if no state has changed. Refetch from
                    // disk. For a normal tab we can't do this fallback as the thumbnail may be
                    // stale.
                    mTabContentManager.getEtc1TabThumbnailWithCallback(
                            currentTab.getId(), thumbnailCallback);
                });
    }

    /**
     * Returns the tab id for a {@link Tab}.
     * @param tab The {@link Tab} to get an ID for or null.
     * @return the {@code tab}'s ID or {@link Tab#INVALID_TAB_ID} if null.
     */
    private int getIdForTab(@Nullable Tab tab) {
        return tab == null ? Tab.INVALID_TAB_ID : tab.getId();
    }
}
