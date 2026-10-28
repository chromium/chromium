// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Objects;

/** Progress of a swipe or slide transition from one pane to another. */
@NullMarked
final class PaneTransitionProgress {
    public final @PaneId int fromPaneId;
    public final @PaneId int toPaneId;

    /** Fraction of the transition completed, from 0.0 to 1.0. */
    public final float progress;

    /** Whether the panes move left as {@link #progress} grows, as for a left swipe. */
    public final boolean isSwipeLeft;

    PaneTransitionProgress(
            @PaneId int fromPaneId, @PaneId int toPaneId, float progress, boolean isSwipeLeft) {
        this.fromPaneId = fromPaneId;
        this.toPaneId = toPaneId;
        this.progress = progress;
        this.isSwipeLeft = isSwipeLeft;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof PaneTransitionProgress other)) return false;
        return fromPaneId == other.fromPaneId
                && toPaneId == other.toPaneId
                && Float.compare(progress, other.progress) == 0
                && isSwipeLeft == other.isSwipeLeft;
    }

    @Override
    public int hashCode() {
        return Objects.hash(fromPaneId, toPaneId, progress, isSwipeLeft);
    }
}
