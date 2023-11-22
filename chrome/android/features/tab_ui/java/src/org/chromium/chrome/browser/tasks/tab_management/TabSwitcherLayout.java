// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.AnimatorSet;
import android.animation.ObjectAnimator;
import android.animation.RectEvaluator;
import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.graphics.RectF;
import android.os.Handler;
import android.os.SystemClock;
import android.util.Size;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.ColorInt;
import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.Layout;
import org.chromium.chrome.browser.compositor.layouts.LayoutRenderHost;
import org.chromium.chrome.browser.compositor.layouts.LayoutUpdateHost;
import org.chromium.chrome.browser.compositor.layouts.components.LayoutTab;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.compositor.scene_layer.SolidColorSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.StaticTabSceneLayer;
import org.chromium.chrome.browser.compositor.scene_layer.TabListSceneLayer;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.ShrinkExpandAnimator;
import org.chromium.chrome.browser.hub.ShrinkExpandImageView;
import org.chromium.chrome.browser.layouts.EventFilter;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimationHandler;
import org.chromium.chrome.browser.layouts.animation.CompositorAnimator;
import org.chromium.chrome.browser.layouts.scene_layer.SceneLayer;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.TabListDelegate;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher.TabSwitcherViewObserver;
import org.chromium.chrome.browser.util.ChromeAccessibilityUtil;
import org.chromium.components.browser_ui.styles.ChromeColors;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.components.browser_ui.widget.scrim.ScrimProperties;
import org.chromium.components.version_info.VersionInfo;
import org.chromium.ui.accessibility.AccessibilityState;
import org.chromium.ui.animation.AnimationPerformanceTracker;
import org.chromium.ui.base.DeviceFormFactor;
import org.chromium.ui.interpolators.Interpolators;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.resources.ResourceManager;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.ref.WeakReference;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

/**
 * A {@link Layout} that shows all tabs in one grid or list view.
 */
public class TabSwitcherLayout extends Layout {
    private static final String TAG = "TSLayout";

    @IntDef({TransitionType.NONE, TransitionType.SHRINK, TransitionType.EXPAND})
    @Retention(RetentionPolicy.SOURCE)
    public @interface TransitionType {
        int NONE = 0;
        int SHRINK = 1;
        int EXPAND = 2;
    }

    // Duration of the transition animation
    public static final long ZOOMING_DURATION = 325;
    private static final int TRANSLATE_DURATION_MS = 300;
    private static final int FOREGROUND_DURATION_MS = 300;
    private static final int BACKGROUND_FADING_DURATION_MS = 150;
    private static final int SCRIM_FADE_DURATION_MS = 350;

    private static final String TRACE_SHOW_TAB_SWITCHER = "TabSwitcherLayout.Show.TabSwitcher";
    private static final String TRACE_HIDE_TAB_SWITCHER = "TabSwitcherLayout.Hide.TabSwitcher";
    private static final String TRACE_DONE_SHOWING_TAB_SWITCHER = "TabSwitcherLayout.DoneShowing";
    private static final String TRACE_DONE_HIDING_TAB_SWITCHER = "TabSwitcherLayout.DoneHiding";

    private boolean mHasPerfListenerForTesting;

    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    // ObjectAnimator only holds a weak reference to its target. Hold a strong reference here to
    // prevent the animation from cancelling early.
    private ShrinkExpandAnimator mShrinkExpandAnimator;

    // The transition animation from a tab to the tab switcher.
    private AnimatorSet mTabToSwitcherAnimation;
    private boolean mRunningNewTabAnimation;
    private AnimatorSet mNewTabAnimation;
    private boolean mIsAnimatingHide;
    private @Nullable WeakReference<ConditionalAnimationRunner> mConditionalAnimationRunnerRef;

    private boolean mShowEmptyLayer;

    /**
     * StaticTabSceneLayer is used to facilitate thumbnail capture when using Android based
     * animations. Used when sGridTabSwitcherAndroidAnimations is enabled.
     */
    private StaticTabSceneLayer mTabSceneLayer;
    /**
     * An empty SceneLayer is used to avoid drawing a SceneLayer with any content when the
     * animation is not running. Used when sGridTabSwitcherAndroidAnimations is enabled.
     */
    private SolidColorSceneLayer mEmptySceneLayer;
    /**
     * TabListSceneLayer is used to show the dynamic resource for the Tab Switcher when using
     * composited animations. Used when sGridTabSwitcherAndroidAnimations is not enabled.
     */
    private TabListSceneLayer mTabListSceneLayer;
    private final TabSwitcher mTabSwitcher;
    private final TabSwitcher.Controller mController;
    private final TabSwitcherViewObserver mTabSwitcherObserver;
    @Nullable
    private final ViewGroup mScrimAnchor;
    @Nullable
    private final ScrimCoordinator mScrimCoordinator;
    private final TabSwitcher.TabListDelegate mGridTabListDelegate;
    private final LayoutStateProvider mLayoutStateProvider;

    private boolean mIsInitialized;

    private float mBackgroundAlpha;

    private final AnimationPerformanceTracker mAnimationTracker;
    private @TransitionType int mAnimationTransitionType;
    private long mTransitionStartTime;

    private boolean mSkipDoneShowingOnTabSwitcherFinishedShowing;
    private boolean mAndroidViewFinishedShowing;

    private Handler mHandler;
    private Runnable mFinishedShowingRunnable;
    private boolean mBackToStartSurface;

    private static class HideTabCallback {
        private Runnable mRunnable;
        private boolean mIsCancelled;

        HideTabCallback(Runnable runnable) {
            mRunnable = runnable;
        }

        void run() {
            RecordHistogram.recordBooleanHistogram("Android.TabSwitcher.TabHidden", !mIsCancelled);
            if (mIsCancelled) return;

            assert mRunnable != null;
            mRunnable.run();
            mRunnable = null;
        }

        void cancel() {
            mIsCancelled = true;
            mRunnable = null;
        }
    }

    private HideTabCallback mHideTabCallback;

    private ShrinkExpandImageView mTabJavaView;

    public TabSwitcherLayout(Context context, LayoutUpdateHost updateHost,
            LayoutStateProvider layoutStateProvider, LayoutRenderHost renderHost,
            BrowserControlsStateProvider browserControlsStateProvider, TabSwitcher tabSwitcher,
            @Nullable ViewGroup tabSwitcherScrimAnchor,
            @Nullable ScrimCoordinator scrimCoordinator) {
        super(context, updateHost, renderHost);
        mBrowserControlsStateProvider = browserControlsStateProvider;
        mTabSwitcher = tabSwitcher;
        mLayoutStateProvider = layoutStateProvider;
        mController = mTabSwitcher.getController();
        mTabSwitcher.setOnTabSelectingListener(this::onTabSelecting);
        mGridTabListDelegate = mTabSwitcher.getTabListDelegate();
        mScrimAnchor = tabSwitcherScrimAnchor;
        mScrimCoordinator = scrimCoordinator;
        mHandler = new Handler();
        mAnimationTracker = new AnimationPerformanceTracker();
        mAnimationTracker.addListener((metrics) -> {
            reportAnimationPerf(metrics, mTransitionStartTime, mAnimationTransitionType);
        });

        mTabJavaView = new ShrinkExpandImageView(context);
        mTabJavaView.setVisibility(View.GONE);
        mController.getTabSwitcherContainer().addView(mTabJavaView,
                new ViewGroup.LayoutParams(Math.round(getWidth()), Math.round(getHeight())));

        mTabSwitcherObserver =
                new TabSwitcherViewObserver() {
                    @Override
                    public void startedShowing() {
                        mAndroidViewFinishedShowing = false;
                    }

                    @Override
                    public void finishedShowing() {
                        mAndroidViewFinishedShowing = true;
                        if (!DeviceFormFactor.isNonMultiDisplayContextOnTablet(context)
                                && !mSkipDoneShowingOnTabSwitcherFinishedShowing) {
                            doneShowing();
                        }

                        if (ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()) {
                            if (ChromeFeatureList.sHideTabOnTabSwitcher.isEnabled()) {
                                final Tab currentTab = mTabModelSelector.getCurrentTab();
                                if (currentTab != null) {
                                    RecordHistogram.recordBooleanHistogram(
                                            "Android.TabSwitcher.TabHidden", true);
                                    currentTab.hide(TabHidingType.TAB_SWITCHER_SHOWN);
                                }
                            }
                            mShowEmptyLayer = true;
                            resetLayoutTabs();
                            return;
                        }

                        // When Tab-to-GTS animation is done, it's time to renew the thumbnail
                        // without causing janky frames. When animation is off or not used, the
                        // thumbnail is already updated when showing the GTS. Tab-to-GTS animation
                        // is not invoked for tablet tab switcher polish.
                        if (TabUiFeatureUtilities.isTabToGtsAnimationEnabled(getContext())) {
                            // Delay thumbnail taking a bit more to make it less likely to happen
                            // before the thumbnail taking triggered by ThumbnailFetcher. See
                            // crbug.com/996385 for details.
                            mFinishedShowingRunnable =
                                    () -> {
                                        final Tab currentTab = mTabModelSelector.getCurrentTab();
                                        if (currentTab != null) {
                                            if (ChromeFeatureList.sHideTabOnTabSwitcher
                                                    .isEnabled()) {
                                                if (mHideTabCallback != null) {
                                                    mHideTabCallback.cancel();
                                                }
                                                HideTabCallback hideTabCallback =
                                                        new HideTabCallback(
                                                                () -> {
                                                                    Tab tab =
                                                                            mTabModelSelector
                                                                                    .getCurrentTab();
                                                                    if (currentTab == tab) {
                                                                        currentTab.hide(
                                                                                TabHidingType
                                                                                        .TAB_SWITCHER_SHOWN);
                                                                    }
                                                                    mHideTabCallback = null;
                                                                });
                                                mHideTabCallback = hideTabCallback;
                                                mTabContentManager.cacheTabThumbnailWithCallback(
                                                        currentTab,
                                                        /* returnBitmap= */ false,
                                                        (bitmap) -> {
                                                            hideTabCallback.run();
                                                        });
                                            } else {
                                                mTabContentManager.cacheTabThumbnail(currentTab);
                                            }
                                        }
                                        resetLayoutTabs();
                                        mFinishedShowingRunnable = null;
                                    };
                            mHandler.postDelayed(mFinishedShowingRunnable, ZOOMING_DURATION);
                        } else {
                            mFinishedShowingRunnable =
                                    () -> {
                                        if (ChromeFeatureList.sHideTabOnTabSwitcher.isEnabled()) {
                                            final Tab currentTab =
                                                    mTabModelSelector.getCurrentTab();
                                            if (currentTab != null) {
                                                RecordHistogram.recordBooleanHistogram(
                                                        "Android.TabSwitcher.TabHidden", true);
                                                currentTab.hide(TabHidingType.TAB_SWITCHER_SHOWN);
                                            }
                                        }
                                        resetLayoutTabs();
                                        mFinishedShowingRunnable = null;
                                    };
                            mHandler.postDelayed(mFinishedShowingRunnable, ZOOMING_DURATION);
                        }
                    }

                    @Override
                    public void startedHiding() {
                        clearFinishedShowingRunnable();
                    }

                    @Override
                    public void finishedHiding() {
                        // The Android View version of GTS overview is hidden.
                        // If not doing GTS-to-Tab transition animation, we show the fade-out
                        // instead, which was already done.
                        if (!TabUiFeatureUtilities.isTabToGtsAnimationEnabled(getContext())
                                || mBackToStartSurface) {
                            mBackToStartSurface = false;
                            postHiding();
                            return;
                        }
                        // If we are doing GTS-to-Tab transition animation, we start showing the
                        // Bitmap version of the GTS overview in the background while expanding the
                        // thumbnail to the viewport.
                        if (!ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()) {
                            expandTab(getThumbnailLocationOfCurrentTab());
                        }
                    }

                    private void resetLayoutTabs() {
                        // Clear the visible IDs. Once mLayoutTabs is empty, tabs will no longer be
                        // captureable and this prevents a thumbnailing request from waiting
                        // indefinitely.
                        updateCacheVisibleIds(Collections.emptyList());

                        // crbug.com/1176548, mLayoutTabs is used to capture thumbnail, null it in a
                        // post delay handler to avoid creating a new pending surface in native,
                        // which will hold the thumbnail capturing task.
                        mLayoutTabs = null;
                    }
                };

        mController.addTabSwitcherViewObserver(mTabSwitcherObserver);
    }

    @Override
    public void onFinishNativeInitialization() {
        if (mIsInitialized) return;

        mIsInitialized = true;
        mTabSwitcher.initWithNative();
        ensureSceneLayerCreated();
        if (mTabListSceneLayer != null) {
            mTabListSceneLayer.setTabModelSelector(mTabModelSelector);
        } else if (mTabSceneLayer != null) {
            mTabSceneLayer.setTabContentManager(mTabContentManager);
        }
    }

    // Layout implementation.
    @Override
    public void setTabModelSelector(TabModelSelector modelSelector) {
        super.setTabModelSelector(modelSelector);
        if (mTabListSceneLayer != null) {
            mTabListSceneLayer.setTabModelSelector(modelSelector);
        }
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
        mController.removeTabSwitcherViewObserver(mTabSwitcherObserver);
        if (mTabSceneLayer != null) {
            mTabSceneLayer.destroy();
            mTabSceneLayer = null;
        }
        if (mEmptySceneLayer != null) {
            mEmptySceneLayer.destroy();
            mEmptySceneLayer = null;
        }
        if (mTabListSceneLayer != null) {
            mTabListSceneLayer.destroy();
            mTabListSceneLayer = null;
        }
    }

    @Override
    public void show(long time, boolean animate) {
        try (TraceEvent e = TraceEvent.scoped(TRACE_SHOW_TAB_SWITCHER)) {
            // This is already in the process of showing (no-op).
            if (isStartingToShow()) return;

            mTransitionStartTime = SystemClock.elapsedRealtime();

            super.show(time, animate);

            // Prevent pending thumbnail captures from running if we start to show again very
            // quickly.
            clearFinishedShowingRunnable();
            forceAnimationToFinish();

            // Keep the current tab in mLayoutTabs even if we are not going to show the shrinking
            // animation so that thumbnail taking is not blocked.
            final Tab currentTab = mTabModelSelector.getCurrentTab();
            final int tabId = currentTab == null ? Tab.INVALID_TAB_ID : currentTab.getId();
            LayoutTab sourceLayoutTab =
                    createLayoutTab(tabId, mTabModelSelector.isIncognitoSelected());
            sourceLayoutTab.setDecorationAlpha(0);
            mLayoutTabs = new LayoutTab[] {sourceLayoutTab};
            mShowEmptyLayer = false;
            updateCacheVisibleIds(Collections.singletonList(tabId));

            // Skip animation when there is no tab in current tab model, we don't show the shrink
            // tab animation.
            boolean isCurrentTabModelEmpty = mTabModelSelector.getCurrentModel().getCount() == 0;
            final boolean shouldAnimate = animate && !isCurrentTabModelEmpty;

            final boolean isTablet =
                    DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext());
            final boolean skipAnimationForListMode = AccessibilityState.isTouchExplorationEnabled();
            ConditionalAnimationRunner conditionalAnimationRunner =
                    (isTablet || skipAnimationForListMode)
                    ? null
                    : createShrinkAnimationRunner(shouldAnimate);
            mConditionalAnimationRunnerRef = new WeakReference(conditionalAnimationRunner);

            // Cache the thumbnail now before the animation starts or any thumbnails are requested.
            if (currentTab != null
                    && ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()) {
                if (conditionalAnimationRunner != null) {
                    mTabContentManager.cacheTabThumbnailWithCallback(
                            currentTab, /*returnBitmap=*/true, (bitmap) -> {
                                if (bitmap != null) {
                                    conditionalAnimationRunner.setBitmap(bitmap);
                                    return;
                                }
                                // NativePages won't produce a new bitmap if nothing changed.
                                // Refetch from disk. For a normal tab we can't do this fallback as
                                // the thumbnail is possibly stale.
                                if (currentTab.isNativePage()) {
                                    mTabContentManager.getEtc1TabThumbnailWithCallback(
                                            currentTab.getId(), (etc1Bitmap) -> {
                                                conditionalAnimationRunner.setBitmap(etc1Bitmap);
                                            });
                                    return;
                                }
                                conditionalAnimationRunner.setBitmap(null);
                            });
                } else {
                    mTabContentManager.cacheTabThumbnail(currentTab);
                }
            }

            // Do this after requesting the thumbnail to ensure the correct thumbnail is shown.
            boolean tabListCanShowQuickly = mGridTabListDelegate.prepareTabSwitcherView();
            if (conditionalAnimationRunner != null) {
                conditionalAnimationRunner.setTabListCanShowQuickly(tabListCanShowQuickly);
            }

            if (isTablet) {
                showOverviewWithTranslateUp(shouldAnimate);
            } else if (skipAnimationForListMode) {
                // Intentionally disable the shrinking animation when touch exploration is enabled.
                // During the shrinking animation, since the ComponsitorViewHolder is not focusable,
                // Chrome is in a temporary no "valid" focus target state. This result in focus
                // shifting to the omnibox and triggers visual jank and accessibility announcement
                // of the URL. Disable the animation and run immediately to avoid this state.
                if (ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()) {
                    showOverviewWithTabShrinkJava(false, () -> null, true, null);
                } else {
                    showOverviewWithTabShrink(false, () -> null, true);
                }
            } else {
                assert conditionalAnimationRunner != null;
                mGridTabListDelegate.runAnimationOnNextLayout(
                        () -> {
                            conditionalAnimationRunner.setLayoutCompleted();

                            if (mHasPerfListenerForTesting) return;

                            // Layout should always wait for completion so post after that signal
                            // finishes.
                            mHandler.postDelayed(
                                    conditionalAnimationRunner::runAnimationDueToTimeout,
                                    TabUiFeatureUtilities.ANIMATION_START_TIMEOUT_MS.getValue());
                        });
            }
        }
    }

    private void showBrowserScrim() {
        if (mScrimCoordinator == null) return;

        PropertyModel.Builder scrimPropertiesBuilder =
                new PropertyModel.Builder(ScrimProperties.ALL_KEYS)
                        .with(ScrimProperties.ANCHOR_VIEW, mScrimAnchor)
                        .with(ScrimProperties.SHOW_IN_FRONT_OF_ANCHOR_VIEW, false);

        if (ChromeFeatureList.sTabStripRedesign.isEnabled()) {
            int scrimColor = ChromeColors.getPrimaryBackgroundColor(getContext(), isIncognito());
            scrimPropertiesBuilder.with(ScrimProperties.AFFECTS_STATUS_BAR, true)
                    .with(ScrimProperties.BACKGROUND_COLOR, scrimColor);
        }

        mScrimCoordinator.showScrim(scrimPropertiesBuilder.build());
    }

    private void hideBrowserScrim() {
        if (mScrimCoordinator == null || !mScrimCoordinator.isShowingScrim()) return;
        mScrimCoordinator.hideScrim(true, SCRIM_FADE_DURATION_MS);
    }

    @Override
    protected void updateLayout(long time, long dt) {
        ensureSceneLayerCreated();
        super.updateLayout(time, dt);
        if (mLayoutTabs == null) return;

        assert mLayoutTabs.length >= 1;
        boolean needUpdate = updateSnap(dt, mLayoutTabs[0]);
        if (needUpdate) requestUpdate();
    }

    @Override
    public void startHiding(int nextId, boolean hintAtTabSelection) {
        try (TraceEvent e = TraceEvent.scoped(TRACE_HIDE_TAB_SWITCHER)) {
            // This is already in the process of hiding. No-op.
            if (isStartingToHide()) return;

            mTransitionStartTime = SystemClock.elapsedRealtime();

            super.startHiding(nextId, hintAtTabSelection);

            // The new tab animation will handle the rest of the hide.
            if (mRunningNewTabAnimation) return;

            // If the hiding of TabSwitcherLayout is triggered by
            // {@link ChromeTabbedActivity#returnToOverviewModeOnBackPressed()} to return to the
            // Start surface, we set the flag here to skip GTS-to-Tab transition animation when
            // hiding.
            mBackToStartSurface =
                    mLayoutStateProvider.getNextLayoutType() == LayoutType.START_SURFACE;

            int sourceTabId = nextId;
            if (sourceTabId == Tab.INVALID_TAB_ID) {
                sourceTabId = mTabModelSelector.getCurrentTabId();
            }

            clearFinishedShowingRunnable();

            boolean isTablet = DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext());
            boolean tabGtsAnimationEnabled =
                    TabUiFeatureUtilities.isTabToGtsAnimationEnabled(getContext());
            boolean expandTabAnimationEnabled = tabGtsAnimationEnabled && !mBackToStartSurface;
            if (!ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()
                    || isTablet
                    || expandTabAnimationEnabled) {
                LayoutTab sourceLayoutTab =
                        createLayoutTab(sourceTabId, mTabModelSelector.isIncognitoSelected());
                sourceLayoutTab.setDecorationAlpha(0);

                List<LayoutTab> layoutTabs = new ArrayList<>();
                List<Integer> tabIds = new ArrayList<>();
                layoutTabs.add(sourceLayoutTab);
                tabIds.add(sourceLayoutTab.getId());

                if (sourceTabId != mTabModelSelector.getCurrentTabId()) {
                    // Keep the original tab in mLayoutTabs to unblock thumbnail taking at the end
                    // of the animation.
                    LayoutTab originalTab = createLayoutTab(mTabModelSelector.getCurrentTabId(),
                            mTabModelSelector.isIncognitoSelected());
                    originalTab.setScale(0);
                    originalTab.setDecorationAlpha(0);
                    layoutTabs.add(originalTab);
                    tabIds.add(originalTab.getId());
                }
                updateCacheVisibleIds(tabIds);
                mLayoutTabs = layoutTabs.toArray(new LayoutTab[0]);
                mShowEmptyLayer = false;
            } else {
                LayoutTab emptyLayoutTab = createLayoutTab(
                        Tab.INVALID_TAB_ID, mTabModelSelector.isIncognitoSelected());
                emptyLayoutTab.setDecorationAlpha(0);
                mLayoutTabs = new LayoutTab[] {emptyLayoutTab};
                mShowEmptyLayer = true;
            }

            mIsAnimatingHide = true;
            if (isTablet) {
                translateDown();
            } else {
                if (ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()) {
                    if (expandTabAnimationEnabled) {
                        mController.prepareHideTabSwitcherView();
                        expandTabJava(sourceTabId, getThumbnailLocationOfCurrentTab(),
                                mGridTabListDelegate.getThumbnailSize());
                    } else {
                        mController.hideTabSwitcherView(false);
                    }
                } else {
                    mController.hideTabSwitcherView(!tabGtsAnimationEnabled);
                }
            }
        }
    }

    @Override
    public void doneHiding() {
        try (TraceEvent e = TraceEvent.scoped(TRACE_DONE_HIDING_TAB_SWITCHER)) {
            super.doneHiding();
            RecordUserAction.record("MobileExitStackView");
        }
    }

    @Override
    public void doneShowing() {
        try (TraceEvent e = TraceEvent.scoped(TRACE_DONE_SHOWING_TAB_SWITCHER)) {
            if (!mAndroidViewFinishedShowing) return;
            mTabJavaView.setVisibility(View.GONE);
            super.doneShowing();
        }
    }

    @Override
    public boolean onBackPressed() {
        return mController.onBackPressed();
    }

    @Override
    protected EventFilter getEventFilter() {
        return null;
    }

    @Override
    protected SceneLayer getSceneLayer() {
        if (mTabSceneLayer != null) {
            return mShowEmptyLayer ? mEmptySceneLayer : mTabSceneLayer;
        }
        return mTabListSceneLayer;
    }

    private void ensureSceneLayerCreated() {
        if (mTabListSceneLayer != null || mTabSceneLayer != null) return;

        if (ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()) {
            mTabSceneLayer = new StaticTabSceneLayer();
            mEmptySceneLayer = new SolidColorSceneLayer();
        } else {
            mTabListSceneLayer = new TabListSceneLayer();
        }
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
    public void onTabCreated(long time, int id, int index, int sourceId, boolean newIsIncognito,
            boolean background, float originX, float originY) {
        super.onTabCreated(time, id, index, sourceId, newIsIncognito, background, originX, originY);

        // Skip the new tab animation for background tabs and on tablet.
        if (background
                || DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())
                || !ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()) {
            return;
        }

        clearFinishedShowingRunnable();
        forceAnimationToFinish();

        // If the dialog is visible or this isn't the active layout we shouldn't show the tab
        // creation animation.
        if (mController.isDialogVisible() || !isActive()) return;

        // The simple animation layout happens behind the TabListRecyclerView. Get the animation to
        // work by playing the animation on top.

        Rect fullscreenRect = mGridTabListDelegate.getRecyclerViewLocation();
        Rect containerRect = new Rect();
        mController.getTabSwitcherContainer().getGlobalVisibleRect(containerRect);
        final int fullscreenTop = fullscreenRect.top;
        final int topMargin = fullscreenTop - containerRect.top;
        fullscreenRect.set(fullscreenRect.left, 0, fullscreenRect.right, fullscreenRect.bottom);

        int x = Math.round(originX);
        int y = Math.round(originY);
        int offsetY = y + topMargin;
        Rect origin = new Rect(x, y, x + 1, y + 1);
        mTabJavaView.reset(new Rect(x, offsetY, x + 1, offsetY + 1));
        mTabJavaView.setImageBitmap(null);
        updateBackgroundColor(newIsIncognito);
        mTabJavaView.setVisibility(View.INVISIBLE);

        mShrinkExpandAnimator = new ShrinkExpandAnimator(mTabJavaView, origin, fullscreenRect);
        ObjectAnimator animator =
                ObjectAnimator.ofObject(
                        mShrinkExpandAnimator,
                        ShrinkExpandAnimator.RECT,
                        new RectEvaluator(),
                        origin,
                        fullscreenRect);
        animator.setDuration(FOREGROUND_DURATION_MS);
        animator.setInterpolator(Interpolators.STANDARD_INTERPOLATOR);

        mNewTabAnimation = new AnimatorSet();
        mNewTabAnimation.play(animator);
        mNewTabAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mNewTabAnimation = null;
                        postHiding();
                        mTabJavaView.reset(fullscreenRect);
                    }
                });

        mRunningNewTabAnimation = true;
        mShowEmptyLayer = true;
        mTabJavaView.invalidate();
        mTabJavaView.runOnNextLayout(
                () -> {
                    mTabJavaView.setVisibility(View.VISIBLE);
                    if (mNewTabAnimation != null) {
                        mNewTabAnimation.start();
                    }
                    mController.hideTabSwitcherView(true);
                });
    }

    @Override
    protected void forceAnimationToFinish() {
        super.forceAnimationToFinish();
        if (mConditionalAnimationRunnerRef != null
                && mConditionalAnimationRunnerRef.get() != null) {
            // Prevent re-entrancy.
            ConditionalAnimationRunner runner = mConditionalAnimationRunnerRef.get();
            mConditionalAnimationRunnerRef.clear();
            mConditionalAnimationRunnerRef = null;
            // If we are forcing the animation to finish treat this identically to a timeout.
            runner.runAnimationDueToTimeout();
        }
        mTabJavaView.runOnNextLayoutRunnables();
        if (mNewTabAnimation != null) {
            if (mNewTabAnimation.isStarted()) {
                mNewTabAnimation.end();
            }
            mNewTabAnimation = null;
        }
        if (mTabToSwitcherAnimation != null) {
            if (mTabToSwitcherAnimation.isStarted()) {
                mTabToSwitcherAnimation.end();
            }
            mTabToSwitcherAnimation = null;
        }
        mTabJavaView.setVisibility(View.GONE);
    }

    /**
     * Animate shrinking a tab to a target {@link Rect} area.
     * @param animate Whether to play an entry animation.
     * @param target The target {@link Rect} area.
     * @param quick Whether the tab list can be shown quickly.
     */
    private void showOverviewWithTabShrink(
            boolean animate, Supplier<Rect> target, boolean tabListCanShowQuickly) {
        // Skip shrinking animation when there is no tab in current tab model.
        boolean isCurrentTabModelEmpty = mTabModelSelector.getCurrentModel().getCount() == 0;
        boolean showShrinkingAnimation = animate
                && TabUiFeatureUtilities.isTabToGtsAnimationEnabled(getContext())
                && !isCurrentTabModelEmpty;

        boolean skipSlowZooming = TabUiFeatureUtilities.SKIP_SLOW_ZOOMING.getValue();
        Log.d(TAG, "SkipSlowZooming = " + skipSlowZooming);
        if (skipSlowZooming) {
            showShrinkingAnimation &= tabListCanShowQuickly;
        }

        final Rect targetRect = target.get();
        if (!showShrinkingAnimation || targetRect == null) {
            mController.showTabSwitcherView(animate);
            return;
        }

        forceAnimationToFinish();

        assert mLayoutTabs != null
                && mLayoutTabs.length > 0
            : "mLayoutTabs should have at least one entry during shrink animation.";
        LayoutTab sourceLayoutTab = mLayoutTabs[0];
        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // With the post start surface refactoring this offset overcompensates. Offset the reverse
        // for the rect.
        int tabListTopOffset = mGridTabListDelegate.getTabListTopOffset();
        targetRect.offset(0, -tabListTopOffset);

        // Step 1: zoom out the source tab
        Supplier<Float> scaleStartValueSupplier = () -> 1.0f;
        Supplier<Float> scaleEndValueSupplier = () -> targetRect.width() / (getWidth() * mDpToPx);

        Supplier<Float> xStartValueSupplier = () -> 0f;
        Supplier<Float> xEndValueSupplier = () -> targetRect.left / mDpToPx;

        Supplier<Float> yStartValueSupplier = () -> 0f;
        Supplier<Float> yEndValueSupplier = () -> targetRect.top / mDpToPx;

        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.SCALE, scaleStartValueSupplier, scaleEndValueSupplier, ZOOMING_DURATION,
                Interpolators.EMPHASIZED));
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.X, xStartValueSupplier, xEndValueSupplier, ZOOMING_DURATION,
                Interpolators.EMPHASIZED));
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.Y, yStartValueSupplier, yEndValueSupplier, ZOOMING_DURATION,
                Interpolators.EMPHASIZED));
        // TODO(crbug.com/964406): when shrinking to the bottom row, bottom of the tab goes up and
        // down, making the "create group" visible for a while.
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.MAX_CONTENT_HEIGHT, sourceLayoutTab.getUnclampedOriginalContentHeight(),
                Math.min(getWidth()
                                / TabUtils.getTabThumbnailAspectRatio(
                                        getContext(), mBrowserControlsStateProvider),
                        sourceLayoutTab.getUnclampedOriginalContentHeight()),
                ZOOMING_DURATION, Interpolators.FAST_OUT_SLOW_IN_INTERPOLATOR));

        CompositorAnimator backgroundAlpha =
                CompositorAnimator.ofFloat(handler, 0f, 1f, BACKGROUND_FADING_DURATION_MS,
                        animator -> mBackgroundAlpha = animator.getAnimatedValue());
        backgroundAlpha.setInterpolator(Interpolators.EMPHASIZED);
        animationList.add(backgroundAlpha);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.playTogether(animationList);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mAnimationTracker.onStart();
                mController.prepareShowTabSwitcherView();
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                // Step 2: fade in the real GTS RecyclerView.
                mController.showTabSwitcherView(true);

                mAnimationTracker.onEnd();
                mAnimationTransitionType = TransitionType.NONE;
            }
        });
        mAnimationTransitionType = TransitionType.SHRINK;
        mTabToSwitcherAnimation.start();
    }

    private void showOverviewWithTabShrinkJava(
            boolean animate, Supplier<Rect> target, boolean tabListCanShowQuickly, Bitmap bitmap) {
        // Skip shrinking animation when there is no tab in current tab model.
        boolean isCurrentTabModelEmpty = mTabModelSelector.getCurrentModel().getCount() == 0;
        boolean showShrinkingAnimation = animate
                && TabUiFeatureUtilities.isTabToGtsAnimationEnabled(getContext())
                && !isCurrentTabModelEmpty && bitmap != null;

        boolean skipSlowZooming = TabUiFeatureUtilities.SKIP_SLOW_ZOOMING.getValue();
        Log.d(TAG, "SkipSlowZooming = " + skipSlowZooming);
        if (skipSlowZooming) {
            showShrinkingAnimation &= tabListCanShowQuickly;
        }

        // Force any incomplete animations to finish regardless of whether a new shrink animation
        // will start.
        forceAnimationToFinish();

        final Rect targetRect = target.get();
        if (!showShrinkingAnimation || targetRect == null) {
            mController.showTabSwitcherView(animate);
            mTabJavaView.reset(targetRect);
            mTabJavaView.setVisibility(View.GONE);
            return;
        }

        Rect fullscreenRect = mGridTabListDelegate.getRecyclerViewLocation();
        Rect containerRect = new Rect();
        mController.getTabSwitcherContainer().getGlobalVisibleRect(containerRect);
        final int fullscreenTop = fullscreenRect.top;
        final int topMargin = fullscreenTop - containerRect.top;
        fullscreenRect.set(fullscreenRect.left, 0, fullscreenRect.right, fullscreenRect.bottom);

        mTabJavaView.reset(
                new Rect(
                        fullscreenRect.left,
                        topMargin,
                        fullscreenRect.right,
                        fullscreenRect.bottom + topMargin));
        updateBackgroundColor(isIncognito());
        mTabJavaView.setImageBitmap(bitmap);
        mTabJavaView.setVisibility(View.INVISIBLE);

        mShrinkExpandAnimator = new ShrinkExpandAnimator(mTabJavaView, fullscreenRect, targetRect);
        mShrinkExpandAnimator.setRect(fullscreenRect);
        ObjectAnimator animator =
                ObjectAnimator.ofObject(
                        mShrinkExpandAnimator,
                        ShrinkExpandAnimator.RECT,
                        new RectEvaluator(),
                        fullscreenRect,
                        targetRect);
        animator.addUpdateListener((valueAnimator) -> { mAnimationTracker.onUpdate(); });
        animator.setDuration(ZOOMING_DURATION);
        animator.setInterpolator(Interpolators.EMPHASIZED);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.play(animator);
        mTabToSwitcherAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationStart(Animator animation) {
                        // Skip the doneShowing() call in the TabSwitcherViewObserver as it should
                        // wait until this animation ends.
                        mSkipDoneShowingOnTabSwitcherFinishedShowing = true;
                        // Skip fade-in for tab switcher view, since it will appear behind the
                        // shrink.
                        mController.showTabSwitcherView(false);
                        mController.setSnackbarParentView(mController.getTabSwitcherContainer());
                    }

                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mTabToSwitcherAnimation = null;
                        doneShowing();
                        mSkipDoneShowingOnTabSwitcherFinishedShowing = false;
                        mTabJavaView.reset(targetRect);

                        mAnimationTracker.onEnd();
                        mAnimationTransitionType = TransitionType.NONE;
                    }
                });

        // The animation needs to be deferred so that the ShrinkExpandImageView has been laid out
        // before the animation starts otherwise it might jank.
        mAnimationTransitionType = TransitionType.SHRINK;
        mTabJavaView.invalidate();
        mTabJavaView.runOnNextLayout(
                () -> {
                    mTabJavaView.setVisibility(View.VISIBLE);
                    mAnimationTracker.onStart();
                    if (mTabToSwitcherAnimation != null) {
                        mTabToSwitcherAnimation.start();
                    }
                });
    }

    /**
     * Animate expanding a tab from a source {@link Rect} area.
     * @param source The source {@link Rect} area.
     */
    private void expandTabJava(int tabId, Rect source, Size thumbnailSize) {
        forceAnimationToFinish();

        Rect fullscreenRect = mGridTabListDelegate.getRecyclerViewLocation();
        Rect containerRect = new Rect();
        mController.getTabSwitcherContainer().getGlobalVisibleRect(containerRect);
        final int fullscreenTop = fullscreenRect.top;
        final int topMargin = fullscreenTop - containerRect.top;
        fullscreenRect.set(fullscreenRect.left, 0, fullscreenRect.right, fullscreenRect.bottom);

        mTabJavaView.reset(
                new Rect(
                        source.left,
                        source.top + topMargin,
                        source.right,
                        source.bottom + topMargin));
        mTabJavaView.setVisibility(View.INVISIBLE);

        mShrinkExpandAnimator = new ShrinkExpandAnimator(mTabJavaView, source, fullscreenRect);
        mShrinkExpandAnimator.setThumbnailSizeForOffset(thumbnailSize);

        ObjectAnimator animator =
                ObjectAnimator.ofObject(
                        mShrinkExpandAnimator,
                        ShrinkExpandAnimator.RECT,
                        new RectEvaluator(),
                        source,
                        fullscreenRect);
        animator.addUpdateListener((valueAnimator) -> { mAnimationTracker.onUpdate(); });
        animator.setDuration(ZOOMING_DURATION);
        animator.setInterpolator(Interpolators.EMPHASIZED);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.play(animator);
        mTabToSwitcherAnimation.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mTabToSwitcherAnimation = null;
                        postHiding();
                        mTabJavaView.reset(fullscreenRect);
                        mAnimationTracker.onEnd();
                        mAnimationTransitionType = TransitionType.NONE;
                    }
                });

        final ConditionalAnimationRunner conditionalAnimationRunner =
                new ConditionalAnimationRunner(
                        (bitmap, tabListCanShowQuickly) -> {
                            if (bitmap == null) {
                                mTabToSwitcherAnimation = null;
                                postHiding();
                                mTabJavaView.reset(fullscreenRect);
                                return;
                            }

                            updateBackgroundColor(isIncognito());
                            mTabJavaView.setImageBitmap(bitmap);
                            mShrinkExpandAnimator.setRect(source);
                            mTabJavaView.setVisibility(View.VISIBLE);

                            mAnimationTransitionType = TransitionType.EXPAND;
                            mTabJavaView.invalidate();
                            mTabJavaView.runOnNextLayout(
                                    () -> {
                                        mAnimationTracker.onStart();
                                        if (mTabToSwitcherAnimation != null) {
                                            mTabToSwitcherAnimation.start();
                                        }
                                    });
                        });
        // Quick and layout completed don't matter for expand, but set them so the animation will
        // trigger.
        conditionalAnimationRunner.setTabListCanShowQuickly(true);
        conditionalAnimationRunner.setLayoutCompleted();

        mConditionalAnimationRunnerRef = new WeakReference(conditionalAnimationRunner);

        mTabContentManager.getEtc1TabThumbnailWithCallback(
                tabId, (bitmap) -> { conditionalAnimationRunner.setBitmap(bitmap); });
        if (mHasPerfListenerForTesting) return;
        mHandler.postDelayed(
                conditionalAnimationRunner::runAnimationDueToTimeout,
                TabUiFeatureUtilities.ANIMATION_START_TIMEOUT_MS.getValue());
    }

    /**
     * Animate expanding a tab from a source {@link Rect} area.
     * @param source The source {@link Rect} area.
     */
    private void expandTab(Rect source) {
        assert mLayoutTabs != null
                && mLayoutTabs.length > 0
            : "mLayoutTabs should have at least one entry during expand animation.";
        LayoutTab sourceLayoutTab = mLayoutTabs[0];

        forceAnimationToFinish();
        CompositorAnimationHandler handler = getAnimationHandler();
        Collection<Animator> animationList = new ArrayList<>(5);

        // With the post start surface refactoring this offset overcompensates. Offset the reverse
        // for the rect.
        int tabListTopOffset = mGridTabListDelegate.getTabListTopOffset();
        source.offset(0, -tabListTopOffset);

        // Zoom in the source tab
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.SCALE, source.width() / (getWidth() * mDpToPx), 1, ZOOMING_DURATION,
                Interpolators.EMPHASIZED));
        animationList.add(
                CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab, LayoutTab.X,
                        source.left / mDpToPx, 0f, ZOOMING_DURATION, Interpolators.EMPHASIZED));
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.Y, source.top / mDpToPx, 0f, ZOOMING_DURATION, Interpolators.EMPHASIZED));
        // TODO(crbug.com/964406): when shrinking to the bottom row, bottom of the tab goes up and
        // down, making the "create group" visible for a while.
        animationList.add(CompositorAnimator.ofWritableFloatPropertyKey(handler, sourceLayoutTab,
                LayoutTab.MAX_CONTENT_HEIGHT,
                Math.min(getWidth()
                                / TabUtils.getTabThumbnailAspectRatio(
                                        getContext(), mBrowserControlsStateProvider),
                        sourceLayoutTab.getUnclampedOriginalContentHeight()),
                sourceLayoutTab.getUnclampedOriginalContentHeight(), ZOOMING_DURATION,
                Interpolators.EMPHASIZED));

        CompositorAnimator backgroundAlpha =
                CompositorAnimator.ofFloat(handler, 1f, 0f, BACKGROUND_FADING_DURATION_MS,
                        animator -> mBackgroundAlpha = animator.getAnimatedValue());
        backgroundAlpha.setInterpolator(Interpolators.EMPHASIZED);
        animationList.add(backgroundAlpha);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.playTogether(animationList);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mAnimationTracker.onStart();
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                postHiding();

                mAnimationTracker.onEnd();
                mAnimationTransitionType = TransitionType.NONE;
            }
        });
        mAnimationTransitionType = TransitionType.EXPAND;
        mTabToSwitcherAnimation.start();
    }

    /**
     * Animate translating grid tab switcher and its toolbar up.
     */
    private void showOverviewWithTranslateUp(boolean animate) {
        forceAnimationToFinish();
        showBrowserScrim();

        Animator translateUp = ObjectAnimator.ofFloat(mController.getTabSwitcherContainer(),
                View.TRANSLATION_Y, mController.getTabSwitcherContainer().getHeight(), 0f);
        translateUp.setInterpolator(Interpolators.EMPHASIZED_DECELERATE);
        translateUp.setDuration(TRANSLATE_DURATION_MS);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.play(translateUp);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                // Skip fade-in for tab switcher view, since it will translate in instead.
                mController.getTabSwitcherContainer().setVisibility(View.VISIBLE);
                mController.showTabSwitcherView(animate);
                mController.setSnackbarParentView(mController.getTabSwitcherContainer());
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;
                mController.getTabSwitcherContainer().setY(0);
                doneShowing();

                reportTabletAnimationPerf(true);
            }
        });
        mTabToSwitcherAnimation.start();
    }

    /**
     * Animate translating grid tab switcher and its toolbar down off-screen.
     */
    private void translateDown() {
        forceAnimationToFinish();
        hideBrowserScrim();

        Animator translateDown = ObjectAnimator.ofFloat(mController.getTabSwitcherContainer(),
                View.TRANSLATION_Y, 0f, mController.getTabSwitcherContainer().getHeight());
        translateDown.setInterpolator(Interpolators.EMPHASIZED_ACCELERATE);
        translateDown.setDuration(TRANSLATE_DURATION_MS);

        mTabToSwitcherAnimation = new AnimatorSet();
        mTabToSwitcherAnimation.play(translateDown);
        mTabToSwitcherAnimation.addListener(new AnimatorListenerAdapter() {
            @Override
            public void onAnimationStart(Animator animation) {
                mController.prepareHideTabSwitcherView();
                mController.setSnackbarParentView(null);
            }

            @Override
            public void onAnimationEnd(Animator animation) {
                mTabToSwitcherAnimation = null;

                // Skip fade-out  for tab switcher view, since it will translate out instead.
                mController.hideTabSwitcherView(false);
                mController.getTabSwitcherContainer().setVisibility(View.GONE);

                reportTabletAnimationPerf(false);
            }
        });
        mTabToSwitcherAnimation.start();
    }

    private Rect getThumbnailLocationOfCurrentTab() {
        return mGridTabListDelegate.getThumbnailLocationOfCurrentTab();
    }

    private TabListDelegate getGridTabListDelegate() {
        return mGridTabListDelegate;
    }

    private void postHiding() {
        mGridTabListDelegate.postHiding();
        mShowEmptyLayer = true;
        mIsAnimatingHide = false;
        mRunningNewTabAnimation = false;
        mTabJavaView.setVisibility(View.GONE);
        doneHiding();
    }

    public void removePerfListenerForTesting(AnimationPerformanceTracker.Listener perfListener) {
        mAnimationTracker.removeListener(perfListener);
        mHasPerfListenerForTesting = false;
    }

    public void addPerfListenerForTesting(AnimationPerformanceTracker.Listener perfListener) {
        mAnimationTracker.addListener(perfListener);
        mHasPerfListenerForTesting = true;
        ResettersForTesting.register(() -> { removePerfListenerForTesting(perfListener); });
    }

    public TabSwitcher getTabSwitcherForTesting() {
        return mTabSwitcher;
    }

    private static String transitionTypeToString(@TransitionType int transitionType) {
        switch (transitionType) {
            case TransitionType.SHRINK:
                return ".Shrink";
            case TransitionType.EXPAND:
                return ".Expand";
            case TransitionType.NONE:
                assert false : "TransitionType should not be none for string conversion.";
        }
        return "";
    }

    /**
     * Reports the animation performance for animating the {@link TabSwitcherLayout} into or out of
     * view. Exposed to be shared with {@link TabSwitcherAndStartSurfaceLayout}.
     * @param metrics the {@link AnimationPerformanceTracker.AnimationMetrics} for the animation.
     * @param animationTransitionType the type of transition to report metrics for.
     */
    public static void reportAnimationPerf(AnimationPerformanceTracker.AnimationMetrics metrics,
            long transitionStartTime, @TransitionType int animationTransitionType) {
        if (metrics.getFrameCount() == 0) return;

        final float fps = metrics.getFramesPerSecond();
        final long totalDurationMs = metrics.getLastFrameTimeMs() - transitionStartTime;

        // TODO(crbug.com/964406): stop logging it after this feature stabilizes.
        if (!VersionInfo.isStableBuild()) {
            String message = String.format(Locale.US,
                    "fps = %.2f (%d / %dms), maxFrameInterval = %d", fps, metrics.getFrameCount(),
                    metrics.getElapsedTimeMs(), metrics.getMaxFrameIntervalMs());
            Log.i(TAG, message);
        }

        String suffix = transitionTypeToString(animationTransitionType);

        // TODO(crbug.com/982018): Separate histograms for carousel tab switcher.
        RecordHistogram.recordCount100Histogram(
                "GridTabSwitcher.FramePerSecond" + suffix, (int) fps);
        RecordHistogram.recordTimesHistogram(
                "GridTabSwitcher.MaxFrameInterval" + suffix, metrics.getMaxFrameIntervalMs());
        RecordHistogram.recordTimesHistogram(
                "Android.GridTabSwitcher.Animation.TotalDuration" + suffix, totalDurationMs);
        RecordHistogram.recordTimesHistogram(
                "Android.GridTabSwitcher.Animation.FirstFrameLatency" + suffix,
                metrics.getFirstFrameLatencyMs());
    }

    private void reportTabletAnimationPerf(boolean translatingUp) {
        // TODO(crbug.com/1304926): Record metrics for tablet animations.
    }

    @Override
    protected void updateSceneLayer(RectF viewport, RectF contentViewport,
            TabContentManager tabContentManager, ResourceManager resourceManager,
            BrowserControlsStateProvider browserControls) {
        ensureSceneLayerCreated();
        super.updateSceneLayer(
                viewport, contentViewport, tabContentManager, resourceManager, browserControls);
        if (mTabSceneLayer != null) {
            if (mShowEmptyLayer) return;

            assert mLayoutTabs != null && mLayoutTabs.length > 0;
            mLayoutTabs[0].set(LayoutTab.IS_ACTIVE_LAYOUT_SUPPLIER, this::isActive);
            mLayoutTabs[0].set(LayoutTab.CONTENT_OFFSET, browserControls.getContentOffset());
            mTabSceneLayer.update(mLayoutTabs[0]);
            return;
        }

        assert mTabListSceneLayer != null;

        // The content viewport is intentionally sent as both params below.
        mTabListSceneLayer.pushLayers(getContext(), contentViewport, contentViewport, this,
                tabContentManager, resourceManager, browserControls,
                TabUiFeatureUtilities.isTabToGtsAnimationEnabled(getContext())
                        ? mGridTabListDelegate.getResourceId()
                        : 0,
                mBackgroundAlpha, 0);

        if (mAnimationTransitionType != TransitionType.NONE) {
            mAnimationTracker.onUpdate();
        }
    }

    @Override
    public int getLayoutType() {
        return LayoutType.TAB_SWITCHER;
    }

    @Override
    public boolean onUpdateAnimation(long time, boolean jumpToEnd) {
        return mTabToSwitcherAnimation == null && !mIsAnimatingHide && !mRunningNewTabAnimation;
    }

    @Override
    public boolean canHostBeFocusable() {
        if (ChromeAccessibilityUtil.get().isAccessibilityEnabled()
                && !DeviceFormFactor.isNonMultiDisplayContextOnTablet(getContext())) {
            // We don't allow this layout to gain focus when accessibility is enabled so that the
            // CompositorViewHolder doesn't steal focus when entering tab switcher.
            // (crbug.com/1125185).
            // We ignore this logic on tablets, since it would cause focus to briefly shift to the
            // omnibox while entering the tab switcher. This was most notable on the NTP, where the
            // virtual keyboard would quickly appear then disappear. (https://crbug.com/1320035).
            return false;
        }
        return super.canHostBeFocusable();
    }

    @Override
    public boolean isRunningAnimations() {
        return (mConditionalAnimationRunnerRef != null
                       && mConditionalAnimationRunnerRef.get() != null)
                || mTabToSwitcherAnimation != null;
    }

    private void clearFinishedShowingRunnable() {
        if (mFinishedShowingRunnable != null) {
            mHandler.removeCallbacks(mFinishedShowingRunnable);
            mFinishedShowingRunnable = null;
        }
        if (mHideTabCallback != null) {
            mHideTabCallback.cancel();
            mHideTabCallback = null;
        }
    }

    private void updateBackgroundColor(boolean isIncognito) {
        @ColorInt
        int backgroundColor = ChromeColors.getPrimaryBackgroundColor(getContext(), isIncognito);
        mTabJavaView.setBackgroundColor(backgroundColor);
        mEmptySceneLayer.setBackgroundColor(backgroundColor);
    }

    private ConditionalAnimationRunner createShrinkAnimationRunner(boolean shouldAnimate) {
        if (ChromeFeatureList.sGridTabSwitcherAndroidAnimations.isEnabled()) {
            return new ConditionalAnimationRunner((bitmap, tabListCanShowQuickly) -> {
                showOverviewWithTabShrinkJava(shouldAnimate, () -> {
                    return mGridTabListDelegate.getThumbnailLocationOfCurrentTab();
                }, tabListCanShowQuickly, bitmap);
            });
        }

        ConditionalAnimationRunner conditionalAnimationRunner =
                new ConditionalAnimationRunner((bitmap, quick) -> {
                    showOverviewWithTabShrink(shouldAnimate, () -> {
                        return mGridTabListDelegate.getThumbnailLocationOfCurrentTab();
                    }, quick);
                });
        // Set the bitmap to null so that the animation proceeds immediately. When
        // sGridTabSwitcherAndroidAnimations is disabled compositor based animations are used where
        // the LayoutTabs of the SceneLayer will load the bitmap through native.
        conditionalAnimationRunner.setBitmap(null);
        return conditionalAnimationRunner;
    }
}
