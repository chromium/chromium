// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.view.MotionEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.list_view.ListViewTouchTracker;
import org.chromium.ui.util.MotionEventUtils;

/** Utility to help build {@link TabClosureParams}. */
@NullMarked
public final class TabClosureParamsUtils {

    private TabClosureParamsUtils() {}

    /**
     * Whether tab / tab group closure should allow undo, depending on the touch events received by
     * a {@link android.widget.ListView} item.
     *
     * @param listViewTouchTracker a {@link ListViewTouchTracker} that records touch events.
     * @return true if undo is allowed.
     * @see org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView
     */
    public static boolean shouldAllowUndo(@Nullable ListViewTouchTracker listViewTouchTracker) {
        if (listViewTouchTracker == null) {
            return true;
        }

        return shouldAllowUndo(listViewTouchTracker.getLastSingleTapUp());
    }

    /**
     * Whether tab / tab group closure should allow undo, depending on the {@link MotionEventInfo}
     * that triggered the closure.
     *
     * @param triggeringMotion {@link MotionEventInfo} that triggered the closure.
     * @return true if undo is allowed.
     */
    public static boolean shouldAllowUndo(@Nullable MotionEventInfo triggeringMotion) {
        if (triggeringMotion == null) {
            return true;
        }

        if (triggeringMotion.toolType.length == 0) {
            return true;
        }

        // Allow undo as long as the triggering motion was *not* from a mouse.
        return !MotionEventUtils.isPointerEvent(
                triggeringMotion.source, triggeringMotion.toolType[0]);
    }

    /**
     * Whether tab / tab group closure should allow undo, depending on the button state of the
     * "down" motion that initiated the closure.
     *
     * @param downMotionButtonState the "down" motion's {@link MotionEvent#getButtonState()}
     * @return true if undo is allowed.
     */
    public static boolean shouldAllowUndo(int downMotionButtonState) {
        return downMotionButtonState == MotionEventUtils.MOTION_EVENT_BUTTON_NONE;
    }
}
