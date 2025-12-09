// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;

import android.animation.Animator;
import android.animation.AnimatorListenerAdapter;
import android.animation.ValueAnimator;
import android.app.Activity;
import android.view.View;

import androidx.annotation.ColorInt;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.ObserverList;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNullIf;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.NullUnmarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.ActivityUtils;
import org.chromium.chrome.browser.browser_controls.BrowserControlsOffsetTagsInfo;
import org.chromium.chrome.browser.browser_controls.BrowserControlsSizer;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.browser_controls.BrowserStateBrowserControlsVisibilityDelegate;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.tab.SadTab;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.chrome.browser.tab.TabBrowserControlsOffsetHelper;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabObserver;
import org.chromium.chrome.browser.toolbar.ControlContainer;
import org.chromium.components.browser_ui.util.BrowserControlsVisibilityDelegate;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.BrowserControlsOffsetTagConstraints;
import org.chromium.ui.BrowserControlsOffsetTagDefinitions;
import org.chromium.ui.OffsetTagConstraints;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.util.TokenHolder;

/** A class that manages browser control visibility and positioning. */
@NullMarked
public class BrowserControlsManager implements ActivityStateListener, BrowserControlsSizer {
    // The amount of time to delay the control show request after returning to a once visible
    // activity.  This delay is meant to allow Android to run its Activity focusing animation and
    // have the controls scroll back in smoothly once that has finished.
    private static final long ACTIVITY_RETURN_SHOW_REQUEST_DELAY_MS = 100;

    /**
     * Maximum duration for the control container slide-in animation and the duration for the
     * browser controls height change animation. Note that this value matches the one in
     * browser_controls_offset_manager.cc.
     */
    private static final int CONTROLS_ANIMATION_DURATION_MS = 200;

    private final Activity mActivity;
    private final BrowserStateBrowserControlsVisibilityDelegate mBrowserVisibilityDelegate;
    @ControlsPosition private int mControlsPosition;
    private final TokenHolder mHidingTokenHolder = new TokenHolder(this::scheduleVisibilityUpdate);

    /**
     * An observable for browser controls being at its minimum height or not. This is as good as the
     * controls being hidden when both min heights are 0.
     */
    private final ObservableSupplierImpl<Boolean> mControlsAtMinHeight =
            new ObservableSupplierImpl<>();

    private @Nullable TabModelSelectorTabObserver mTabControlsObserver;
    private @Nullable ControlContainer mControlContainer;
    private int mTopControlsHeight;
    private int mTopControlsMinHeight;
    private int mBottomControlsHeight;
    private int mBottomControlsMinHeight;
    private int mBottomControlsAdditionalHeight;
    private boolean mAnimateBrowserControlsHeightChanges;

    private int mRendererTopControlOffset;
    private int mRendererBottomControlOffset;
    private int mRendererTopContentOffset;
    private int mRendererTopControlsMinHeightOffset;
    private int mRendererBottomControlsMinHeightOffset;
    private boolean mRendererTopControlsMinHeightChanged;
    private boolean mRendererBottomControlsMinHeightChanged;

    private float mControlOffsetRatio;
    private @Nullable ActivityTabTabObserver mActiveTabObserver;

    private final ObserverList<BrowserControlsStateProvider.Observer> mControlsObservers =
            new ObserverList<>();
    private final FullscreenHtmlApiHandlerBase mHtmlApiHandler;
    private @Nullable Tab mTab;

    /** The animator for the Android browser controls. */
    private @Nullable ValueAnimator mControlsAnimator;

    /**
     * Indicates if control offset is in the overridden state by animation. Stays {@code true} from
     * animation start till the next offset update from compositor arrives.
     */
    private boolean mOffsetOverridden;

    private boolean mContentViewScrolling;

    private boolean mForceRelayoutOnVisibilityChange;

    private BrowserControlsOffsetTagDefinitions mOffsetTagDefinitions =
            new BrowserControlsOffsetTagDefinitions();

    // These are the renderer offsets of the controls just prior to the first frame of an animation,
    // before height changes are applied. During the animation, all offsets sent from the renderer
    // will be referencing the new height. The last frame of the animation occurs when the browser
    // receives an offset from the renderer that is equal to the initial offset.
    private int mTopAnimationInitialOffset;
    private int mBottomAnimationInitialOffset;
    private boolean mHasTopControlsHeightAnimation;
    private boolean mHasBottomControlsHeightAnimation;

    private boolean mDisableSyncMinHeightWithTotalHeight;

    private final Runnable mUpdateVisibilityRunnable =
            new Runnable() {
                @Override
                public void run() {
                    int visibility = shouldShowAndroidControls() ? View.VISIBLE : View.INVISIBLE;
                    if (mControlContainer == null
                            || mControlContainer.getView().getVisibility() == visibility) {
                        return;
                    } else if (visibility == View.VISIBLE
                            && mContentViewScrolling
                            && assumeNonNull(mBrowserVisibilityDelegate.get())
                                    == BrowserControlsState.BOTH) {
                        // TODO(crbug.com/430320400): mBrowserVisibilityDelegate.get() never
                        // returns null, but ObservableSupplier.get() returns @Nullable.

                        // Don't make the controls visible until scrolling has stopped to avoid
                        // doing it more often than we need to. onContentViewScrollingStateChanged
                        // will schedule us again when scrolling ceases.
                        return;
                    }

                    try (TraceEvent e =
                            TraceEvent.scoped(
                                    "BrowserControlsManager.onAndroidVisibilityChanged")) {
                        mControlContainer.getView().setVisibility(visibility);
                        for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
                            obs.onAndroidControlsVisibilityChanged(visibility);
                        }
                        if (mForceRelayoutOnVisibilityChange && shouldShowAndroidControls()) {
                            // requestLayout is required to trigger a new gatherTransparentRegion(),
                            // which only occurs together with a layout and let's SurfaceFlinger
                            // trim overlays.
                            // This may be almost equivalent to using View.GONE, but we still use
                            // View.INVISIBLE since drawing caches etc. won't be destroyed, and the
                            // layout may be less expensive. The overlay trimming optimization
                            // only works pre-Android N (see https://crbug.com/725453), so this
                            // call should be removed entirely once it's confirmed to be safe.
                            ViewUtils.requestLayout(
                                    mControlContainer.getView(),
                                    "BrowserControlsManager.mUpdateVisibilityRunnable Runnable");
                            mForceRelayoutOnVisibilityChange = false;
                        }
                    }
                }
            };

    /**
     * Creates an instance of the browser controls manager.
     *
     * @param activity The activity that supports browser controls.
     * @param controlsPosition Where the browser controls are.
     * @param multiWindowDispatcher The multi-window mode observer for exiting fullscreen when the
     *     user drags the window out of edge-to-edge fullscreen
     */
    public BrowserControlsManager(
            Activity activity,
            @ControlsPosition int controlsPosition,
            MultiWindowModeStateDispatcher multiWindowDispatcher) {
        this(activity, controlsPosition, true, multiWindowDispatcher);
    }

    /**
     * Creates an instance of the browser controls manager.
     *
     * @param activity The activity that supports browser controls.
     * @param controlsPosition Where the browser controls are.
     * @param exitFullscreenOnStop Whether fullscreen mode should exit on stop - should be true for
     *     Activities that are not always fullscreen.
     * @param multiWindowDispatcher The multi-window mode observer for exiting fullscreen when the
     *     user drags the window out of edge-to-edge fullscreen
     */
    public BrowserControlsManager(
            Activity activity,
            @ControlsPosition int controlsPosition,
            boolean exitFullscreenOnStop,
            MultiWindowModeStateDispatcher multiWindowDispatcher) {
        mActivity = activity;
        mControlsPosition = controlsPosition;
        mControlsAtMinHeight.set(false);
        mHtmlApiHandler =
                FullscreenHtmlApiHandlerFactory.createInstance(
                        activity,
                        mControlsAtMinHeight,
                        exitFullscreenOnStop,
                        multiWindowDispatcher);
        mBrowserVisibilityDelegate =
                new BrowserStateBrowserControlsVisibilityDelegate(
                        mHtmlApiHandler.getPersistentFullscreenModeSupplier());
        mBrowserVisibilityDelegate.addObserver(
                (constraints) -> {
                    if (constraints == BrowserControlsState.SHOWN) {
                        // When compositor can drive the animation to show controls, do not call
                        // setPositionsForTabToNonFullscreen to avoid control offset being forced
                        // set to 0 before the render-driven animation kicks in.
                        boolean allowRenderDrivenShowConstraint =
                                ChromeFeatureList.sBrowserControlsRenderDrivenShowConstraint
                                        .isEnabled();
                        boolean renderDrivenShowConstraint =
                                allowRenderDrivenShowConstraint
                                        && canAnimateNativeBrowserControls();
                        if (!renderDrivenShowConstraint) {
                            setPositionsForTabToNonFullscreen();
                        }

                        // TODO(https://crbug.com/449011189): Maybe cleanup
                        if (allowRenderDrivenShowConstraint) {
                            RecordHistogram.recordBooleanHistogram(
                                    "Android.BrowserControls.RenderDrivenShowConstraint",
                                    renderDrivenShowConstraint);
                        }

                        // If controls become locked, it's possible we've previously delayed
                        // actually setting visibility until a touch event is over. In this case, we
                        // need to trigger an update again now, which should go through due to
                        // constraints.
                        scheduleVisibilityUpdate();
                    }

                    // From https://crbug.com/452885338, https://crbug.com/461532432: When changing
                    // controls visibility when exiting fullscreen, the visibility change might not
                    // honor a redraw. We do this through forcing a relayout to avoid the toolbar
                    // remains hidden.
                    if ((constraints == BrowserControlsState.SHOWN
                                    || constraints == BrowserControlsState.BOTH)
                            && getAndroidControlsVisibility() != View.VISIBLE) {
                        mForceRelayoutOnVisibilityChange = true;
                        scheduleVisibilityUpdate();
                    }
                });
    }

    /**
     * Initializes the browser controls manager with the required dependencies.
     *
     * @param controlContainer Container holding the controls (Toolbar).
     * @param activityTabProvider Provider of the current activity tab.
     * @param modelSelector The tab model selector that will be monitored for tab changes.
     * @param resControlContainerHeight The dimension resource ID for the control container height.
     */
    public void initialize(
            @Nullable ControlContainer controlContainer,
            ActivityTabProvider activityTabProvider,
            final TabModelSelector modelSelector,
            int resControlContainerHeight) {
        mHtmlApiHandler.initialize(activityTabProvider, modelSelector);
        ApplicationStatus.registerStateListenerForActivity(this, mActivity);
        mActiveTabObserver =
                new ActivityTabTabObserver(activityTabProvider) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab, boolean hint) {
                        setTab(tab);

                        // The tab that's been switched away from is never going to update us that
                        // the scroll event stopped.
                        assumeNonNull(mTabControlsObserver);
                        mTabControlsObserver.onContentViewScrollingStateChanged(false);
                    }
                };

        mTabControlsObserver =
                new TabModelSelectorTabObserver(modelSelector) {
                    @Override
                    public void onInteractabilityChanged(Tab tab, boolean interactable) {
                        if (!interactable || tab != getTab()) return;
                        TabBrowserControlsOffsetHelper helper =
                                TabBrowserControlsOffsetHelper.get(tab);
                        if (!helper.offsetInitialized()) return;

                        onOffsetsChanged(
                                helper.topControlsOffset(),
                                helper.bottomControlsOffset(),
                                helper.contentOffset(),
                                helper.topControlsMinHeightOffset(),
                                helper.bottomControlsMinHeightOffset());
                    }

                    @Override
                    public void onContentChanged(Tab tab) {
                        if (tab.isShowingCustomView()) {
                            showAndroidControls(false);
                        }
                    }

                    @Override
                    public void onRendererResponsiveStateChanged(Tab tab, boolean isResponsive) {
                        if (tab == getTab() && !isResponsive) showAndroidControls(false);
                    }

                    @Override
                    public void onBrowserControlsOffsetChanged(
                            Tab tab,
                            int topControlsOffset,
                            int bottomControlsOffset,
                            int contentOffset,
                            int topControlsMinHeightOffset,
                            int bottomControlsMinHeightOffset) {
                        if (tab == getTab() && tab.isUserInteractable() && !tab.isNativePage()) {
                            onOffsetsChanged(
                                    topControlsOffset,
                                    bottomControlsOffset,
                                    contentOffset,
                                    topControlsMinHeightOffset,
                                    bottomControlsMinHeightOffset);
                        }
                    }

                    @Override
                    public void onOffsetTagsInfoChanged(
                            Tab tab,
                            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
                            BrowserControlsOffsetTagsInfo offsetTagsInfo,
                            @BrowserControlsState int constraints) {
                        int hairlineHeight = getTopControlsHairlineHeight();
                        offsetTagsInfo.mTopControlsAdditionalHeight = hairlineHeight;
                        offsetTagsInfo.mContentConstraints =
                                new OffsetTagConstraints(0, 0, -mTopControlsHeight, 0);
                        offsetTagsInfo.mTopControlsConstraints =
                                new OffsetTagConstraints(
                                        0, 0, -(mTopControlsHeight + hairlineHeight), 0);

                        // Notify observers of changes before passing tags to native so observers
                        // can set their relevant fields in offsetTagsInfo.
                        notifyConstraintsChanged(oldOffsetTagsInfo, offsetTagsInfo, constraints);

                        offsetTagsInfo
                                .getConstraints()
                                .assertAndFixConstraints(
                                        "BrowserControlsManager constraints changed ");
                        updateOffsetTagDefinitions(
                                new BrowserControlsOffsetTagDefinitions(
                                        offsetTagsInfo.getTags(), offsetTagsInfo.getConstraints()));
                    }

                    @Override
                    public void onContentViewScrollingStateChanged(boolean scrolling) {
                        mContentViewScrolling = scrolling;
                        if (!scrolling
                                && shouldShowAndroidControls()
                                && mControlContainer.getView().getVisibility() != View.VISIBLE) {
                            scheduleVisibilityUpdate();
                        }
                    }
                };
        assert controlContainer != null || mControlsPosition == ControlsPosition.NONE;
        mControlContainer = controlContainer;
        int controlContainerHeight =
                mActivity.getResources().getDimensionPixelSize(resControlContainerHeight);

        switch (mControlsPosition) {
            case ControlsPosition.TOP:
                assert resControlContainerHeight != ActivityUtils.NO_RESOURCE_ID;
                mTopControlsHeight = controlContainerHeight;
                break;
            case ControlsPosition.BOTTOM:
                assert resControlContainerHeight != ActivityUtils.NO_RESOURCE_ID;
                mBottomControlsHeight = controlContainerHeight;
                break;
            case ControlsPosition.NONE:
                // Treat the case of no controls as controls always being totally offscreen.
                mControlOffsetRatio = 1.0f;
                break;
        }

        if (doSyncMinHeightWithTotalHeight()) {
            mTopControlsMinHeight = mTopControlsHeight;
        }

        mRendererTopContentOffset = mTopControlsHeight;
        updateControlOffset();
        scheduleVisibilityUpdate();
    }

    /**
     * @return {@link FullscreenManager} object.
     */
    public FullscreenManager getFullscreenManager() {
        return mHtmlApiHandler;
    }

    @Override
    public BrowserStateBrowserControlsVisibilityDelegate getBrowserVisibilityDelegate() {
        return mBrowserVisibilityDelegate;
    }

    /**
     * @return The currently selected tab for fullscreen.
     */
    @VisibleForTesting
    public @Nullable Tab getTab() {
        return mTab;
    }

    private void setTab(@Nullable Tab tab) {
        Tab previousTab = getTab();
        mTab = tab;
        if (previousTab != tab) {
            if (tab != null) {
                mBrowserVisibilityDelegate.showControlsTransient();
                if (tab.isUserInteractable()) restoreControlsPositions();
            }
        }

        if (tab == null
                && assumeNonNull(mBrowserVisibilityDelegate.get()) != BrowserControlsState.HIDDEN) {
            setPositionsForTabToNonFullscreen();
        }
    }

    // ActivityStateListener

    @Override
    public void onActivityStateChange(Activity activity, int newState) {
        if (newState == ActivityState.STARTED) {
            PostTask.postDelayedTask(
                    TaskTraits.UI_DEFAULT,
                    mBrowserVisibilityDelegate::showControlsTransient,
                    ACTIVITY_RETURN_SHOW_REQUEST_DELAY_MS);
        } else if (newState == ActivityState.DESTROYED) {
            ApplicationStatus.unregisterActivityStateListener(this);
        }
    }

    @Override
    public float getBrowserControlHiddenRatio() {
        return mControlOffsetRatio;
    }

    /**
     * @return True if the browser controls are showing as much as the min height. Note that this is
     *     the same as {@link
     *     BrowserControlsUtils#areBrowserControlsOffScreen(BrowserControlsStateProvider)} when both
     *     min-heights are 0.
     */
    @VisibleForTesting
    public boolean areBrowserControlsAtMinHeight() {
        return assertNonNull(mControlsAtMinHeight.get());
    }

    private void bottomControlsAnimationMaybeStarted(
            int oldHeight, int oldMinHeight, int newHeight, int newMinHeight) {
        mHasBottomControlsHeightAnimation = shouldAnimateBrowserControlsHeightChanges();
        if (mHasBottomControlsHeightAnimation) {
            mBottomAnimationInitialOffset = getBottomControlOffset();
        }
        if (ChromeFeatureList.sBcivBottomControls.isEnabled() && !isVisibilityForced()) {
            updateBottomControlsOffsetTagConstraints(
                    oldHeight, oldMinHeight, newHeight, newMinHeight);
        }
    }

    private void bottomControlsAnimationEnded() {
        mHasBottomControlsHeightAnimation = false;
        if (ChromeFeatureList.sBcivBottomControls.isEnabled() && !isVisibilityForced()) {
            updateBottomControlsOffsetTagConstraints(
                    mBottomControlsHeight,
                    mBottomControlsMinHeight,
                    mBottomControlsHeight,
                    mBottomControlsMinHeight);
        }
    }

    @Override
    public void setBottomControlsHeight(int bottomControlsHeight, int bottomControlsMinHeight) {
        if (mBottomControlsHeight == bottomControlsHeight
                && mBottomControlsMinHeight == bottomControlsMinHeight) {
            return;
        }
        try (TraceEvent e = TraceEvent.scoped("BrowserControlsManager.setBottomControlsHeight")) {
            final int oldBottomControlsHeight = mBottomControlsHeight;
            final int oldBottomControlsMinHeight = mBottomControlsMinHeight;
            mBottomControlsHeight = bottomControlsHeight;
            mBottomControlsMinHeight = bottomControlsMinHeight;

            if (!canAnimateNativeBrowserControls()) {
                if (shouldAnimateBrowserControlsHeightChanges()) {
                    runBrowserDrivenBottomControlsHeightChangeAnimation(
                            oldBottomControlsHeight, oldBottomControlsMinHeight);
                } else {
                    updateBrowserControlsOffsets(
                            /* toNonFullscreen= */ false,
                            0,
                            0,
                            getTopControlsHeight(),
                            getTopControlsMinHeight(),
                            getBottomControlsMinHeight());
                }
            }

            // Signal that the animation started even if canAnimateNativeBrowserControls() returns
            // false in the previous block, in case the browser driven animation changes to a
            // renderer driven animation.
            bottomControlsAnimationMaybeStarted(
                    oldBottomControlsHeight,
                    oldBottomControlsMinHeight,
                    bottomControlsHeight,
                    bottomControlsMinHeight);

            for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
                obs.onBottomControlsHeightChanged(mBottomControlsHeight, mBottomControlsMinHeight);
            }
        }
    }

    @Override
    public void setBottomControlsAdditionalHeight(int height) {
        mBottomControlsAdditionalHeight = height;
    }

    private void topControlsAnimationMaybeStarted(
            int oldHeight, int oldMinHeight, int newHeight, int newMinHeight) {
        mHasTopControlsHeightAnimation = shouldAnimateBrowserControlsHeightChanges();
        if (mHasTopControlsHeightAnimation) {
            mTopAnimationInitialOffset = getTopControlOffset();
        }
        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled() && !isVisibilityForced()) {
            updateTopControlsOffsetTagConstraints(oldHeight, oldMinHeight, newHeight, newMinHeight);
        }
    }

    private void topControlsAnimationEnded() {
        mHasTopControlsHeightAnimation = false;
        if (ChromeFeatureList.sBrowserControlsInViz.isEnabled() && !isVisibilityForced()) {
            updateTopControlsOffsetTagConstraints(
                    mTopControlsHeight,
                    mTopControlsMinHeight,
                    mTopControlsHeight,
                    mTopControlsMinHeight);
        }
    }

    @Override
    public void setTopControlsHeight(int topControlsHeight, int topControlsMinHeight) {
        if (doSyncMinHeightWithTotalHeight()) {
            topControlsMinHeight = topControlsHeight;
        }

        if (mTopControlsHeight == topControlsHeight
                && mTopControlsMinHeight == topControlsMinHeight) {
            return;
        }
        try (TraceEvent e = TraceEvent.scoped("BrowserControlsManager.setTopControlsHeight")) {
            final int oldTopHeight = mTopControlsHeight;
            final int oldTopMinHeight = mTopControlsMinHeight;
            mTopControlsHeight = topControlsHeight;
            mTopControlsMinHeight = topControlsMinHeight;

            if (!canAnimateNativeBrowserControls()) {
                if (shouldAnimateBrowserControlsHeightChanges()) {
                    runBrowserDrivenTopControlsHeightChangeAnimation(oldTopHeight, oldTopMinHeight);
                } else {
                    showAndroidControls(false);
                }
            }

            // Signal that the animation started even if canAnimateNativeBrowserControls() returns
            // false in the previous block, in case the browser driven animation changes to a
            // renderer driven animation.
            topControlsAnimationMaybeStarted(
                    oldTopHeight, oldTopMinHeight, topControlsHeight, topControlsMinHeight);

            for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
                obs.onTopControlsHeightChanged(mTopControlsHeight, mTopControlsMinHeight);
            }
        }
    }

    @Override
    public void setAnimateBrowserControlsHeightChanges(
            boolean animateBrowserControlsHeightChanges) {
        mAnimateBrowserControlsHeightChanges = animateBrowserControlsHeightChanges;
    }

    @Override
    public int getTopControlsHeight() {
        return mTopControlsHeight;
    }

    @Override
    public int getTopControlsHairlineHeight() {
        if (mControlContainer == null) {
            return 0;
        } else {
            return mControlContainer.getToolbarHairlineHeight();
        }
    }

    @Override
    public int getTopControlsMinHeight() {
        return mTopControlsMinHeight;
    }

    @Override
    public int getBottomControlsHeight() {
        return mBottomControlsHeight;
    }

    @Override
    public int getBottomControlsMinHeight() {
        return mBottomControlsMinHeight;
    }

    @Override
    public boolean shouldAnimateBrowserControlsHeightChanges() {
        return mAnimateBrowserControlsHeightChanges;
    }

    private boolean shouldUpdateOffsetsWhenConstraintsChange(
            @BrowserControlsState int constraints) {
        // With BCIV enabled, scrolls will not update the offsets in the browser's property models
        // anymore. The browser compositor frame will always show the controls in their fully
        // visible state. When the controls become locked, their offset tags will be removed, which
        // means the offset tag values won't be applied anymore, which means the controls will be
        // drawn at their fully visible positions. If the controls were not at their fully visible
        // positions before their offset tags were removed, then we need to update the property
        // models with the correct offsets to avoid visible jumps.
        // More specifically, there are two cases where this happens when the controls become locked
        // after being scrolled off screen:
        // - If we transition to a HIDDEN state, then the renderer sees the controls are already not
        // visible, so it will not notify the browser to hide them. So the browser needs to update
        // the offsets to hide the controls.
        // - If we transition to a SHOWN state, the browser also needs to update the offsets,
        // otherwise the animation to show the controls will start with a frame where the controls
        // are fully visible.
        boolean areControlsOffscreen = false;
        if (getControlsPosition() == ControlsPosition.TOP) {
            areControlsOffscreen = getContentOffset() == getTopControlsMinHeight();
        } else if (getControlsPosition() == ControlsPosition.BOTTOM) {
            areControlsOffscreen = getBottomContentOffset() == getBottomControlsMinHeight();
        }

        return (areControlsOffscreen
                && (constraints == BrowserControlsState.HIDDEN
                        || constraints == BrowserControlsState.SHOWN));
    }

    @Override
    public int getContentOffset() {
        return mRendererTopContentOffset;
    }

    @Override
    public int getTopControlOffset() {
        return mRendererTopControlOffset;
    }

    @Override
    public int getTopControlsMinHeightOffset() {
        return mRendererTopControlsMinHeightOffset;
    }

    private int getBottomContentOffset() {
        return BrowserControlsUtils.getBottomContentOffset(this);
    }

    @Override
    public int getBottomControlOffset() {
        // If the height is currently 0, the offset generated by the bottom controls should be too.
        // TODO(crbug.com/40112494): Send a offset update from the browser controls manager when the
        // height changes to ensure correct offsets (removing the need for min()).
        return Math.min(mRendererBottomControlOffset, mBottomControlsHeight);
    }

    @Override
    public int getBottomControlsMinHeightOffset() {
        return mRendererBottomControlsMinHeightOffset;
    }

    private void updateControlOffset() {
        if (mControlsPosition == ControlsPosition.NONE) return;

        if (mControlsPosition == ControlsPosition.TOP) {
            mControlOffsetRatio =
                    getTopControlsHeight() == 0
                            ? 1.0f
                            : Math.abs((float) mRendererTopControlOffset / getTopControlsHeight());
        } else {
            mControlOffsetRatio =
                    getBottomControlsHeight() == 0
                            ? 1.0f
                            : Math.abs(
                                    (float) mRendererBottomControlOffset
                                            / getBottomControlsHeight());
        }
    }

    @Override
    public float getTopVisibleContentOffset() {
        return getTopControlsHeight() + getTopControlOffset();
    }

    @Override
    public int getAndroidControlsVisibility() {
        return mControlContainer == null
                ? View.INVISIBLE
                : mControlContainer.getView().getVisibility();
    }

    @Override
    public int getControlsPosition() {
        return mControlsPosition;
    }

    @Override
    public boolean isVisibilityForced() {
        Tab tab = getTab();

        // If the tab gets destroyed, we should be transitioning to a state
        // where there are no tabs (in which case we show the grid tab
        // switcher) or we change to another tab (in which case we force the
        // controls to be visible for a while.)
        if (tab != null && tab.isDestroyed()) {
            return true;
        }

        @BrowserControlsState
        int constraints = TabBrowserControlsConstraintsHelper.getConstraints(tab);
        return constraints == BrowserControlsState.HIDDEN
                || constraints == BrowserControlsState.SHOWN;
    }

    @Override
    public void setControlsPosition(
            @ControlsPosition int controlsPosition,
            int newTopControlsHeight,
            int newTopControlsMinHeight,
            int newRendererTopControlsOffset,
            int newBottomControlsHeight,
            int newBottomControlsMinHeight,
            int newRendererBottomControlsOffset) {
        assert controlsPosition == ControlsPosition.TOP
                        || controlsPosition == ControlsPosition.BOTTOM
                : "Cannot change to ControlPosition.NONE after initialization";
        if (mControlsPosition == controlsPosition) return;
        try (TraceEvent e = TraceEvent.scoped("BrowserControlsManager.setControlsPosition")) {
            topControlsAnimationMaybeStarted(
                    mTopControlsHeight,
                    mTopControlsMinHeight,
                    newTopControlsHeight,
                    newTopControlsMinHeight);
            bottomControlsAnimationMaybeStarted(
                    mBottomControlsHeight,
                    mBottomControlsMinHeight,
                    newBottomControlsHeight,
                    newBottomControlsMinHeight);

            // Only one pending update to browser controls params can be in-flight at once, so we
            // need to fully update all params before notifying that the params have changed via
            // observer methods. If we don't, a partial update will get pushed that delays the final
            // state from being recognized and prevents animations from running correctly.
            mControlsPosition = controlsPosition;
            mTopControlsHeight = newTopControlsHeight;
            mTopControlsMinHeight = newTopControlsMinHeight;
            mRendererTopContentOffset = newRendererTopControlsOffset + newTopControlsHeight;
            mBottomControlsHeight = newBottomControlsHeight;
            mBottomControlsMinHeight = newBottomControlsMinHeight;
            // If the controls position changes concurrently with a change to renderer offset(s),
            // the control container will be invisible during the subsequent layout pass. This
            // causes it to fail to draw when it returns to visible, so we force a relayout upon
            // returning to visibility via this flag.
            mForceRelayoutOnVisibilityChange =
                    newRendererTopControlsOffset != 0 || newRendererBottomControlsOffset != 0;
            if (canAnimateNativeBrowserControls()) {
                mRendererTopControlOffset = newRendererTopControlsOffset;
                mRendererBottomControlOffset = newRendererBottomControlsOffset;
            } else {
                mRendererTopControlOffset = 0;
                mRendererBottomControlOffset = 0;
            }

            for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
                obs.onTopControlsHeightChanged(newTopControlsHeight, newTopControlsMinHeight);
                obs.onBottomControlsHeightChanged(
                        newBottomControlsHeight, newBottomControlsMinHeight);
            }

            updateControlOffset();

            // With BCIV, if there's an animation, updating offsets here causes incorrect animation
            // frames because the browser submits a frame with the height update before the offsets
            // in the renderer and browser are updated.
            // When visibility is forced, BCIV doesn't apply, so offsets should still be updated.
            if (!ChromeFeatureList.sBcivBottomControls.isEnabled()
                    || !shouldAnimateBrowserControlsHeightChanges()
                    || isVisibilityForced()) {
                notifyControlOffsetChanged();
            }
            notifyControlsPositionChanged();
        }
    }

    @Override
    public void notifyBackgroundColor(@ColorInt int color) {
        for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
            obs.onBottomControlsBackgroundColorChanged(color);
        }
    }

    @Override
    public void addObserver(BrowserControlsStateProvider.Observer obs) {
        mControlsObservers.addObserver(obs);
    }

    @Override
    public void removeObserver(BrowserControlsStateProvider.Observer obs) {
        mControlsObservers.removeObserver(obs);
    }

    /**
     * Utility routine for ensuring visibility updates are synchronized with animation, preventing
     * message loop stalls due to untimely invalidation.
     */
    private void scheduleVisibilityUpdate() {
        if (mControlContainer == null) {
            return;
        }
        final int desiredVisibility = shouldShowAndroidControls() ? View.VISIBLE : View.INVISIBLE;
        if (mControlContainer.getView().getVisibility() == desiredVisibility) return;
        mControlContainer.getView().removeCallbacks(mUpdateVisibilityRunnable);
        mControlContainer.getView().postOnAnimation(mUpdateVisibilityRunnable);
    }

    /**
     * Forces the Android controls to hide. While there are acquired tokens the browser controls
     * Android view will always be hidden, otherwise they will show/hide based on position.
     *
     * NB: this only affects the Android controls. For controlling composited toolbar visibility,
     * implement {@link BrowserControlsVisibilityDelegate#canShowBrowserControls()}.
     */
    private int hideAndroidControls() {
        return mHidingTokenHolder.acquireToken();
    }

    @Override
    public int hideAndroidControlsAndClearOldToken(int oldToken) {
        int newToken = hideAndroidControls();
        mHidingTokenHolder.releaseToken(oldToken);
        return newToken;
    }

    @Override
    public void releaseAndroidControlsHidingToken(int token) {
        mHidingTokenHolder.releaseToken(token);
    }

    @EnsuresNonNullIf({"mControlContainer"})
    private boolean shouldShowAndroidControls() {
        if (mControlContainer == null) return false;
        if (mHidingTokenHolder.hasTokens()) {
            return false;
        }
        if (offsetOverridden()) return true;

        return !BrowserControlsUtils.drawControlsAsTexture(this);
    }

    /**
     * Updates the positions of the browser controls and content to the default non fullscreen
     * values.
     */
    private void setPositionsForTabToNonFullscreen() {
        Tab tab = getTab();
        if (tab == null
                || !tab.isInitialized()
                || TabBrowserControlsConstraintsHelper.getConstraints(tab)
                        != BrowserControlsState.HIDDEN) {
            setPositionsForTab(
                    0,
                    0,
                    getTopControlsHeight(),
                    getTopControlsMinHeight(),
                    getBottomControlsMinHeight());
        } else {
            // Tab isn't null and the BrowserControlsState is HIDDEN. In this case, set the offsets
            // to values that will position the browser controls at the min-height.
            setPositionsForTab(
                    getTopControlsMinHeight() - getTopControlsHeight(),
                    getBottomControlsHeight() - getBottomControlsMinHeight(),
                    getTopControlsMinHeight(),
                    getTopControlsMinHeight(),
                    getBottomControlsMinHeight());
        }
    }

    /**
     * Updates the positions of the browser controls and content based on the desired position of
     * the current tab.
     * @param topControlsOffset The Y offset of the top controls in px.
     * @param bottomControlsOffset The Y offset of the bottom controls in px.
     * @param topContentOffset The Y offset for the content in px.
     * @param topControlsMinHeightOffset The Y offset for the top controls min-height in px.
     * @param bottomControlsMinHeightOffset The Y offset for the bottom controls min-height in px.
     */
    private void setPositionsForTab(
            int topControlsOffset,
            int bottomControlsOffset,
            int topContentOffset,
            int topControlsMinHeightOffset,
            int bottomControlsMinHeightOffset) {
        // This min/max logic is here to handle changes in the browser controls height. For example,
        // if we change either height to 0, the offsets of the controls should also be 0. This works
        // assuming we get an event from the renderer after the browser control heights change.
        int rendererTopControlOffset = Math.max(topControlsOffset, -getTopControlsHeight());
        int rendererBottomControlOffset = Math.min(bottomControlsOffset, getBottomControlsHeight());

        int rendererTopContentOffset =
                Math.min(topContentOffset, rendererTopControlOffset + getTopControlsHeight());

        if (rendererTopControlOffset == mRendererTopControlOffset
                && rendererBottomControlOffset == mRendererBottomControlOffset
                && rendererTopContentOffset == mRendererTopContentOffset
                && topControlsMinHeightOffset == mRendererTopControlsMinHeightOffset
                && bottomControlsMinHeightOffset == mRendererBottomControlsMinHeightOffset) {
            return;
        }

        mRendererTopControlsMinHeightChanged =
                mRendererTopControlsMinHeightOffset != topControlsMinHeightOffset;
        mRendererBottomControlsMinHeightChanged =
                mRendererBottomControlsMinHeightOffset != bottomControlsMinHeightOffset;
        mRendererTopControlOffset = rendererTopControlOffset;
        mRendererBottomControlOffset = rendererBottomControlOffset;
        mRendererTopControlsMinHeightOffset = topControlsMinHeightOffset;
        mRendererBottomControlsMinHeightOffset = bottomControlsMinHeightOffset;
        mRendererTopContentOffset = rendererTopContentOffset;

        mControlsAtMinHeight.set(
                getContentOffset() == getTopControlsMinHeight()
                        && getBottomContentOffset() == getBottomControlsMinHeight());
        updateControlOffset();
        notifyControlOffsetChanged();
    }

    private void notifyControlOffsetChanged() {
        try (TraceEvent e =
                TraceEvent.scoped("BrowserControlsManager.notifyControlOffsetChanged")) {
            scheduleVisibilityUpdate();
            if (shouldShowAndroidControls() && mControlsPosition == ControlsPosition.TOP) {
                mControlContainer.getView().setTranslationY(getTopControlOffset());
            }

            // Explicitly tell the compositor to draw again. Should be |true| only when the android
            // views for the browser controls are visible, or when the android browser controls are
            // being moved by a browser driven animation. Browser driven animations refer to
            // situations where composited views do not exist. Note: requestNewFrame can be false,
            // and a new browser compositor frame could still be produced if other observers make
            // changes to the layer tree.
            boolean requestNewFrame = shouldShowAndroidControls();

            // With BCIV enabled, renderer scrolling will not update the control offsets of the
            // browser's compositor frame, but we still want this update to happen if the browser
            // is controlling the controls.
            for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
                obs.onControlsOffsetChanged(
                        getTopControlOffset(),
                        getTopControlsMinHeightOffset(),
                        mRendererTopControlsMinHeightChanged,
                        getBottomControlOffset(),
                        getBottomControlsMinHeightOffset(),
                        mRendererBottomControlsMinHeightChanged,
                        requestNewFrame,
                        isVisibilityForced());
            }

            boolean atTopInitialOffset = getTopControlOffset() == mTopAnimationInitialOffset;
            boolean atFinalTopMinHeightOffset =
                    mRendererTopControlsMinHeightChanged
                            && (mTopControlsMinHeight == mRendererTopControlsMinHeightOffset);
            boolean topControlsLastAnimationFrame =
                    mHasTopControlsHeightAnimation
                            && (atTopInitialOffset || atFinalTopMinHeightOffset);
            if (topControlsLastAnimationFrame) {
                topControlsAnimationEnded();
            }

            boolean atBottomInitialOffset =
                    getBottomControlOffset() == mBottomAnimationInitialOffset;
            boolean atFinalBottomMinHeightOffset =
                    mRendererBottomControlsMinHeightChanged
                            && (mBottomControlsMinHeight == mRendererBottomControlsMinHeightOffset);
            boolean bottomControlsLastAnimationFrame =
                    mHasBottomControlsHeightAnimation
                            && (atBottomInitialOffset || atFinalBottomMinHeightOffset);
            if (bottomControlsLastAnimationFrame) {
                bottomControlsAnimationEnded();
            }
        }
    }

    private void notifyConstraintsChanged(
            BrowserControlsOffsetTagsInfo oldOffsetTagsInfo,
            BrowserControlsOffsetTagsInfo offsetTagsInfo,
            @BrowserControlsState int constraints) {
        for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
            obs.onOffsetTagsInfoChanged(
                    oldOffsetTagsInfo,
                    offsetTagsInfo,
                    constraints,
                    shouldUpdateOffsetsWhenConstraintsChange(constraints));
        }
    }

    private void notifyControlsPositionChanged() {
        for (BrowserControlsStateProvider.Observer obs : mControlsObservers) {
            obs.onControlsPositionChanged(mControlsPosition);
        }
    }

    /**
     * Called when offset values related with fullscreen functionality has been changed by the
     * compositor.
     *
     * @param topControlsOffsetY The Y offset of the top controls in physical pixels.
     * @param bottomControlsOffsetY The Y offset of the bottom controls in physical pixels.
     * @param contentOffsetY The Y offset of the content in physical pixels.
     * @param topControlsMinHeightOffsetY The current offset of the top controls min-height.
     * @param bottomControlsMinHeightOffsetY The current offset of the bottom controls min-height.
     */
    private void onOffsetsChanged(
            int topControlsOffsetY,
            int bottomControlsOffsetY,
            int contentOffsetY,
            int topControlsMinHeightOffsetY,
            int bottomControlsMinHeightOffsetY) {
        // Cancel any animation on the Android controls and let compositor drive the offset updates.
        resetControlsOffsetOverridden();

        Tab tab = getTab();
        assert tab != null;
        if (SadTab.isShowing(tab) || tab.isNativePage()) {
            showAndroidControls(false);
        } else {
            updateBrowserControlsOffsets(
                    false,
                    topControlsOffsetY,
                    bottomControlsOffsetY,
                    contentOffsetY,
                    topControlsMinHeightOffsetY,
                    bottomControlsMinHeightOffsetY);
        }
    }

    @Override
    public void showAndroidControls(boolean animate) {
        if (animate) {
            runBrowserDrivenShowAnimation();
        } else {
            updateBrowserControlsOffsets(
                    true,
                    0,
                    0,
                    getTopControlsHeight(),
                    getTopControlsMinHeight(),
                    getBottomControlsMinHeight());
        }
    }

    @Override
    public void restoreControlsPositions() {
        resetControlsOffsetOverridden();

        // Make sure the dominant control offsets have been set.
        Tab tab = getTab();
        TabBrowserControlsOffsetHelper offsetHelper = null;
        if (tab != null) offsetHelper = TabBrowserControlsOffsetHelper.get(tab);

        // Browser controls should always be shown on native pages and restoring offsets might cause
        // the controls to get stuck in an invalid position.
        if (offsetHelper != null
                && offsetHelper.offsetInitialized()
                && tab != null
                && !tab.isNativePage()) {
            updateBrowserControlsOffsets(
                    false,
                    offsetHelper.topControlsOffset(),
                    offsetHelper.bottomControlsOffset(),
                    offsetHelper.contentOffset(),
                    offsetHelper.topControlsMinHeightOffset(),
                    offsetHelper.bottomControlsMinHeightOffset());
        } else {
            showAndroidControls(false);
        }
        TabBrowserControlsConstraintsHelper.updateEnabledState(tab);
    }

    /** Helper method to update offsets and notify offset changes to observers if necessary. */
    private void updateBrowserControlsOffsets(
            boolean toNonFullscreen,
            int topControlsOffset,
            int bottomControlsOffset,
            int topContentOffset,
            int topControlsMinHeightOffset,
            int bottomControlsMinHeightOffset) {
        if (toNonFullscreen) {
            setPositionsForTabToNonFullscreen();
        } else {
            setPositionsForTab(
                    topControlsOffset,
                    bottomControlsOffset,
                    topContentOffset,
                    topControlsMinHeightOffset,
                    bottomControlsMinHeightOffset);
        }
    }

    @Override
    public boolean offsetOverridden() {
        return mOffsetOverridden;
    }

    /**
     * Sets the flat indicating if browser control offset is overridden by animation.
     * @param flag Boolean flag of the new offset overridden state.
     */
    private void setOffsetOverridden(boolean flag) {
        mOffsetOverridden = flag;
    }

    /** Helper method to cancel overridden offset on Android browser controls. */
    private void resetControlsOffsetOverridden() {
        if (!offsetOverridden()) return;
        if (mControlsAnimator != null) mControlsAnimator.cancel();
        setOffsetOverridden(false);
    }

    /** Helper method to run slide-in animations on the Android browser controls views. */
    private void runBrowserDrivenShowAnimation() {
        if (mControlsAnimator != null) return;

        setOffsetOverridden(true);

        final float hiddenRatio = getBrowserControlHiddenRatio();
        final int topControlHeight = getTopControlsHeight();
        final int topControlOffset = getTopControlOffset();

        // Set animation start value to current renderer controls offset.
        mControlsAnimator = ValueAnimator.ofInt(topControlOffset, 0);
        mControlsAnimator.setDuration(
                (long) Math.abs(hiddenRatio * CONTROLS_ANIMATION_DURATION_MS));
        mControlsAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        mControlsAnimator = null;
                    }

                    @Override
                    public void onAnimationCancel(Animator animation) {
                        updateBrowserControlsOffsets(
                                false,
                                0,
                                0,
                                topControlHeight,
                                getTopControlsMinHeight(),
                                getBottomControlsMinHeight());
                    }
                });
        mControlsAnimator.addUpdateListener(
                (animator) -> {
                    updateBrowserControlsOffsets(
                            false,
                            (int) animator.getAnimatedValue(),
                            0,
                            topControlHeight,
                            getTopControlsMinHeight(),
                            getBottomControlsMinHeight());
                });
        mControlsAnimator.start();
    }

    private void runBrowserDrivenTopControlsHeightChangeAnimation(
            int oldTopControlsHeight, int oldTopControlsMinHeight) {
        runBrowserDrivenControlsAnimation(
                oldTopControlsHeight,
                oldTopControlsMinHeight,
                getBottomControlsHeight(),
                getBottomControlsMinHeight());
    }

    private void runBrowserDrivenBottomControlsHeightChangeAnimation(
            int oldBottomControlsHeight, int oldBottomControlsMinHeight) {
        runBrowserDrivenControlsAnimation(
                getTopControlsHeight(),
                getTopControlsMinHeight(),
                oldBottomControlsHeight,
                oldBottomControlsMinHeight);
    }

    private void runBrowserDrivenControlsAnimation(
            int oldTopControlsHeight,
            int oldTopControlsMinHeight,
            int oldBottomControlsHeight,
            int oldBottomControlsMinHeight) {
        if (mControlsAnimator != null) return;
        assert getContentOffset() == oldTopControlsHeight
                : "Height change animations are implemented for fully shown controls only!";

        setOffsetOverridden(true);

        final int newTopControlsHeight = getTopControlsHeight();
        final int newTopControlsMinHeight = getTopControlsMinHeight();
        final int newBottomControlsHeight = getBottomControlsHeight();
        final int newBottomControlsMinHeight = getBottomControlsMinHeight();

        mControlsAnimator = ValueAnimator.ofFloat(0.f, 1.f);
        mControlsAnimator.addUpdateListener(
                (animator) -> {
                    final float topValue = (float) animator.getAnimatedValue();
                    final float topControlsMinHeightOffset =
                            interpolate(topValue, oldTopControlsMinHeight, newTopControlsMinHeight);
                    final float topContentOffset =
                            interpolate(topValue, oldTopControlsHeight, newTopControlsHeight);
                    final float topControlsOffset = topContentOffset - newTopControlsHeight;

                    // Bottom controls offsets need to change in the opposite direction, so use the
                    // same calculations but with animation progress going from 1 to 0 instead of 0
                    // to 1.
                    final float bottomValue = 1.f - topValue;
                    final float bottomControlsMinHeightOffset =
                            interpolate(
                                    bottomValue,
                                    oldBottomControlsMinHeight,
                                    newBottomControlsMinHeight);
                    final float bottomControlsOffset =
                            interpolate(
                                    bottomValue, oldBottomControlsHeight, newBottomControlsHeight);

                    updateBrowserControlsOffsets(
                            false,
                            (int) topControlsOffset,
                            (int) bottomControlsOffset,
                            (int) topContentOffset,
                            (int) topControlsMinHeightOffset,
                            (int) bottomControlsMinHeightOffset);
                });
        mControlsAnimator.setDuration(CONTROLS_ANIMATION_DURATION_MS);
        mControlsAnimator.addListener(
                new AnimatorListenerAdapter() {
                    @Override
                    public void onAnimationEnd(Animator animation) {
                        updateBrowserControlsOffsets(
                                false,
                                0,
                                0,
                                getTopControlsHeight(),
                                getTopControlsMinHeight(),
                                getBottomControlsMinHeight());
                        mControlsAnimator = null;
                    }
                });
        mControlsAnimator.start();
    }

    private static float interpolate(float progress, float oldValue, float newValue) {
        return oldValue + progress * (newValue - oldValue);
    }

    private boolean canAnimateNativeBrowserControls() {
        final Tab tab = getTab();
        return tab != null && tab.isUserInteractable() && !tab.isNativePage();
    }

    private void updateOffsetTagDefinitions(BrowserControlsOffsetTagDefinitions newDefinitions) {
        mOffsetTagDefinitions = newDefinitions;

        Tab tab = getTab();
        WebContents webContents = tab != null ? tab.getWebContents() : null;
        if (webContents != null) {
            webContents.updateOffsetTagDefinitions(mOffsetTagDefinitions);
        }
    }

    private void updateTopControlsOffsetTagConstraints(
            int oldHeight, int oldMinHeight, int newHeight, int newMinHeight) {
        // These constraints allow the top controls to move within the new scrollable range.
        int minY = -(newHeight - newMinHeight);
        int maxY = 0;

        // Sometimes, the constraints need to be adjusted to allow for a wider range of movement
        // during an animation. This is because all offsets during an animation are applied with
        // respect to the new height, and the controls could be animating from a position that is
        // outside of the new scrollable range.
        if (mHasTopControlsHeightAnimation) {
            // If the controls are shrinking in height while they are fully visible, the offsets
            // will be greater than 0 throughout the animation.
            if (oldHeight > newHeight) {
                maxY = oldHeight - newHeight;
            }

            // If the controls are growing in min height while they are fully hidden, the offsets
            // will be smaller than lower end of the scrollable range throughout the animation.
            if (newMinHeight > oldMinHeight) {
                minY -= newMinHeight - oldMinHeight;
            }
        }

        OffsetTagConstraints newTopConstraints =
                new OffsetTagConstraints(0, 0, minY - getTopControlsHairlineHeight(), maxY);
        OffsetTagConstraints newContentConstraints = new OffsetTagConstraints(0, 0, minY, maxY);
        BrowserControlsOffsetTagConstraints constraints =
                new BrowserControlsOffsetTagConstraints(
                        newTopConstraints,
                        newContentConstraints,
                        mOffsetTagDefinitions.getConstraints().getBottomControlsConstraints());
        constraints.assertAndFixConstraints("BrowserControlsManager updating top constraints ");
        updateOffsetTagDefinitions(
                new BrowserControlsOffsetTagDefinitions(
                        mOffsetTagDefinitions.getTags(), constraints));
    }

    private void updateBottomControlsOffsetTagConstraints(
            int oldHeight, int oldMinHeight, int newHeight, int newMinHeight) {
        int minY = 0;
        int maxY = newHeight - newMinHeight;

        // crbug.com/406429149: unlike other browser controls, the height for the custom tabs bottom
        // controls is inclusive of the shadow's height, so newHeight can be negative.
        maxY = Math.max(0, maxY);

        // See comment in updateTopControlsOffsetTagConstraints(), the logic is similar.
        if (mHasBottomControlsHeightAnimation) {
            if (oldHeight > newHeight) {
                minY = -(oldHeight - newHeight);
            }

            if (newMinHeight > oldMinHeight) {
                maxY += newMinHeight - oldMinHeight;
            }
        }

        OffsetTagConstraints newBottomConstraints =
                new OffsetTagConstraints(0, 0, minY, maxY + mBottomControlsAdditionalHeight);
        BrowserControlsOffsetTagConstraints constraints =
                new BrowserControlsOffsetTagConstraints(
                        mOffsetTagDefinitions.getConstraints().getTopControlsConstraints(),
                        mOffsetTagDefinitions.getConstraints().getContentConstraints(),
                        newBottomConstraints);
        constraints.assertAndFixConstraints("BrowserControlsManager updating bottom constraints ");
        updateOffsetTagDefinitions(
                new BrowserControlsOffsetTagDefinitions(
                        mOffsetTagDefinitions.getTags(), constraints));
    }

    /** Destroys the BrowserControlsManager */
    public void destroy() {
        mTab = null;
        mHtmlApiHandler.destroy();
        if (mActiveTabObserver != null) mActiveTabObserver.destroy();
        mBrowserVisibilityDelegate.destroy();
        if (mTabControlsObserver != null) mTabControlsObserver.destroy();
    }

    /**
     * Disables locking the top control height. Used in TWAs with display modes that want
     * flexibility on whether to show a custom tab bar (notably MINIMAL_UI and
     * WINDOW_CONTROLS_OVERLAY).
     */
    public void disableSyncMinHeightWithTotalHeight() {
        mDisableSyncMinHeightWithTotalHeight = true;
    }

    private boolean doSyncMinHeightWithTotalHeight() {
        // When V2 flag is enabled, this logic is coordinated in TopControlsStacker.
        if (BrowserControlsUtils.doSyncMinHeightWithTotalHeightV2()) return false;
        if (mDisableSyncMinHeightWithTotalHeight) return false;

        return BrowserControlsUtils.doSyncMinHeightWithTotalHeight(mActivity);
    }

    @NullUnmarked
    public TabModelSelectorTabObserver getTabControlsObserverForTesting() {
        return mTabControlsObserver;
    }

    @NullUnmarked
    ValueAnimator getControlsAnimatorForTesting() {
        return mControlsAnimator;
    }

    int getControlsAnimationDurationMsForTesting() {
        return CONTROLS_ANIMATION_DURATION_MS;
    }
}
