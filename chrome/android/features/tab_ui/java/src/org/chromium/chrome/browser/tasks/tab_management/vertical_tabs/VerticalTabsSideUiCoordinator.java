// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.transition.ChangeBounds;
import android.transition.Fade;
import android.transition.Transition;
import android.transition.TransitionSet;
import android.view.View;
import android.view.ViewGroup;
import android.widget.FrameLayout;

import androidx.annotation.Px;
import androidx.annotation.StringRes;

import org.chromium.base.MathUtils;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListCoordinator.ExternalDragObserver;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.side_ui.SideUiContainer;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.AnchorSide;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.HeightType;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiId;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.SideUiSpecs.SideUiSize;
import org.chromium.chrome.browser.ui.side_ui.SideUiCoordinator.UiUpdateRequest;
import org.chromium.chrome.browser.ui.side_ui.SideUiObserver;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.WindowWidthBoundary;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * Coordinator that acts as a container for the Vertical Tab List within the Side UI framework. This
 * wraps {@link VerticalTabListCoordinator} to adapt it to the {@link SideUiContainer} interface,
 * separating container-level layout and sizing concerns from the tab list itself.
 *
 * <p>The user-resized rail width belongs to this window. It is loaded from shared prefs once, at
 * construction, and is written back when the user resizes the rail and whenever this window becomes
 * the top resumed activity, so that a new window starts with the width of the last active window.
 */
@NullMarked
public class VerticalTabsSideUiCoordinator
        implements SideUiContainer, SideUiObserver, WindowAndroid.ActivityStateObserver {
    static final int VIEW_WIDTH_DP = VerticalTabUtils.SIDE_UI_CONTAINER_WIDTH_DP;
    static final int COLLAPSED_WIDTH_DP = VerticalTabUtils.SIDE_UI_CONTAINER_COLLAPSED_WIDTH_DP;

    private final SideUiCoordinator mSideUiCoordinator;
    private final WindowAndroid mWindowAndroid;
    private final FrameLayout mRootView;
    private final @AnchorSide int mAnchorSide;
    private final VerticalTabListCoordinator mTabListCoordinator;
    private final VerticalTabRailCollapseController mCollapseController;
    private final @Px int mExpandedViewWidth;
    private final @Px int mCollapsedViewWidth;
    private final @Px int mMinManualWidth;
    private final @Px int mMaxManualWidth;
    private final SettableNonNullObservableSupplier<Boolean> mIsVerticalTabsActiveSupplier;
    private final SettableNonNullObservableSupplier<Boolean> mIsAutoHiddenSupplier =
            ObservableSuppliers.createNonNull(false);

    // Whether the vertical tab is set to visible via UI. Remains true even if it is temporarily
    // hidden by other conditions such as narrow window i.e. |mIsAutoHiddenSupplier.get()| is true.
    private boolean mManualVisible;
    // The rail width proposed by an in-progress manual resize drag, or 0 if no drag is in progress.
    private @Px int mLiveResizeWidth;
    // The expanded rail width in dp chosen by a manual resize, or 0 if the user never resized it.
    private int mCommittedResizeWidthDp;
    private boolean mIsDestroyed;

    /**
     * Freezes the rail while a tab or group is dragged out of it. See {@link
     * #onExternalDragStarted} and {@link #onExternalDragEnded}.
     */
    private final ExternalDragObserver mExternalDragObserver =
            new ExternalDragObserver() {
                @Override
                public void onExternalDragStarted() {
                    VerticalTabsSideUiCoordinator.this.onExternalDragStarted();
                }

                @Override
                public void onExternalDragEnded() {
                    VerticalTabsSideUiCoordinator.this.onExternalDragEnded();
                }
            };

    public VerticalTabsSideUiCoordinator(
            Activity activity,
            WindowAndroid windowAndroid,
            SideUiCoordinator sideUiCoordinator,
            VerticalTabListCoordinator tabListCoordinator,
            SettableNonNullObservableSupplier<Boolean> isVerticalTabsActiveSupplier) {
        mAnchorSide = AnchorSide.LEFT;

        mSideUiCoordinator = sideUiCoordinator;
        mWindowAndroid = windowAndroid;
        mTabListCoordinator = tabListCoordinator;
        mCollapseController = mTabListCoordinator.getCollapseController();
        mIsVerticalTabsActiveSupplier = isVerticalTabsActiveSupplier;
        mCommittedResizeWidthDp = VerticalTabUtils.getUserResizedWidthDpFromSharedPref();
        mSideUiCoordinator.addObserver(this);
        mSideUiCoordinator.registerSideUiContainer(this);
        mWindowAndroid.addActivityStateObserver(this);

        mRootView = new FrameLayout(activity);
        mRootView.setLayoutParams(
                new FrameLayout.LayoutParams(
                        FrameLayout.LayoutParams.MATCH_PARENT,
                        FrameLayout.LayoutParams.MATCH_PARENT));
        mRootView.addView(mTabListCoordinator.getView());
        mExpandedViewWidth = ViewUtils.dpToPx(activity, VIEW_WIDTH_DP);
        mCollapsedViewWidth = ViewUtils.dpToPx(activity, COLLAPSED_WIDTH_DP);
        mMinManualWidth = ViewUtils.dpToPx(activity, VerticalTabUtils.MIN_EXPANDED_WIDTH_DP);
        mMaxManualWidth = ViewUtils.dpToPx(activity, VerticalTabUtils.MAX_EXPANDED_WIDTH_DP);
        mCollapseController.setRailStateChangeDelegate(
                () ->
                        mSideUiCoordinator.updateUi(
                                new UiUpdateRequest(
                                        getSideUiId(), /* suppressAnimations= */ false)));
        mTabListCoordinator.setExternalDragObserver(mExternalDragObserver);
    }

    public NonNullObservableSupplier<Boolean> getIsAutoHiddenSupplier() {
        return mIsAutoHiddenSupplier;
    }

    public void setVisible(boolean show, boolean suppressAnimations) {
        mManualVisible = show;
        if (!show) {
            updateAutoHiddenState(false);
        }
        mSideUiCoordinator.updateUi(new UiUpdateRequest(getSideUiId(), suppressAnimations));
        // Fallback: If hiding VT when spec diff is empty (no hide animation scheduled),
        // update active state immediately to avoid dropping the state update.
        SideUiSpecs currentSpecs = mSideUiCoordinator.getCurrentSideUiSpecs();
        if (!show
                && (currentSpecs == null || currentSpecs.getReservedWidth(getAnchorSide()) == 0)) {
            mIsVerticalTabsActiveSupplier.set(false);
        }
    }

    public void destroy() {
        mIsDestroyed = true;
        updateAutoHiddenState(false);
        mSideUiCoordinator.removeObserver(this);
        // Unregistering also resumes the rail's Side UI updates if a drag is in progress.
        mSideUiCoordinator.unregisterSideUiContainer(this);
        mWindowAndroid.removeActivityStateObserver(this);
        mCollapseController.setRailStateChangeDelegate(null);
        // Cleared before destroying the list on purpose: the list reports an in-progress drag as
        // ended when destroyed, and collapse state deferral should stay on so that nothing is
        // applied to a half-destroyed list.
        mTabListCoordinator.setExternalDragObserver(null);
        mTabListCoordinator.destroy();
        mIsVerticalTabsActiveSupplier.set(false);
    }

    /** Requests keyboard focus on the Vertical Tabs rail. */
    public void requestKeyboardFocus() {
        mTabListCoordinator.requestKeyboardFocus();
    }

    /** Returns whether the Vertical Tabs rail contains keyboard focus. */
    public boolean containsKeyboardFocus() {
        View view = mTabListCoordinator.getView();
        return view != null && view.hasFocus();
    }

    /**
     * Opens the context menu for the currently keyboard-focused tab or group header in Vertical
     * Tabs.
     *
     * @return Whether the context menu was successfully opened.
     */
    public boolean openKeyboardFocusedContextMenu() {
        if (!containsKeyboardFocus()) {
            return false;
        }
        return mTabListCoordinator.openKeyboardFocusedContextMenu();
    }

    /**
     * Returns whether or not Tab layout toggle menu can be activated. Used to grey out the menu
     * item if it cannot be activated due to conditions such as a narrow app Window width.
     */
    public boolean canActivateTabLayoutToggleMenu() {
        return mSideUiCoordinator.canShowSideUi(SideUiId.VERTICAL_TABS);
    }

    /**
     * Freezes the rail for the duration of an OS drag started from it.
     *
     * <p>A Side UI update during the drag (e.g. the window being resized below the width that shows
     * the rail, as when the dragged tab is dropped to enter split-screen) would otherwise resize or
     * detach the rail's subtree mid-drag. A detached subtree is not sent {@code ACTION_DRAG_ENDED},
     * so the drag handler never releases {@code DragDropGlobalState} and every Chrome window in the
     * process stops responding to touch. See crbug.com/556140903.
     *
     * <p>Called in the same stack as the OS accepting the drag, so no update can slip in before the
     * pause. Only the originating window is frozen: it is the one holding the drag state.
     */
    private void onExternalDragStarted() {
        if (!ChromeFeatureList.sSideUiAllowPause.isEnabled()) return;
        // Pause first so any in-flight transition settles at its end state before the rail's
        // contents are frozen.
        mSideUiCoordinator.pauseSideUiUpdates(getSideUiId());
        // The rail's View keeps its width while paused, so applying a new collapse state meanwhile
        // would lay out its contents for a width it does not have.
        mCollapseController.setApplyDeferred(true);
    }

    /**
     * Unfreezes the rail once the drag started from it has ended.
     *
     * <p>Posted rather than run inline: this is called from within {@code
     * ViewGroup#dispatchDragEvent(ACTION_DRAG_ENDED)}, while the rail's ancestors iterate their
     * drag-interested children, and resuming runs a Side UI update that may detach the rail. The
     * update is deferred until that dispatch has unwound.
     */
    private void onExternalDragEnded() {
        if (!ChromeFeatureList.sSideUiAllowPause.isEnabled()) return;
        PostTask.postTask(TaskTraits.UI_DEFAULT, this::unfreezeIfNoExternalDrag);
    }

    private void unfreezeIfNoExternalDrag() {
        // A new drag may have started, or this coordinator been destroyed, before this ran. In the
        // latter case, unregistering already resumed the Side UI updates.
        if (mIsDestroyed || mTabListCoordinator.isExternalDragInProgress()) return;
        // Resume first so the rail gets its new width, then apply the latest collapse state once;
        // no frame is drawn in between.
        mSideUiCoordinator.resumeSideUiUpdates(getSideUiId());
        mCollapseController.setApplyDeferred(false);
    }

    /** Returns whether the rail's Side UI updates are paused, i.e. whether its View is frozen. */
    private boolean isFrozen() {
        return mSideUiCoordinator.areSideUiUpdatesPaused(getSideUiId());
    }

    // SideUiContainer implementation:
    @Override
    public View getView() {
        return mRootView;
    }

    @Override
    public @SideUiId int getSideUiId() {
        return SideUiId.VERTICAL_TABS;
    }

    @Override
    public SideUiSize determineShowableSize(
            @Px int availableWidth, @Px int windowWidth, boolean isFullscreen) {
        Context context = mRootView.getContext();
        int availableWidthDp = ViewUtils.pxToDp(context, availableWidth);
        int windowWidthDp = ViewUtils.pxToDp(context, windowWidth);
        @WindowWidthBoundary
        int boundary = VerticalTabUtils.getWindowWidthBoundary(windowWidthDp, availableWidthDp);

        // SideUiCoordinator only dispatches onSideUiSpecsChangeStarted() when there is a non-empty
        // SideUiSpecs diff. If the window is resized while rail width remains unchanged (e.g.
        // staying at collapsed 76dp across the narrow-window threshold),
        // onSideUiSpecsChangeStarted() will be skipped. Feeding the input here makes the
        // controller re-apply the effective state on forced-collapsed transitions.
        // If any future internal state changes while rail width (specs) remains unchanged,
        // synchronize it here or handle the state transition explicitly.
        mCollapseController.setWindowWidthBoundary(boundary);

        @RailCollapseState int effectiveState = mCollapseController.getEffectiveRailCollapseState();
        @Px
        int renderedWidth =
                calculateRenderedWidthPx(boundary, effectiveState, windowWidth, availableWidth);
        // Expanding on hover is a transient preview, so it must not resize or reposition anything
        // outside the rail: the rail keeps reserving its collapsed width and renders the expanded
        // width over the web contents and the toolbar.
        @Px
        int reservedWidth =
                effectiveState == RailCollapseState.EXPANDED_FOR_HOVERING
                        ? mCollapsedViewWidth
                        : renderedWidth;
        boolean shouldHide = boundary == WindowWidthBoundary.NOT_SHOWABLE;

        updateAutoHiddenState(mManualVisible && shouldHide);
        if (isFullscreen || shouldHide) {
            return new SideUiSize(0, HeightType.NOT_APPLICABLE);
        }
        return new SideUiSize(reservedWidth, renderedWidth, HeightType.TOOLBAR);
    }

    private void updateAutoHiddenState(boolean isHiddenDueToNarrowWidth) {
        mIsAutoHiddenSupplier.set(isHiddenDueToNarrowWidth);
    }

    @Override
    public @AnchorSide int getAnchorSide() {
        return mAnchorSide;
    }

    @Override
    public boolean hasContentToShow(Tab tab) {
        return mManualVisible;
    }

    @Override
    public void setRenderedWidth(int renderedWidth) {
        ViewGroup.LayoutParams layoutParams = mRootView.getLayoutParams();
        if (layoutParams != null) {
            layoutParams.width = renderedWidth;
            mRootView.setLayoutParams(layoutParams);
        }
    }

    @Override
    public boolean shouldLockTopControls() {
        return true;
    }

    @Override
    public void onUiUpdateCompleted(
            @Px int oldReservedWidth,
            @Px int newReservedWidth,
            @HeightType int oldHeightType,
            @HeightType int newHeightType) {
        mIsVerticalTabsActiveSupplier.set(mManualVisible);
    }

    @Override
    public boolean supportsManualResize() {
        if (!VerticalTabUtils.isManualResizeEnabled() || mCollapseController.isForcedCollapsed()) {
            return false;
        }
        // With expand-on-hover, only the expanded rail is resizable: the collapsed rail expands as
        // soon as the pointer reaches its edge, and the hover expansion is transient and only
        // overlays the web contents, so neither has a stable edge to drag.
        return !VerticalTabUtils.isExpandOnHoverEnabled()
                || mCollapseController.getEffectiveRailCollapseState()
                        == RailCollapseState.EXPANDED;
    }

    @Override
    public @StringRes int getResizeHandleContentDescriptionRes() {
        return R.string.accessibility_resize_vertical_tab_rail;
    }

    @Override
    public @Px Integer getResizeHandleWidthPx() {
        // Match the rail's inner padding so that the handle doesn't overlap the tabs.
        return mRootView
                .getResources()
                .getDimensionPixelSize(R.dimen.vertical_tabs_rail_horizontal_margin);
    }

    @Override
    public @Px Integer getResizeHandleBarInsetPx() {
        // Align the bar with the tab list scrollbars, which are drawn at the end of the tab lists.
        // The tab lists extend into the rail's inner padding by their negative end margin.
        Resources res = mRootView.getResources();
        return res.getDimensionPixelSize(R.dimen.vertical_tabs_rail_horizontal_margin)
                + res.getDimensionPixelSize(R.dimen.vertical_tabs_scrollbar_margin_end);
    }

    @Override
    public void onResizeLive(@Px int proposedWidthPx) {
        mLiveResizeWidth = proposedWidthPx;
        mCollapseController.onResizeLive(ViewUtils.pxToDp(mRootView.getContext(), proposedWidthPx));
    }

    @Override
    public void onResizeCommitted(@Px int finalWidthPx) {
        mLiveResizeWidth = 0;
        int finalWidthDp = ViewUtils.pxToDp(mRootView.getContext(), finalWidthPx);
        boolean isCollapsed = mCollapseController.onResizeCommitted(finalWidthDp);

        // A collapsed rail keeps its previous expanded width, so only persist the width chosen for
        // an expanded rail.
        if (!isCollapsed) {
            mCommittedResizeWidthDp = finalWidthDp;
            VerticalTabUtils.setUserResizedWidthDpInSharedPref(mCommittedResizeWidthDp);
        }
    }

    // WindowAndroid.ActivityStateObserver implementation:
    @Override
    public void onActivityTopResumedChanged(boolean isTopResumedActivity) {
        if (!isTopResumedActivity) return;
        // Make this window's width the one a new window starts with.
        VerticalTabUtils.setUserResizedWidthDpInSharedPref(mCommittedResizeWidthDp);
    }

    // SideUiObserver implementation:
    @Override
    public @Nullable Transition onPreSideUiSpecsChange(
            SideUiSpecs sideUiSpecs, UiUpdateRequest request) {
        // A frozen rail keeps its rendered width, so there is nothing to animate.
        if (isFrozen()) return null;

        int side = getAnchorSide();
        // The rail's contents follow the width the rail is rendered at, which is wider than the
        // width it reserves while it is expanded on hover.
        int newRenderedWidth = sideUiSpecs.getRenderedWidth(side);
        int oldRenderedWidth = mSideUiCoordinator.getCurrentSideUiSpecs().getRenderedWidth(side);

        if (oldRenderedWidth > 0 && newRenderedWidth > 0 && oldRenderedWidth != newRenderedWidth) {
            mTabListCoordinator.setInTransition(true);
            // setInTransition() requests a layout with extra layout space for the pinned tabs. Run
            // it now so the offscreen pinned tabs are attached before the rail's views are
            // collected as targets below.
            ViewUtils.triggerSynchronousMeasureAndLayout(mRootView);
            TransitionSet transitionSet =
                    new TransitionSet()
                            .setOrdering(TransitionSet.ORDERING_TOGETHER)
                            .addTransition(new ChangeBounds())
                            .addTransition(new Fade());
            // Only animate the rail's views, leaving everything outside it untouched.
            transitionSet.addTarget(mRootView);
            List<View> railViews = new ArrayList<>();
            ViewUtils.getAllDescendants(mRootView, railViews, Set.of());
            for (View view : railViews) {
                transitionSet.addTarget(view);
            }
            return transitionSet;
        }
        return null;
    }

    @Override
    public void onSideUiSpecsChangeStarted(SideUiSpecs sideUiSpecs, UiUpdateRequest request) {
        mCollapseController.applyEffectiveState();
        mTabListCoordinator.setVerticalTabsReservedWidth(sideUiSpecs.getReservedWidth(mAnchorSide));
    }

    @Override
    public void onSideUiSpecsChangeCompleted(SideUiSpecs sideUiSpecs, UiUpdateRequest request) {
        mTabListCoordinator.setInTransition(false);
    }

    private @Px int calculateRenderedWidthPx(
            @WindowWidthBoundary int boundary,
            @RailCollapseState int effectiveState,
            @Px int windowWidthPx,
            @Px int availableWidthPx) {
        if (effectiveState == RailCollapseState.COLLAPSED) {
            return mCollapsedViewWidth;
        }
        // Expanding on hover is a transient overlay, so it always renders at the fixed container
        // width, independent of the user-resized width and the window-ratio width.
        if (effectiveState == RailCollapseState.EXPANDED_FOR_HOVERING) {
            return mExpandedViewWidth;
        }
        if (VerticalTabUtils.isManualResizeEnabled()) {
            @Px
            int committedResizeWidth =
                    ViewUtils.dpToPx(mRootView.getContext(), mCommittedResizeWidthDp);
            @Px int manualWidth = mLiveResizeWidth != 0 ? mLiveResizeWidth : committedResizeWidth;
            if (manualWidth != 0) {
                // User-resized width overrides auto-sizing and is clamped only by rail and
                // available width bounds.
                return MathUtils.clamp(
                        manualWidth, mMinManualWidth, Math.min(mMaxManualWidth, availableWidthPx));
            }
        }
        if (boundary == WindowWidthBoundary.DYNAMIC_EXPANDABLE) {
            int ratioWidthPx =
                    Math.round(windowWidthPx * VerticalTabUtils.EXPANDED_WINDOW_WIDTH_RATIO);
            return Math.min(mExpandedViewWidth, Math.min(ratioWidthPx, availableWidthPx));
        }
        return mExpandedViewWidth;
    }

    @RailCollapseState
    int getRailCollapseStateForTesting() {
        return mCollapseController.getEffectiveRailCollapseState();
    }
}
