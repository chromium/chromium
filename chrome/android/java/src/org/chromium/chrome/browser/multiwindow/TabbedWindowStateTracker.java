// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.multiwindow;

import android.graphics.Rect;
import android.view.Display;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.ui.browser_window.ChromeAndroidTaskFeature;

/** A {@link ChromeAndroidTaskFeature} that tracks and persists window state. */
@NullMarked
public class TabbedWindowStateTracker implements ChromeAndroidTaskFeature {
    private final int mWindowId;

    /**
     * Creates and returns a {@link TabbedWindowStateTracker}.
     *
     * @param windowId The id of the ChromeTabbedActivity window whose state will be tracked.
     * @return The {@link TabbedWindowStateTracker} instance.
     */
    public static @Nullable TabbedWindowStateTracker create(int windowId) {
        if (!MultiWindowUtils.isMultiInstanceApi31Enabled()
                || !ChromeFeatureList.isEnabled(ChromeFeatureList.SESSION_RESTORE_AFTER_CRASH)) {
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
        saveWindowBounds(initInfo.displayId, initInfo.boundsInPx);
    }

    @Override
    public void onFeatureRemoved() {}

    @Override
    public void onTaskBoundsChanged(Rect newBoundsInDp) {
        // TODO: Implement this method.
    }

    @Override
    public void onTaskVisibilityChanged(boolean isVisible) {
        ChromeMultiInstancePersistentStore.writeIsVisible(mWindowId, isVisible);
    }

    private void saveWindowBounds(int displayId, Rect boundsInPx) {
        // Only persist bounds if the task is on the primary display. This is to avoid the persisted
        // bounds from being incorrectly used to start an activity in a different non-primary
        // display than the display on which the bounds for the task were originally captured.
        Rect bounds = displayId == Display.DEFAULT_DISPLAY ? boundsInPx : new Rect();
        ChromeMultiInstancePersistentStore.writeBounds(mWindowId, bounds);
    }
}
