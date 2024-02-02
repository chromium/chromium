// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import static org.chromium.chrome.browser.hub.HubLayoutConstants.EXPAND_NEW_TAB_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubLayoutConstants.FADE_DURATION_MS;
import static org.chromium.chrome.browser.hub.HubLayoutConstants.TIMEOUT_MS;
import static org.chromium.chrome.browser.hub.HubLayoutConstants.TRANSLATE_DURATION_MS;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.Rect;
import android.graphics.RectF;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;

import androidx.annotation.ColorInt;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Promise;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.SyncOneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.scene_layer.SolidColorSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.resources.ResourceManager;

import java.util.Collections;
import java.util.function.DoubleConsumer;

/**
 * A {@link Layout} for Hub that has an empty or single tab {@link SceneLayer}. Android UI for a
 * toolbar and panes will be rendered atop this layout.
 *
 * <p>This implementation is a heavily modified fork of {@link TabSwitcherLayout} that will delegate
 * animations to the current pane.
 *
 * <p>Normally, this layout will show an empty {@link SceneLayer}. However, to facilitate thumbnail
 * capture and animations it may transiently host a {@link StaticTabSceneLayer}.
 */
public class HubLayout extends Layout implements HubLayoutController {
    private final @NonNull Callback<Pane> mOnPaneFocused = this::updateEmptyLayerColor;
    private final @NonNull LayoutStateProvider mLayoutStateProvider;
    private final @NonNull ViewGroup mRootView;
    private final @NonNull HubController mHubController;
    private final @NonNull PaneManager mPaneManager;
    private final @NonNull HubLayoutScrimController mScrimController;
    private final @NonNull DoubleConsumer mOnToolbarAlphaChange;

    /**
     * The previous {@link LayoutType}, valid between {@link #show(long, boolean)} and {@link
     * #doneShowing()}.
     */
    private final @NonNull ObservableSupplierImpl<Integer> mPreviousLayoutTypeSupplier =
            new ObservableSupplierImpl<>();

    private @Nullable SceneLayer mCurrentSceneLayer;

    /** Scene layer to facilitate thumbnail capture prior to starting a transition animation. */
    private @Nullable StaticTabSceneLayer mTabSceneLayer;

    /** An empty scene layer used to avoid drawing anything. */
    private @Nullable SolidColorSceneLayer mEmptySceneLayer;

    private @Nullable HubLayoutAnimationRunner mCurrentAnimationRunner;

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
        mPreviousLayoutTypeSupplier.set(layoutStateProvider.getActiveLayoutType());

        mLayoutStateProvider = layoutStateProvider;
        // This is the R.id.tab_switcher_view_holder. It is used by tablets for the tab switcher,
        // but in that case it is the animated object and is opaque. Here we will animate the
        // HubContainerView instead and just animate and just use this view as a transparent
        // container for z-indexing and geometry.
        // TODO(crbug/1499984): Consider removing the tab_switcher_view_holder post launch.
        mRootView = dependencyHolder.getHubRootView();
        mRootView.setBackgroundColor(Color.TRANSPARENT);

        HubManager hubManager = dependencyHolder.getHubManager();
        mHubController = hubManager.getHubController();
        mHubController.setHubLayoutController(this);
        mPaneManager = hubManager.getPaneManager();
        mPaneManager.getFocusedPaneSupplier().addObserver(mOnPaneFocused);
        mScrimController = dependencyHolder.getScrimController();
        mOnToolbarAlphaChange = dependencyHolder.getOnToolbarAlphaChange();
    }

    /** Returns the current {@link HubLayoutAnimationType}. */
    @HubLayoutAnimationType
    int getCurrentAnimationType() {
        return mCurrentAnimationRunner != null
                ? mCurrentAnimationRunner.getAnimationType()
                : HubLayoutAnimationType.NONE;
    }

    @Override
    public void selectTabAndHideHubLayout(int tabId) {
        TabModelUtils.selectTabById(mTabModelSelector, tabId, TabSelectionType.FROM_USER, false);
        startHiding();
    }

    @Override
    public ObservableSupplier<Integer> getPreviousLayoutTypeSupplier() {
        return mPreviousLayoutTypeSupplier;
    }

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
        mPaneManager.getFocusedPaneSupplier().removeObserver(mOnPaneFocused);
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
        mPreviousLayoutTypeSupplier.set(mLayoutStateProvider.getActiveLayoutType());
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

        try (TraceEvent e = TraceEvent.scoped("HubLayout.show")) {
            super.show(time, animate);

            forceAnimationToFinish();

            Promise<Bitmap> bitmapPromise = new Promise<>();
            @LayoutType int previousLayoutType = mPreviousLayoutTypeSupplier.get();
            if (previousLayoutType == LayoutType.BROWSING) {
                final Tab currentTab = mTabModelSelector.getCurrentTab();
                createLayoutTabForTabId(getIdForTab(currentTab));
                mCurrentSceneLayer = mTabSceneLayer;
                captureTabThumbnail(currentTab, bitmapPromise);
            } else {
                mCurrentSceneLayer = mEmptySceneLayer;
                bitmapPromise.fulfill(null);
            }

            // TODO(crbug/1516760): This is a stop gap solution that will work until we have more
            // panes. While we only have tab switcher panes, selecting the pane based on the
            // currently selected tab model is correct. However, if we have more panes we likely
            // want to be able to "select" a pane to focus as part of the HubLayout show transition.
            mPaneManager.focusPane(
                    mTabModelSelector.isIncognitoSelected()
                            ? PaneId.INCOGNITO_TAB_SWITCHER
                            : PaneId.TAB_SWITCHER);

            mHubController.onHubLayoutShow();

            HubContainerView containerView = mHubController.getContainerView();
            HubLayoutAnimatorProvider animatorProvider = createShowAnimatorProvider(containerView);

            Callback<Bitmap> thumbnailCallback = animatorProvider.getThumbnailCallback();
            if (thumbnailCallback != null) {
                bitmapPromise.then(thumbnailCallback);
            }
            updateEmptyLayerColor(mPaneManager.getFocusedPaneSupplier().get());

            assert mCurrentAnimationRunner == null;
            mCurrentAnimationRunner =
                    HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(
                            animatorProvider);
            mCurrentAnimationRunner.addListener(
                    new HubLayoutAnimationListener() {
                        @Override
                        public void onEnd(boolean wasForcedToFinish) {
                            doneShowing();
                            if (!wasForcedToFinish) {
                                // We don't want to hide the tab if the animation was forced to
                                // finish since that means another layout is going to show and
                                // hiding the tab could leave the tab in a bad state.
                                hideCurrentTab();
                            }
                        }
                    });

            mRootView.setVisibility(View.VISIBLE);
            containerView.setVisibility(View.INVISIBLE);
            LayoutParams params = (LayoutParams) containerView.getLayoutParams();
            // TODO(crbug/1523037): Change this to an assert and fix any broken tests.
            if (params == null) {
                params = new LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT);
            }
            mRootView.addView(containerView, /* index= */ 0, params);

            // For start surface transitions the behavior prior to Hub is to instantly switch
            // between layouts. Ideally, there should be a coordinated fade between the layouts, but
            // this is difficult to implement due to how LayoutManager works and results in a
            // flicker of the window's background color. To avoid this skip the animation for start
            // surface. See https://crbug.com/1520657.
            if (!animate || previousLayoutType == LayoutType.START_SURFACE) {
                // Don't post or wait for a layout as HubLayout is not in control of when the
                // previous layout was hidden and this avoids a possibly empty frame.
                queueAnimation();
                forceAnimationToFinish();
                hideCurrentTab();
            } else {
                containerView.runOnNextLayout(this::queueAnimation);
            }
        }
    }

    @Override
    public void doneShowing() {
        try (TraceEvent e = TraceEvent.scoped("HubLayout.doneShowing")) {
            super.doneShowing();
            mCurrentSceneLayer = mEmptySceneLayer;
            mCurrentAnimationRunner = null;
            resetLayoutTabs(/* clearVisibleIds= */ true);

            // This is a legacy value from the stack tab switcher, we are using at a proxy for Hub
            // shown. Prior to Hub this was recorded in when the TabSwitcherMediator finished
            // showing. However, this location is a better analog than TabSwitcherPaneMediator and
            // it keeps the call with doneHiding() symmetric.
            RecordUserAction.record("MobileToolbarShowStackView");
        }
    }

    @Override
    public void startHiding() {
        if (isStartingToHide()) return;

        try (TraceEvent e = TraceEvent.scoped("HubLayout.startHiding")) {
            super.startHiding();

            // Use the EXPAND_NEW_TAB animation if it is already prepared.
            if (getCurrentAnimationType() == HubLayoutAnimationType.EXPAND_NEW_TAB) {
                PostTask.postTask(TaskTraits.UI_DEFAULT, this::queueAnimation);
                return;
            }

            forceAnimationToFinish();

            int tabId = mTabModelSelector.getCurrentTabId();
            @LayoutType int nextLayoutType = mLayoutStateProvider.getNextLayoutType();
            if (nextLayoutType == LayoutType.BROWSING) {
                // During fade and translate animations the composited scene layer is visible. At
                // the end of the animation a composited tab will be fully visible. To ensure
                // continuity during the animation create and show the mTabSceneLayer with the
                // LayoutTab for the tabId that will be shown once the animation finishes.
                createLayoutTabForTabId(tabId);
                mCurrentSceneLayer = mTabSceneLayer;
            } else {
                mCurrentSceneLayer = mEmptySceneLayer;
            }
            updateEmptyLayerColor(mPaneManager.getFocusedPaneSupplier().get());

            HubContainerView containerView = mHubController.getContainerView();
            HubLayoutAnimatorProvider animatorProvider =
                    createHideAnimatorProvider(containerView, nextLayoutType);

            Callback<Bitmap> thumbnailCallback = animatorProvider.getThumbnailCallback();
            if (thumbnailCallback != null) {
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
                    HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(
                            animatorProvider);
            mCurrentAnimationRunner.addListener(
                    new HubLayoutAnimationListener() {
                        @Override
                        public void onEnd(boolean wasForcedToFinish) {
                            doneHiding();
                        }
                    });

            // For start surface transitions the behavior prior to Hub is to instantly switch
            // between layouts. Ideally, there should be a coordinated fade between the layouts, but
            // this is difficult to implement due to how LayoutManager works and results in a
            // flicker of the window's background color. To avoid this skip the animation for start
            // surface. See https://crbug.com/1520657.
            if (nextLayoutType == LayoutType.START_SURFACE) {
                // Posting is okay here as start surface won't show until doneHiding happens.
                PostTask.postTask(
                        TaskTraits.UI_DEFAULT,
                        () -> {
                            queueAnimation();
                            forceAnimationToFinish();
                        });
            } else {
                PostTask.postTask(TaskTraits.UI_DEFAULT, this::queueAnimation);
            }
        }
    }

    @Override
    public void doneHiding() {
        try (TraceEvent e = TraceEvent.scoped("HubLayout.doneHiding")) {
            HubContainerView containerView = mHubController.getContainerView();
            containerView.setVisibility(View.INVISIBLE);
            mRootView.removeView(containerView);
            mRootView.setVisibility(View.GONE);
            mCurrentAnimationRunner = null;
            mHubController.onHubLayoutDoneHiding();

            mCurrentSceneLayer = mEmptySceneLayer;
            // Don't clear the visible ids because the next layout might have already updated them.
            resetLayoutTabs(/* clearVisibleIds= */ false);

            // This is a legacy value from the stack tab switcher, we are using at a proxy for Hub
            // hidden.
            RecordUserAction.record("MobileExitStackView");

            // Do this last so the Hub is ready to show again.
            super.doneHiding();
        }
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
        // This is for the legacy backpress handler which will soon be obsolete.
        return mHubController.onHubLayoutBackPressed();
    }

    @Override
    public void onTabCreated(
            long time,
            int tabId,
            int tabIndex,
            int sourceTabId,
            boolean newIsIncognito,
            boolean background,
            float originX,
            float originY) {
        super.onTabCreated(
                time, tabId, tabIndex, sourceTabId, newIsIncognito, background, originX, originY);

        // Background tab creation or creation while hiding does not trigger a Hub layout
        // transition.
        if (background || isStartingToHide()) return;

        // Tablet Hub doesn't handle new tab animations.
        if (DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            selectTabAndHideHubLayout(tabId);
            return;
        }

        forceAnimationToFinish();

        mCurrentSceneLayer = mEmptySceneLayer;
        updateEmptyLayerColor(mPaneManager.getFocusedPaneSupplier().get());

        @ColorInt int backgroundColor;
        if (newIsIncognito) {
            backgroundColor = ChromeColors.getPrimaryBackgroundColor(getContext(), newIsIncognito);
        } else {
            // See https://crbug/1507124.
            backgroundColor =
                    ChromeColors.getSurfaceColor(
                            getContext(), R.dimen.home_surface_background_color_elevation);
        }
        SyncOneshotSupplierImpl<ShrinkExpandAnimationData> animationDataSupplier =
                new SyncOneshotSupplierImpl<>();
        HubLayoutAnimatorProvider animatorProvider =
                ShrinkExpandHubLayoutAnimationFactory.createNewTabAnimatorProvider(
                        mHubController.getContainerView(),
                        animationDataSupplier,
                        backgroundColor,
                        EXPAND_NEW_TAB_DURATION_MS,
                        mOnToolbarAlphaChange);

        HubContainerView containerView = mHubController.getContainerView();
        assert containerView.isLaidOut();
        Rect containerViewRect = new Rect();
        containerView.getGlobalVisibleRect(containerViewRect);

        View paneHost = mHubController.getPaneHostView();
        assert paneHost.isLaidOut();
        Rect finalRect = new Rect();
        paneHost.getGlobalVisibleRect(finalRect);
        // Ignore left offset and just ensure the width is correct. See crbug/1502437.
        int leftOffset = finalRect.left;
        finalRect.offset(-leftOffset, -containerViewRect.top);

        // TODO(crbug/1492207): Supply this from HubController so it can look like the animation
        // originated from wherever on the Hub was clicked. This defaults to the top left of the
        // pane host view.
        int x = finalRect.left;
        int y = finalRect.top;
        Rect initialRect = new Rect(x, y, x + 1, y + 1);

        animationDataSupplier.set(
                new ShrinkExpandAnimationData(
                        initialRect,
                        finalRect,
                        /* thumbnailSize= */ null,
                        /* useFallbackAnimation= */ false));

        mCurrentAnimationRunner =
                HubLayoutAnimationRunnerFactory.createHubLayoutAnimationRunner(animatorProvider);
        mCurrentAnimationRunner.addListener(
                new HubLayoutAnimationListener() {
                    @Override
                    public void onEnd(boolean wasForcedToFinish) {
                        doneHiding();
                    }
                });

        selectTabAndHideHubLayout(tabId);
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
    protected void updateSceneLayer(
            RectF viewport,
            RectF contentViewport,
            TabContentManager tabContentManager,
            ResourceManager resourceManager,
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
        } else if (mPreviousLayoutTypeSupplier.get() == LayoutType.START_SURFACE || pane == null) {
            return FadeHubLayoutAnimationFactory.createFadeInAnimatorProvider(
                    containerView, FADE_DURATION_MS, mOnToolbarAlphaChange);
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
                    containerView, FADE_DURATION_MS, mOnToolbarAlphaChange);
        }
        return pane.createHideHubLayoutAnimatorProvider(containerView);
    }

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
            mEmptySceneLayer = new SolidColorSceneLayer();
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

    private void createLayoutTabForTabId(int tabId) {
        LayoutTab layoutTab = createLayoutTab(tabId, mTabModelSelector.isIncognitoSelected());
        mLayoutTabs = new LayoutTab[] {layoutTab};
        updateCacheVisibleIds(Collections.singletonList(tabId));
    }

    private void resetLayoutTabs(boolean clearVisibleIds) {
        if (clearVisibleIds) {
            // Clear the visible IDs as once mLayoutTabs is empty tabs thumbnails cannot be
            // captured. This prevents thumbnail requests from waiting indefinitely.
            updateCacheVisibleIds(Collections.emptyList());
        }

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

    private void updateEmptyLayerColor(@Nullable Pane pane) {
        if (mEmptySceneLayer == null) return;

        mEmptySceneLayer.setBackgroundColor(mHubController.getBackgroundColor(pane));
    }

    private void captureTabThumbnail(
            @Nullable Tab currentTab, @NonNull Promise<Bitmap> bitmapPromise) {
        if (currentTab == null) {
            bitmapPromise.fulfill(null);
            return;
        }

        mTabContentManager.cacheTabThumbnailWithCallback(
                currentTab,
                /* returnBitmap= */ true,
                (bitmap) -> {
                    if (bitmap != null || !currentTab.isNativePage()) {
                        bitmapPromise.fulfill(bitmap);
                        return;
                    }

                    // NativePage may not produce a new bitmap if no state has changed. Refetch from
                    // disk. For a normal tab we can't do this fallback as the thumbnail may be
                    // stale.
                    mTabContentManager.getEtc1TabThumbnailWithCallback(
                            currentTab.getId(), bitmapPromise::fulfill);
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
