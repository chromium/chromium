// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Tracks whether the drag pointer is inside a drag delegate's region, and runs enter/exit effects
 * when it crosses the boundary.
 *
 * <p>The region is not expressed here. The owner evaluates its own predicate and reports the
 * result; this class only turns a sequence of boolean readings into edges.
 *
 * <p>Readings come exclusively from {@code ACTION_DRAG_LOCATION}, the only continuous drag event
 * carrying coordinates. {@code ACTION_DRAG_ENTERED} and {@code ACTION_DRAG_EXITED} are delivered
 * with x, y and ClipData zeroed out, so they cannot be evaluated against a region. {@code
 * ACTION_DRAG_EXITED} is still meaningful as an edge, though - it is the only notice that the
 * pointer left the window, after which no further LOCATION arrives - so it is reported separately
 * via {@link #onExitedContainer()} and forces the outside state without consulting a predicate.
 *
 * <p>{@code ACTION_DROP} requires no tracker notification: the drop itself resolves the drag, and
 * {@code ACTION_DRAG_ENDED} immediately follows to reset state via {@link #onDragEnded()}.
 */
@NullMarked
class VerticalTabDragRegionTracker {

    @IntDef({State.NO_DRAG, State.OUTSIDE, State.INSIDE})
    @Retention(RetentionPolicy.SOURCE)
    @interface State {
        /** No drag session is in progress. */
        int NO_DRAG = 0;

        /** A drag is in progress and the pointer is outside the region. */
        int OUTSIDE = 1;

        /** A drag is in progress and the pointer is inside the region. */
        int INSIDE = 2;
    }

    private final Runnable mOnEnterRegion;
    private final Runnable mOnExitRegion;

    private @State int mState = State.NO_DRAG;

    /**
     * @param onEnterRegion Run when the pointer crosses into the region.
     * @param onExitRegion Run when the pointer crosses out of the region.
     */
    VerticalTabDragRegionTracker(Runnable onEnterRegion, Runnable onExitRegion) {
        mOnEnterRegion = onEnterRegion;
        mOnExitRegion = onExitRegion;
    }

    /**
     * Reports {@code ACTION_DRAG_STARTED} with the initial region reading.
     *
     * <p>Establishes the initial state without firing transition callbacks; the caller's drag-start
     * handling establishes the initial presentation for that state.
     */
    void onDragStarted(boolean isInsideRegion) {
        mState = isInsideRegion ? State.INSIDE : State.OUTSIDE;
    }

    /**
     * Reports a predicate reading taken at a coordinate-bearing event.
     *
     * @param isInsideRegion Whether the event's point is inside the owner's region.
     */
    void onLocation(boolean isInsideRegion) {
        if (mState == State.NO_DRAG) return;
        advanceTo(isInsideRegion ? State.INSIDE : State.OUTSIDE);
    }

    /**
     * Reports {@code ACTION_DRAG_EXITED}. If a drag session is active, forces the outside state and
     * runs the exit effects if the pointer was inside. Ignored when in {@link State#NO_DRAG}.
     */
    void onExitedContainer() {
        if (mState == State.NO_DRAG) return;
        advanceTo(State.OUTSIDE);
    }

    /** Reports {@code ACTION_DRAG_ENDED}. A silent reset; the end handling runs its own effects. */
    void onDragEnded() {
        mState = State.NO_DRAG;
    }

    private void advanceTo(@State int newState) {
        if (newState == mState) return;

        @State int oldState = mState;
        mState = newState;

        if (newState == State.INSIDE) {
            mOnEnterRegion.run();
        } else if (oldState == State.INSIDE) {
            mOnExitRegion.run();
        }
    }
}
