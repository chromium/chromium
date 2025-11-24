// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.app.ActivityManager;
import android.app.ActivityManager.AppTask;
import android.content.Context;
import android.content.Context.BindServiceFlags;
import android.content.ServiceConnection;
import android.graphics.Rect;
import android.graphics.RectF;
import android.hardware.display.DisplayManager;
import android.util.Pair;
import android.util.SparseArray;
import android.view.Display;
import android.view.View;
import android.view.ViewConfiguration;
import android.view.Window;
import android.webkit.WebViewDelegate;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.concurrent.Executor;

/** Interface to call unreleased Android APIs that are guarded by aconfig flags. */
@NullMarked
public interface AconfigFlaggedApiDelegate {
    /**
     * The default text cursor blink interval in milliseconds. This value is used as a fallback in
     * public Chromium builds where the real implementation is not available.
     */
    int DEFAULT_TEXT_CURSOR_BLINK_INTERVAL_MS = 500;

    /**
     * Prefer to use this to get a instance instead of calling ServiceLoaderUtil. If possible, avoid
     * caching the return value in member or global variables as it allows more compile time
     * optimizations.
     */
    static @Nullable AconfigFlaggedApiDelegate getInstance() {
        return ServiceLoaderUtil.maybeCreate(AconfigFlaggedApiDelegate.class);
    }

    static void setInstanceForTesting(AconfigFlaggedApiDelegate testInstance) {
        ServiceLoaderUtil.setInstanceForTesting(AconfigFlaggedApiDelegate.class, testInstance);
    }

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

    /**
     * Calls the {@link android.app.ActivityManager.AppTask#moveTaskTo} method if supported,
     * otherwise no-op. Trigger callback when this succeeds or fails.
     *
     * @param at {@link android.app.ActivityManager.AppTask} on which the method should be called.
     * @param displayId identifier of the target display.
     * @param bounds pixel-based target coordinates relative to the top-left corner of the target
     *     display.
     * @return A promise fulfilled with a pair of the actual target display id and actual updated
     *     bounds.
     */
    default Promise<Pair<Integer, Rect>> moveTaskToWithPromise(
            AppTask at, int displayId, Rect bounds) {
        return Promise.fulfilled(Pair.create(Display.INVALID_DISPLAY, new Rect()));
    }

    // Helper interfaces and methods for calling the unreleased Display Topology Android API, used
    // within {@link ui.display.DisplayAndroidManager}.

    /** Interface that is used to subscribe to Display Topology Updates. */
    interface DisplayTopologyListener {
        void onDisplayTopologyChanged(SparseArray<RectF> absoluteBounds);
    }

    /**
     * Checks if the display topology is available, based on the API level, Aconfig flags and
     * Display Topology state.
     *
     * @param displayManager {@link android.hardware.display.DisplayManager} from which Display
     *     Topology will be obtained.
     */
    default boolean isDisplayTopologyAvailable(DisplayManager displayManager) {
        return false;
    }

    /**
     * Calls the {@link android.hardware.display.DisplayTopology#getAbsoluteBounds()} method if
     * supported, otherwise returns {@code null}.
     *
     * @param displayManager {@link android.hardware.display.DisplayManager} from which Display
     *     Topology will be obtained.
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

    /** Returns whether rebindService() is available or not. */
    default boolean isUpdateServiceBindingApiAvailable() {
        return false;
    }

    /**
     * Calls the {@link android.content.Context#rebindService(ServiceConnection, BindServiceFlags)}
     * method if supported.
     *
     * @param context {@link android.content.Context} on which the method should be called.
     * @param connection {@link android.content.ServiceConnection} The connection to rebind.
     * @param flags {@link android.content.Context.BindServiceFlags} The flags to use when binding.
     */
    default void rebindService(
            Context context, ServiceConnection connection, BindServiceFlags flags) {}

    /** Returns the {@link BindingRequestQueue} if supported, otherwise returns {@code null}. */
    default @Nullable BindingRequestQueue getBindingRequestQueue() {
        return null;
    }

    /**
     * Calls the {@link android.view.ViewConfiguration#getTextCursorBlinkIntervalMillis()} method if
     * an implementation is available, otherwise returns a default value.
     *
     * @param viewConfiguration The {@link android.view.ViewConfiguration} instance to use.
     */
    default int getTextCursorBlinkInterval(ViewConfiguration viewConfiguration) {
        return DEFAULT_TEXT_CURSOR_BLINK_INTERVAL_MS;
    }

    /**
     * Calls {@link android.view.View#requestRectangleOnScreen(Rect, boolean, int)} if supported,
     * with focus type of {@link android.view.View#RECTANGLE_ON_SCREEN_REQUEST_SOURCE_INPUT_FOCUS}.
     *
     * @param view view on which the method should be called
     * @param boundsInView the rect to request on screen, in coordinates relative to {@code view}
     * @return whether the Android API was invoked
     */
    default boolean requestInputFocusOnScreen(View view, Rect boundsInView) {
        // TODO(crbug.com/450540343) inline internal delegate into callsites when API 36.1 releases.
        return false;
    }

    /**
     * Calls {@link View#requestRectangleOnScreen(Rect, boolean, int)} if supported, with focus type
     * of {@link View#RECTANGLE_ON_SCREEN_REQUEST_SOURCE_TEXT_CURSOR}.
     *
     * @param view view on which the method should be called
     * @param boundsInView the rect to request on screen, in coordinates relative to {@code view}
     * @return whether the Android API was invoked
     */
    default boolean requestTextCursorOnScreen(View view, Rect boundsInView) {
        // TODO(crbug.com/450540343) inline internal delegate into callsites when API 36.1 releases.
        return false;
    }

    /**
     * Checks if the Selection Action Menu Client is available, based on the API level and Aconfig
     * flags. If the client is available, this method returns it wrapped in a {@code
     * SelectionActionMenuClientWrapper}. This does not check if the client has been overridden and
     * calling this method may return the default client. If the client is unavailable, this method
     * returns null.
     *
     * @param delegate the WebViewDelegate used to get the client object.
     */
    default @Nullable SelectionActionMenuClientWrapper getSelectionActionMenuClient(
            WebViewDelegate delegate) {
        return null;
    }
}
