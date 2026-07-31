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

/** Controller for managing the vertical tab rail's expanded/collapsed state. */
@NullMarked
class VerticalTabRailCollapseController {
    /** Listener for changes to rail collapse state. */
    interface RailCollapseListener {
        /**
         * Called when the rail collapse state change is requested by user interaction.
         *
         * @param currentState The current {@link RailCollapseState}.
         * @param targetState The target {@link RailCollapseState}.
         */
        void onRailCollapseStateChangeRequestedByUser(
                @RailCollapseState int currentState, @RailCollapseState int targetState);
    }

    /** Returns whether the given state is an expanded state. */
    static boolean isExpanded(@RailCollapseState int state) {
        return state == RailCollapseState.EXPANDED
                || state == RailCollapseState.EXPANDED_FOR_HOVERING;
    }

    private final @Nullable Callback<@RailCollapseState Integer> mSetRailCollapseStateCallback;
    private final SettableNonNullObservableSupplier<@RailCollapseState Integer>
            mRailCollapseStateSupplier;

    private @Nullable RailCollapseListener mRailCollapseListener;
    private @RailCollapseState int mRailCollapseStateByUser;
    private boolean mIsCollapseButtonEnabled = true;

    VerticalTabRailCollapseController(
            @Nullable Callback<@RailCollapseState Integer> setRailCollapseStateCallback) {
        mSetRailCollapseStateCallback = setRailCollapseStateCallback;
        mRailCollapseStateByUser =
                VerticalTabUtils.isRailCollapsedFromSharedPref()
                        ? RailCollapseState.COLLAPSED
                        : RailCollapseState.EXPANDED;
        mRailCollapseStateSupplier = ObservableSuppliers.createNonNull(mRailCollapseStateByUser);
    }

    /** Cleans up observers and listeners. */
    void destroy() {
        mRailCollapseListener = null;
    }

    /** Sets the listener for rail collapse state changes. */
    void setRailCollapseListener(@Nullable RailCollapseListener listener) {
        mRailCollapseListener = listener;
    }

    /** Toggles the rail collapse state between expanded and collapsed by user. */
    void toggleCollapseState() {
        if (!mIsCollapseButtonEnabled) return;

        @RailCollapseState
        int targetState =
                mRailCollapseStateByUser == RailCollapseState.EXPANDED
                        ? RailCollapseState.COLLAPSED
                        : RailCollapseState.EXPANDED;
        RecordHistogram.recordBooleanHistogram(
                "Android.VerticalTabs.RailCollapsed", targetState == RailCollapseState.COLLAPSED);
        VerticalTabUtils.setRailCollapsedInSharedPref(targetState == RailCollapseState.COLLAPSED);

        requestRailCollapseStateChangeByUser(mRailCollapseStateByUser, targetState);
    }

    /**
     * Expands or collapses the rail on hover by user.
     *
     * @param targetState The target {@link RailCollapseState}.
     */
    void expandOrCollapseOnHover(@RailCollapseState int targetState) {
        if (!mIsCollapseButtonEnabled) return;

        if (mRailCollapseStateByUser != RailCollapseState.EXPANDED_FOR_HOVERING
                && mRailCollapseStateByUser != RailCollapseState.COLLAPSED) {
            return;
        }

        @RailCollapseState int currentState = mRailCollapseStateSupplier.get();
        requestRailCollapseStateChangeByUser(currentState, targetState);
    }

    /** Sets the user-selected rail collapse state. */
    void setRailCollapseStateByUser(@RailCollapseState int railCollapseState) {
        mRailCollapseStateByUser = railCollapseState;
    }

    /**
     * Returns the user-requested rail collapse state preference ({@link RailCollapseState#EXPANDED}
     * or {@link RailCollapseState#COLLAPSED}).
     *
     * <p>Note: This is the user's intent, not necessarily the final effective state, as window
     * width constraints may temporarily force the rail to collapse when the window is narrow.
     */
    @RailCollapseState
    int getRailCollapseStateByUser() {
        return mRailCollapseStateByUser;
    }

    /**
     * Returns the final effective rail collapse state, applying window width constraints.
     *
     * @param isNarrow True if the current window width is below the threshold for expanding.
     * @return {@link RailCollapseState#COLLAPSED} if {@code isNarrow} is true; otherwise returns
     *     the user preference from {@link #getRailCollapseStateByUser()}.
     */
    @RailCollapseState
    int getEffectiveRailCollapseState(boolean isNarrow) {
        return isNarrow ? RailCollapseState.COLLAPSED : mRailCollapseStateByUser;
    }

    /** Updates the rail collapse state supplier value. */
    void setRailCollapseStateSupplierValue(@RailCollapseState int railCollapseState) {
        mRailCollapseStateSupplier.set(railCollapseState);
    }

    /** Returns the supplier for the current rail collapse state. */
    NonNullObservableSupplier<@RailCollapseState Integer> getRailCollapseStateSupplier() {
        return mRailCollapseStateSupplier;
    }

    /** Sets whether the collapse button is enabled. */
    void setCollapseButtonEnabled(boolean enabled) {
        mIsCollapseButtonEnabled = enabled;
    }

    /** Returns whether the collapse button is enabled. */
    boolean isCollapseButtonEnabled() {
        return mIsCollapseButtonEnabled;
    }

    /** Applies the rail collapse state by invoking the callback. */
    void dispatchRailCollapseStateUpdate(@RailCollapseState int railCollapseState) {
        if (mSetRailCollapseStateCallback != null) {
            mSetRailCollapseStateCallback.onResult(railCollapseState);
        }
    }

    /**
     * Requests a change in rail collapse state by user interaction.
     *
     * @param currentState The current {@link RailCollapseState}.
     * @param targetState The target {@link RailCollapseState}.
     */
    void requestRailCollapseStateChangeByUser(
            @RailCollapseState int currentState, @RailCollapseState int targetState) {
        if (currentState == targetState) return;
        // Call setRailCollapseStateByUser before notifying listeners so
        // getEffectiveRailCollapseState() reflects the new state.
        setRailCollapseStateByUser(targetState);

        // If SideUiCoordinator is listening, delegate the state change request so it can update
        // user state and trigger Side UI transitions. Otherwise, fall back to setting state
        // directly.
        if (mRailCollapseListener != null) {
            mRailCollapseListener.onRailCollapseStateChangeRequestedByUser(
                    currentState, targetState);
        } else {
            dispatchRailCollapseStateUpdate(targetState);
        }
    }

    /** Returns the registered listener for testing. */
    @Nullable RailCollapseListener getRailCollapseListenerForTesting() {
        return mRailCollapseListener;
    }
}
