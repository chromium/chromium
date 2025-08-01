// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.graphics.Rect;
import android.graphics.RectF;
import android.hardware.display.DisplayManager;
import android.util.SparseArray;
import android.view.Window;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.Executor;

/** Interface to call unreleased Android APIs that are guarded by aconfig flags. */
@NullMarked
public interface AconfigFlaggedApiDelegate {
    /**
     * Calls the {@link android.app.ActivityManager#isTaskMoveAllowedOnDisplay} method if supported,
     * otherwise returns false.
     *
     * @param am {@link android.app.ActivityManager} on which the method should be called.
     */
    default boolean isTaskMoveAllowedOnDisplay(ActivityManager am, int displayId) {
        return false;
    }

    /**
     * Calls the {@link android.app.ActivityManager.AppTask#moveTaskTo} method if supported,
     * otherwise no-op.
     *
     * @param at {@link android.app.ActivityManager.AppTask} on which the method should be called.
     * @param displayId identifier of the target display.
     * @param bounds pixel-based target coordinates relative to the top-left corner of the target
     *     display.
     */
    default void moveTaskTo(AppTask at, int displayId, Rect bounds) {}

    // Helper interfaces and methods for calling the unreleased Display Topology Android API, used
    // within {@link ui.display.DisplayAndroidManager}.

    /** Interface that is used to subscribe to Display Topology Updates. */
    public interface DisplayTopologyListener {
        public void onDisplayTopologyChanged(SparseArray<RectF> absoluteBounds);
    }

    /** Checks if the display topology is available, based on the API level and Aconfig flags. */
    default boolean isDisplayTopologyAvailable() {
        return false;
    }

    /**
     * Calls the {@link android.hardware.display.DisplayTopology#getAbsoluteBounds()} method if
     * supported, otherwise returns {@code null}.
     *
     * @param displayManager {@link android.hardware.display.DisplayManager} from which Display
     *     Topology be will be obtained.
     * @return Map from logical display ID to the display's absolute bounds if method supported,
     *     otherwise {@code null}.
     */
    @Nullable
    default SparseArray<RectF> getAbsoluteBounds(DisplayManager displayManager) {
        return null;
    }

    /**
     * Calls the {@link android.hardware.display.DisplayTopology#registerTopologyListener(Executor,
     * Consumer<DisplayTopology> listener)} method if supported.
     *
     * @param displayManager {@link android.hardware.display.DisplayManager} on which the method
     *     should be called.
     * @param Executor {@link java.util.concurrent.Executor} The executor specifying the thread on
     *     which the callbacks will be invoked.
     * @param DisplayTopologyListener The listener to be notified of display topology updates
     *     through {@link DisplayTopologyListener#onDisplayTopologyChanged(SparseArray<RectF>} about
     *     every Display Topoloygy updates.
     */
    default void registerTopologyListener(
            DisplayManager displayManager,
            Executor executor,
            DisplayTopologyListener displayTopologyListener) {}

    /**
     * Calls the {@link android.view.WindowManager.LayoutParams#setKeyboardCaptureEnabled(boolean
     * hasCapture)} method if supported.
     *
     * @param window {@link android.view.Window} on which the method should be called.
     * @param hasCapture whether keyboard capture should be enabled or disabled.
     * @return boolean indicating whether the android API was invoked.
     */
    default boolean setKeyboardCaptureEnabled(Window window, boolean hasCapture) {
        return false;
    }
}
