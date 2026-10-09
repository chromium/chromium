// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;

/**
 * Immutable description of what the download toolbar button should display.
 *
 * <p>Mirrors the inputs of the desktop download bubble icon so behaviour can be compared
 * side-by-side: {@link #iconState} is {@code DownloadDisplay::IconState}, {@link #isActive} is
 * {@code DownloadDisplay::IconActive} and the remaining fields are {@code
 * DownloadDisplay::ProgressInfo} (see {@code chrome/browser/ui/download/download_display.h}).
 *
 * <p>Published by {@link DownloadToolbarButtonController} and consumed only by the toolbar button;
 * the download tray reads item data from the controller directly instead.
 */
@NullMarked
public final class DownloadToolbarButtonState {
    /** Which icon to draw. Mirrors desktop {@code DownloadDisplay::IconState}. */
    @IntDef({IconState.PROGRESS, IconState.COMPLETE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IconState {
        /** At least one download is in progress, pending or paused. */
        int PROGRESS = 0;

        /** No download is active; the button is lingering after the last one ended. */
        int COMPLETE = 1;
    }

    /** The state of a hidden button. Also the initial value before any download is observed. */
    public static final DownloadToolbarButtonState HIDDEN =
            new DownloadToolbarButtonState(
                    /* shouldShow= */ false,
                    IconState.COMPLETE,
                    /* isActive= */ false,
                    /* downloadCount= */ 0,
                    /* progressPercent= */ 0,
                    /* progressCertain= */ true);

    /** Whether the button should be shown at all. */
    public final boolean shouldShow;

    /** Which icon to draw. */
    public final @IconState int iconState;

    /**
     * Whether the icon should use the active (primary) colour rather than the default. While
     * downloading, false only if every active download is paused; after a download completes, true
     * for a short window until the user acts on the button. Matches desktop.
     */
    public final boolean isActive;

    /** Number of active downloads. Drives the badge when greater than one. */
    public final int downloadCount;

    /**
     * Aggregate completion of all active downloads whose size is known, in [0, 100]. Zero when none
     * has a known size.
     */
    public final int progressPercent;

    /**
     * Whether the final size of every active download is known. When false the button should show
     * indeterminate progress.
     */
    public final boolean progressCertain;

    /**
     * Creates a state snapshot. Each parameter initialises the field of the same name; see the
     * field documentation for semantics. Callers that only need a hidden button should use {@link
     * #HIDDEN} rather than constructing one.
     *
     * @param shouldShow Whether the button should be shown at all.
     * @param iconState Which icon to draw.
     * @param isActive Whether to use the active colour rather than the default.
     * @param downloadCount Number of active downloads.
     * @param progressPercent Aggregate progress of active downloads with a known size, in [0, 100].
     * @param progressCertain Whether the size of every active download is known.
     */
    public DownloadToolbarButtonState(
            boolean shouldShow,
            @IconState int iconState,
            boolean isActive,
            int downloadCount,
            int progressPercent,
            boolean progressCertain) {
        this.shouldShow = shouldShow;
        this.iconState = iconState;
        this.isActive = isActive;
        this.downloadCount = downloadCount;
        this.progressPercent = progressPercent;
        this.progressCertain = progressCertain;
    }

    @Override
    public boolean equals(@Nullable Object obj) {
        if (this == obj) return true;
        if (!(obj instanceof DownloadToolbarButtonState other)) return false;
        return shouldShow == other.shouldShow
                && iconState == other.iconState
                && isActive == other.isActive
                && downloadCount == other.downloadCount
                && progressPercent == other.progressPercent
                && progressCertain == other.progressCertain;
    }

    @Override
    public int hashCode() {
        return Objects.hash(
                shouldShow, iconState, isActive, downloadCount, progressPercent, progressCertain);
    }
}
