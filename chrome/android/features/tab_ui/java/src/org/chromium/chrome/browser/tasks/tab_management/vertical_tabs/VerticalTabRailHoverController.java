// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management.vertical_tabs;

import android.content.SharedPreferences.OnSharedPreferenceChangeListener;
import android.view.DragEvent;
import android.view.InputDevice;
import android.view.MotionEvent;
import android.view.View;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.ui.vertical_tabs.VerticalTabUtils;
import org.chromium.ui.base.WindowAndroid;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.function.BooleanSupplier;

/**
 * Tracks the mouse pointer relative to the vertical tab rail and drives its expand-on-hover state.
 *
 * <p>Each pointer event either applies the pointer state to the rail, when the position is known,
 * or only records it, when hover events stop reaching the rail (mouse press, drag, or a window such
 * as a context menu covering the pointer). Note that collapses are held back while a context menu
 * shows and while the activity is not the top resumed one.
 */
@NullMarked
class VerticalTabRailHoverController
        implements VerticalTabRailLayout.RailEventListener, WindowAndroid.ActivityStateObserver {
    /** Where the mouse pointer is relative to the rail, as last observed from pointer events. */
    @IntDef({PointerState.OUTSIDE, PointerState.INSIDE, PointerState.INSIDE_UNCONFIRMED})
    @Retention(RetentionPolicy.SOURCE)
    @interface PointerState {
        /** The pointer is outside the rail bounds. */
        int OUTSIDE = 0;

        /** The pointer is inside the rail bounds. */
        int INSIDE = 1;

        /**
         * The pointer was last seen inside the rail, but pointer events stopped reaching the rail
         * (e.g. a mouse button is pressed, a drag is in progress, or another window covers the
         * pointer). The next pointer event confirms where it is.
         */
        int INSIDE_UNCONFIRMED = 2;
    }

    /**
     * How long to wait, after a context menu is dismissed, for a hover event confirming that the
     * pointer is still over the rail. This gives the user a chance to keep the rail expanded, e.g.
     * by dismissing the menu with a click on the rail, without a collapse then re-expand.
     */
    static final long MENU_DISMISS_TIMEOUT_MS = 500;

    private final VerticalTabRailLayout mRailView;
    private final VerticalTabRailCollapseController mCollapseController;
    private final WindowAndroid mWindowAndroid;
    private final BooleanSupplier mIsContextMenuShowingSupplier;
    // Collapses the rail if no hover event confirmed the pointer over it in time after a context
    // menu was dismissed. Any pointer event that changes the state cancels this.
    private final Runnable mMenuDismissTimeout = () -> applyPointerState(PointerState.OUTSIDE);

    // Reused by containsRawPoint() to avoid allocating on every hover event.
    private final int[] mTempLocation = new int[2];

    // Where the pointer is relative to the rail, as last observed from pointer events.
    private @PointerState int mPointerState = PointerState.OUTSIDE;

    // Held as a field because SharedPreferences only keeps weak references to its listeners.
    private final OnSharedPreferenceChangeListener mPrefsListener =
            (sharedPreferences, key) -> {
                if (ChromePreferenceKeys.VERTICAL_TABS_EXPAND_ON_HOVER.equals(key)) {
                    onExpandOnHoverSettingChanged();
                }
            };

    /**
     * Starts observing the pointer events dispatched to {@code railView}.
     *
     * @param railView The rail to observe.
     * @param collapseController Receives whether the rail is hovered.
     * @param windowAndroid The window hosting the rail, used to observe whether its activity is the
     *     top resumed activity.
     * @param isContextMenuShowingSupplier Returns whether any rail context menu is showing.
     */
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    VerticalTabRailHoverController(
            VerticalTabRailLayout railView,
            VerticalTabRailCollapseController collapseController,
            WindowAndroid windowAndroid,
            BooleanSupplier isContextMenuShowingSupplier) {
        mRailView = railView;
        mCollapseController = collapseController;
        mWindowAndroid = windowAndroid;
        mIsContextMenuShowingSupplier = isContextMenuShowingSupplier;
        mRailView.setRailEventListener(this);
        mWindowAndroid.addActivityStateObserver(this);
        ContextUtils.getAppSharedPreferences()
                .registerOnSharedPreferenceChangeListener(mPrefsListener);
    }

    /** Stops observing the rail. */
    @SuppressWarnings("UseSharedPreferencesManagerFromChromeCheck")
    void destroy() {
        cancelMenuDismissTimeout();
        mRailView.setRailEventListener(null);
        mWindowAndroid.removeActivityStateObserver(this);
        ContextUtils.getAppSharedPreferences()
                .unregisterOnSharedPreferenceChangeListener(mPrefsListener);
    }

    /** Called when the last showing context menu is dismissed. */
    void onContextMenuDismissed() {
        // When expand-on-hover was turned off, the rail was already collapsed.
        if (!isTrackingPointer()) return;
        switch (mPointerState) {
            case PointerState.OUTSIDE:
                // A collapse arrived while the menu was showing, e.g. a HOVER_EXIT outside the rail
                // when the pointer moved straight to the web contents without entering the menu.
                // The pointer is known to be outside, so apply the held collapse right away.
                applyPointerState(PointerState.OUTSIDE);
                break;
            case PointerState.INSIDE_UNCONFIRMED:
                // The pointer moved into the menu, so the rail lost track of it. Stay expanded if a
                // hover event confirms the pointer over the rail in time, e.g. after a click on the
                // rail that dismissed the menu. Collapse otherwise, e.g. after a click on a menu
                // item, or when the pointer left the rail through the menu.
                cancelMenuDismissTimeout();
                ThreadUtils.getUiThreadHandler()
                        .postDelayed(mMenuDismissTimeout, MENU_DISMISS_TIMEOUT_MS);
                break;
            default:
                // INSIDE: the pointer is confirmed over the rail, so the rail stays expanded.
                break;
        }
    }

    // VerticalTabRailLayout.RailEventListener implementation.

    @Override
    public void onGenericMotionEventDispatched(MotionEvent event) {
        if (!isTrackingPointer() || !event.isFromSource(InputDevice.SOURCE_MOUSE)) return;

        int action = event.getActionMasked();
        float rawX = event.getRawX();
        float rawY = event.getRawY();
        if (action == MotionEvent.ACTION_HOVER_ENTER || action == MotionEvent.ACTION_HOVER_MOVE) {
            // Hover moves past the rail edge are followed by a HOVER_EXIT, which handles them.
            if (!containsRawPoint(mRailView, rawX, rawY)) return;
            // Only record the position, without expanding the rail:
            // - Over the collapse button, so clicking it triggers a full collapsed-to-expanded
            //   animation instead of cutting an in-flight hover animation short.
            // - When the activity is not in front, as hover events also reach a window that is
            //   not in front.
            boolean isOverCollapseButton =
                    containsRawPoint(mRailView.getCollapseButton(), rawX, rawY);
            if (isOverCollapseButton || !mWindowAndroid.isTopResumedActivity()) {
                recordPointerState(PointerState.INSIDE);
                return;
            }
            // The pointer is confirmed over the rail: expand it.
            applyPointerState(PointerState.INSIDE);
        } else if (action == MotionEvent.ACTION_HOVER_EXIT) {
            if (containsRawPoint(mRailView, rawX, rawY)) {
                // Hover stopped while the pointer is still within the rail bounds: a mouse button
                // was pressed, or another window now covers the pointer. Keep the current state
                // until a later pointer event reveals where the pointer is.
                recordPointerState(PointerState.INSIDE_UNCONFIRMED);
            } else {
                applyPointerState(PointerState.OUTSIDE);
            }
        }
    }

    @Override
    public void onTouchEventDispatched(MotionEvent event) {
        if (!isTrackingPointer()) return;
        // While a mouse button is pressed, Android sends touch events from SOURCE_MOUSE instead of
        // hover events, and keeps sending them to the rail even outside its bounds, e.g. when a
        // press on the rail is dragged over the web contents. Finger touches are ignored.
        if (event.getActionMasked() != MotionEvent.ACTION_UP
                || !event.isFromSource(InputDevice.SOURCE_MOUSE)) {
            return;
        }
        // A release inside the rail is left to the hover events that follow it.
        if (!containsRawPoint(mRailView, event.getRawX(), event.getRawY())) {
            applyPointerState(PointerState.OUTSIDE);
        }
    }

    @Override
    public void onDragEventDispatched(DragEvent event) {
        if (!isTrackingPointer()) return;
        // Hover events are not sent during a system drag and drop, so the rail cannot tell when the
        // pointer leaves it. The position is unconfirmed for the duration of the drag.
        switch (event.getAction()) {
            case DragEvent.ACTION_DRAG_STARTED:
                if (mPointerState == PointerState.INSIDE) {
                    recordPointerState(PointerState.INSIDE_UNCONFIRMED);
                }
                break;
            case DragEvent.ACTION_DRAG_ENDED:
                if (mPointerState == PointerState.INSIDE_UNCONFIRMED) {
                    applyPointerState(PointerState.OUTSIDE);
                }
                break;
            default:
                break;
        }
    }

    // WindowAndroid.ActivityStateObserver implementation.

    @Override
    public void onActivityTopResumedChanged(boolean isTopResumedActivity) {
        if (!isTrackingPointer() || isTopResumedActivity) return;
        // Another activity came to the front. Expand on hover only works in the front activity.
        applyPointerState(PointerState.OUTSIDE);
    }

    /** Cancels the pending menu dismiss timeout, if any. */
    private void cancelMenuDismissTimeout() {
        ThreadUtils.getUiThreadHandler().removeCallbacks(mMenuDismissTimeout);
    }

    /** Records the pointer state without changing the rail. */
    private void recordPointerState(@PointerState int state) {
        // Recording a confirmed state cancels a pending menu dismiss timeout, so the timeout never
        // collapses the rail once the position is known.
        if (state != PointerState.INSIDE_UNCONFIRMED) cancelMenuDismissTimeout();
        mPointerState = state;
    }

    /**
     * Records the pointer state and applies it to the rail: {@link PointerState#INSIDE} expands it,
     * {@link PointerState#OUTSIDE} collapses it.
     */
    private void applyPointerState(@PointerState int state) {
        assert state != PointerState.INSIDE_UNCONFIRMED;
        recordPointerState(state);
        boolean isHovering = state == PointerState.INSIDE;
        // Hold back a collapse while a context menu is showing, so the rail does not collapse
        // under the menu. onContextMenuDismissed() applies it based on the pointer state.
        if (!isHovering && mIsContextMenuShowingSupplier.getAsBoolean()) return;
        mCollapseController.setHovering(isHovering);
    }

    /** Returns whether the given screen coordinates fall inside {@code view}'s bounds. */
    private boolean containsRawPoint(View view, float rawX, float rawY) {
        view.getLocationOnScreen(mTempLocation);
        return rawX >= mTempLocation[0]
                && rawX < mTempLocation[0] + view.getWidth()
                && rawY >= mTempLocation[1]
                && rawY < mTempLocation[1] + view.getHeight();
    }

    /** Returns whether pointer events should drive the rail's expand-on-hover state. */
    private static boolean isTrackingPointer() {
        return VerticalTabUtils.isExpandOnHoverEnabled();
    }

    /**
     * Called when the user turns expand-on-hover on or off. When turned off, pointer events are no
     * longer tracked, so the rail is collapsed right away if it is expanded for hovering. This does
     * not wait for a context menu to be dismissed, as the setting is usually changed from one that
     * is being dismissed. When turned on, the next hover event over the rail expands it.
     */
    private void onExpandOnHoverSettingChanged() {
        if (isTrackingPointer()) return;
        recordPointerState(PointerState.OUTSIDE);
        mCollapseController.setHovering(false);
    }

    @PointerState
    int getPointerStateForTesting() {
        return mPointerState;
    }
}
