// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.graphics.Rect;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;

/** A {@link ChromeAndroidTaskFeature} that tracks and persists window state. */
@NullMarked
public class TabbedWindowStateTracker implements ChromeAndroidTaskFeature {
    private final int mWindowId;

    private int mLastRecordedWidthDp;

    /**
     * Creates and returns a {@link TabbedWindowStateTracker}.
     *
     * @param windowId The id of the ChromeTabbedActivity window whose state will be tracked.
     * @return The {@link TabbedWindowStateTracker} instance.
     */
    public static @Nullable TabbedWindowStateTracker create(int windowId) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()
                || !MultiWindowUtils.isSessionRestoreAfterCrashEnabled()) {
            return null;
        }
        return new TabbedWindowStateTracker(windowId);
    }

    private TabbedWindowStateTracker(int windowId) {
        mWindowId = windowId;
    }

    @Override
    public void onAddedToTask(InitInfo initInfo) {
        // Save initial window state.
        ChromeMultiInstancePersistentStore.writeIsVisible(mWindowId, initInfo.isVisible);

        // Record initial window width.
        recordWindowWidth(initInfo.boundsInDp.width());
    }

    @Override
    public void onFeatureRemoved() {}

    @Override
    public void onTaskBoundsChanged(int displayId, Rect newBoundsInDp, Rect newBoundsInPx) {
        recordWindowWidth(newBoundsInDp.width());
    }

    @Override
    public void onTaskVisibilityChanged(boolean isVisible) {
        ChromeMultiInstancePersistentStore.writeIsVisible(mWindowId, isVisible);
    }

    private void recordWindowWidth(int widthDp) {
        if (widthDp == mLastRecordedWidthDp) return;
        mLastRecordedWidthDp = widthDp;
        RecordHistogram.recordCustomCountHistogram("Android.WindowWidth", widthDp, 1, 10000, 50);
    }
}
