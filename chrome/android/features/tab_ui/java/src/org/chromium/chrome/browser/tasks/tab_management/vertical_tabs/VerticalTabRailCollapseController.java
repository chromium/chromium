// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.vertical_tabs.VerticalTabListProperties.RailCollapseState;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils.WindowWidthBoundary;

/**
 * Controller for managing the vertical tab rail's expanded/collapsed state.
 *
 * <p>This is the single source of truth for the rail collapse state. It keeps three independent
 * inputs:
 *
 * <ul>
 *   <li>The user preference, {@link RailCollapseState#EXPANDED} or {@link
 *       RailCollapseState#COLLAPSED}, changed by {@link #toggleCollapseState()} and persisted in
 *       shared prefs.
 *   <li>Whether the pointer is hovering the rail, from {@link #expandOrCollapseOnHover(int)}.
 *   <li>The window width constraint, from {@link #setWindowWidthBoundary(int)}, supplied by {@link
 *       VerticalTabsSideUiCoordinator}.
 * </ul>
 *
 * <p>Everything else (the effective {@link #getEffectiveRailCollapseState()} and whether the
 * collapse button is enabled) is derived from those inputs and pushed to the view layer by {@link
 * #applyEffectiveState()}.
 */
@NullMarked
class VerticalTabRailCollapseController {
    /**
     * Delegate that carries out a user-requested rail state change. At most one may be registered;
     * when none is, the controller applies the change itself.
     */
    interface RailStateChangeDelegate {
        /**
         * Called when the user requested a change that moves the effective rail state. The delegate
         * is responsible for driving the resulting Side UI update, which ends up calling {@link
         * #applyEffectiveState()}.
         */
        void handleUserRequestedStateChange();
    }

    private final Callback<@RailCollapseState Integer> mSetRailCollapseStateCallback;
    private final Callback<Boolean> mSetCollapseButtonEnabledCallback;
    private final SettableNonNullObservableSupplier<@RailCollapseState Integer>
            mRailCollapseStateSupplier;
    private @Nullable RailStateChangeDelegate mRailStateChangeDelegate;

    // The user preference. Hovering and window width constraints never change it.
    private boolean mIsCollapsedByUser;

    // Whether the pointer is currently hovering the rail. Only expands the rail while the user
    // preference is COLLAPSED and the window is wide enough.
    private boolean mIsHoverExpanded;

    // Whether the rail is forced to collapse due to narrow window constraints or insufficient
    // available width. When true, the rail collapses and the collapse button is disabled,
    // regardless of the other inputs.
    private boolean mIsForcedCollapsed;

    /**
     * @param setRailCollapseStateCallback Applies the effective {@link RailCollapseState} to the
     *     view layer.
     * @param setCollapseButtonEnabledCallback Applies the derived collapse button enabled state to
     *     the view layer.
     */
    VerticalTabRailCollapseController(
            Callback<@RailCollapseState Integer> setRailCollapseStateCallback,
            Callback<Boolean> setCollapseButtonEnabledCallback) {
        mSetRailCollapseStateCallback = setRailCollapseStateCallback;
        mSetCollapseButtonEnabledCallback = setCollapseButtonEnabledCallback;
        mIsCollapsedByUser = VerticalTabUtils.isRailCollapsedFromSharedPref();
        mRailCollapseStateSupplier =
                ObservableSuppliers.createNonNull(getEffectiveRailCollapseState());
    }

    /** Cleans up the registered delegate. */
    void destroy() {
        mRailStateChangeDelegate = null;
    }

    /** Sets the delegate that carries out user-requested rail state changes. */
    void setRailStateChangeDelegate(@Nullable RailStateChangeDelegate delegate) {
        mRailStateChangeDelegate = delegate;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Inputs.
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /** Toggles the user preference between expanded and collapsed. */
    void toggleCollapseState() {
        if (isForcedCollapsed()) return;

        @RailCollapseState int previousState = getEffectiveRailCollapseState();

        mIsCollapsedByUser = !mIsCollapsedByUser;
        VerticalTabUtils.setRailCollapsedInSharedPref(mIsCollapsedByUser);
        RecordHistogram.recordBooleanHistogram(
                "Android.VerticalTabs.RailCollapsed", mIsCollapsedByUser);

        // An explicit toggle overrides the current hover; the rail expands on hover again only
        // after the pointer leaves and re-enters.
        mIsHoverExpanded = false;
        requestEffectiveStateChangeByUser(previousState);
    }

    /**
     * Records whether the pointer is hovering the rail. Hover is tracked even while the rail cannot
     * expand (narrow window), so that a hover exit is never missed.
     *
     * @param targetState {@link RailCollapseState#EXPANDED_FOR_HOVERING} on hover enter, {@link
     *     RailCollapseState#COLLAPSED} on hover exit.
     */
    void expandOrCollapseOnHover(@RailCollapseState int targetState) {
        @RailCollapseState int previousState = getEffectiveRailCollapseState();
        mIsHoverExpanded = targetState == RailCollapseState.EXPANDED_FOR_HOVERING;
        requestEffectiveStateChangeByUser(previousState);
    }

    /**
     * Updates the user collapse preference from a manual resize. Does not apply the new state, as
     * the resize flow drives its own Side UI update.
     *
     * @param isCollapsed Whether the rail should be collapsed.
     */
    void setCollapsedByUserFromResize(boolean isCollapsed) {
        if (mIsCollapsedByUser == isCollapsed) return;
        mIsCollapsedByUser = isCollapsed;
        VerticalTabUtils.setRailCollapsedInSharedPref(isCollapsed);
        // A resize is an explicit request, so like a toggle it overrides the current hover.
        mIsHoverExpanded = false;
    }

    /**
     * Feeds the window width constraint. This is the only rail state input owned by {@link
     * VerticalTabsSideUiCoordinator}; the narrow-window policy, the resulting effective state and
     * the collapse button state are all derived here. Applies the new effective state if the
     * constraint changed.
     *
     * @param boundary The {@link WindowWidthBoundary} for the current window/available width.
     */
    void setWindowWidthBoundary(@WindowWidthBoundary int boundary) {
        boolean isForcedCollapsed = boundary <= WindowWidthBoundary.FORCED_COLLAPSED;
        if (mIsForcedCollapsed == isForcedCollapsed) return;
        mIsForcedCollapsed = isForcedCollapsed;
        applyEffectiveState();
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Derived state.
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /** Returns the final effective rail collapse state derived from all three inputs. */
    @RailCollapseState
    int getEffectiveRailCollapseState() {
        if (mIsForcedCollapsed) return RailCollapseState.COLLAPSED;
        if (!mIsCollapsedByUser) return RailCollapseState.EXPANDED;
        return mIsHoverExpanded
                ? RailCollapseState.EXPANDED_FOR_HOVERING
                : RailCollapseState.COLLAPSED;
    }

    /**
     * Returns whether the rail is currently expanded only because the pointer is hovering it. The
     * rail renders expanded over the web contents in this state, but keeps reserving its collapsed
     * width, so the web contents are neither resized nor repositioned.
     */
    boolean isHoverExpanded() {
        return getEffectiveRailCollapseState() == RailCollapseState.EXPANDED_FOR_HOVERING;
    }

    /**
     * Returns whether the window is too narrow to host an expanded rail. While this is true the
     * rail stays collapsed regardless of the other inputs, so the collapse button is disabled and
     * the rail cannot be resized.
     */
    boolean isForcedCollapsed() {
        return mIsForcedCollapsed;
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////
    // Applying and publishing.
    ///////////////////////////////////////////////////////////////////////////////////////////////

    /**
     * Pushes the effective rail collapse state and the derived collapse button enabled state to the
     * view layer, then publishes the applied state to {@link #getRailCollapseStateSupplier()}.
     * Callers should invoke this whenever an input may have been applied out-of-band (e.g. after
     * Side UI specs changed).
     */
    void applyEffectiveState() {
        @RailCollapseState int effectiveState = getEffectiveRailCollapseState();
        mSetRailCollapseStateCallback.onResult(effectiveState);
        mSetCollapseButtonEnabledCallback.onResult(!isForcedCollapsed());
        // Published after the view layer is updated so observers see a consistent state.
        mRailCollapseStateSupplier.set(effectiveState);
    }

    /** Returns the supplier for the applied rail collapse state. */
    NonNullObservableSupplier<@RailCollapseState Integer> getRailCollapseStateSupplier() {
        return mRailCollapseStateSupplier;
    }

    /**
     * Propagates a user-driven input change, if it moved the effective state.
     *
     * @param previousState The effective {@link RailCollapseState} before the input changed.
     */
    private void requestEffectiveStateChangeByUser(@RailCollapseState int previousState) {
        @RailCollapseState int targetState = getEffectiveRailCollapseState();
        if (previousState == targetState) return;

        // If a delegate is registered, hand the change over so it can trigger the Side UI
        // transition, which ends up applying the effective state. Otherwise, fall back to applying
        // the effective state directly.
        if (mRailStateChangeDelegate != null) {
            mRailStateChangeDelegate.handleUserRequestedStateChange();
        } else {
            applyEffectiveState();
        }
    }

    /**
     * Returns whether the user asked for the rail to be collapsed.
     *
     * <p>Note: This is the user's intent, not necessarily the final effective state, as hovering or
     * window width constraints may temporarily override it. Use {@link
     * #getEffectiveRailCollapseState()} for the state that should be rendered.
     */
    boolean isCollapsedByUserForTesting() {
        return mIsCollapsedByUser;
    }
}
