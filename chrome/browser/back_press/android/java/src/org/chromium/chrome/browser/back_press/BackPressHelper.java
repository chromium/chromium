// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import android.view.KeyEvent;

import androidx.activity.OnBackPressedCallback;
import androidx.activity.OnBackPressedDispatcher;
import androidx.lifecycle.LifecycleOwner;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

/**
 * Helper class for back press event handling via {@link OnBackPressedDisptacher}. This is a
 * recommended way over {@link Activity#onBackPressed}. Refer to the Android developer's guide
 * {@link https://developer.android.com/guide/navigation/navigation-custom-back}.
 */
@NullMarked
public final class BackPressHelper {
    /**
     * @deprecated Handles back press event. #onBackPressed is deprecated starting from U. Prefer
     *     {@link BackPressHandler} whenever possible.
     */
    public interface ObsoleteBackPressedHandler {
        /**
         * @return {@code true} if the event was consumed. {@code false} otherwise.
         */
        boolean onBackPressed();
    }

    /** A functional interface for handling key down events, returned by {@link #create}. */
    @FunctionalInterface
    public interface OnKeyDownHandler {
        /**
         * Processes a key down event to potentially handle it as an Escape key action.
         *
         * <p>This method first checks if a clean, unmodified Escape key was pressed. If so, it
         * consults the {@link BackPressHandler} it was created with to determine how to proceed.
         * The logic is as follows:
         *
         * <ol>
         *   <li>If the handler's {@link BackPressHandler#getHandleBackPressChangedSupplier()} is
         *       not currently {@code true}, the event is ignored.
         *   <li>If the handler's {@link BackPressHandler#invokeBackActionOnEscape()} method returns
         *       {@code true}, this method will trigger a standard back press. This unifies the
         *       behavior of the Escape key and the system back gesture for simple "dismiss"
         *       actions.
         *   <li>If {@code invokeBackActionOnEscape()} returns {@code false}, this method will
         *       instead call the handler's {@link BackPressHandler#handleEscPress()} method to
         *       allow for custom, non-back-like behavior.
         * </ol>
         *
         * This entire functionality is gated by the {@code
         * ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES} feature flag.
         *
         * @param keyCode The key code from the {@code onKeyDown} event.
         * @param event The full {@link KeyEvent}, used to check for modifier keys (e.g., Ctrl, Alt)
         *     and repeat counts.
         * @return {@code true} if the Escape key event was consumed by the handler, {@code false}
         *     otherwise. If {@code false}, the caller should continue with default event processing
         *     (e.g., calling {@code super.onKeyDown()}).
         */
        boolean onKeyDown(int keyCode, KeyEvent event);
    }

    /**
     * @param lifecycleOwner {@link LifecycleOwner} managing the back press logic's lifecycle.
     * @param dispatcher {@link OnBackPressedDispatcher} that holds other callbacks.
     * @param handler {@link ObsoleteBackPressedHandler} implementing the caller's back press.
     * @deprecated Create a {@link BackPressHelper} that can handle a chain of handlers. Prefer
     *     {@link #create(LifecycleOwner, OnBackPressedDispatcher, BackPressHandler)} whenever
     *     possible.
     */
    @Deprecated
    public static void create(
            LifecycleOwner lifecycleOwner,
            OnBackPressedDispatcher dispatcher,
            ObsoleteBackPressedHandler handler) {
        new BackPressHelper(lifecycleOwner, dispatcher, handler);
    }

    /**
     * Register a {@link BackPressHandler} on a given {@link OnBackPressedDispatcher}.
     *
     * @param lifecycleOwner {@link LifecycleOwner} managing the back press logic's lifecycle.
     * @param dispatcher {@link OnBackPressedDispatcher} that holds other callbacks.
     * @param handler {@link BackPressHandler} observing back press state and consuming back press.
     * @return An {@link OnKeyDownHandler} that should be called from the Activity's onKeyDown.
     */
    public static OnKeyDownHandler create(
            LifecycleOwner lifecycleOwner,
            OnBackPressedDispatcher dispatcher,
            BackPressHandler handler) {
        var callback =
                new OnBackPressedCallback(/* enabled= */ false) {
                    @Override
                    public void handleOnBackPressed() {
                        handler.handleBackPress();
                    }
                };
        // Update it now since ObservableSupplier posts updates asynchronously.
        if (handler.getHandleBackPressChangedSupplier().get() != null) {
            callback.setEnabled(handler.getHandleBackPressChangedSupplier().get());
        }
        handler.getHandleBackPressChangedSupplier().addObserver(callback::setEnabled);
        dispatcher.addCallback(lifecycleOwner, callback);

        return (keyCode, event) -> {
            if (!ChromeFeatureList.isEnabled(
                    ChromeFeatureList.ENABLE_ESCAPE_HANDLING_FOR_SECONDARY_ACTIVITIES)) {
                return false;
            }

            boolean isEscapeAndOnlyEscape =
                    keyCode == KeyEvent.KEYCODE_ESCAPE
                            && event.getRepeatCount() == 0
                            && !event.isShiftPressed()
                            && !event.isCtrlPressed()
                            && !event.isAltPressed()
                            && !event.isMetaPressed();

            if (!isEscapeAndOnlyEscape) {
                return false;
            }

            if (!Boolean.TRUE.equals(handler.getHandleBackPressChangedSupplier().get())) {
                return false;
            }

            if (handler.invokeBackActionOnEscape()) {
                dispatcher.onBackPressed();
                return true;
            }

            return Boolean.TRUE.equals(handler.handleEscPress());
        };
    }

    /**
     * Register a list of {@link BackPressHandler} on a given {@link OnBackPressedDispatcher}. The
     * first handler has the top priority and the last one has the least. TODO(crbug.com/40252517):
     * consider introducing a lightweight {@link
     * org.chromium.chrome.browser.back_press.BackPressManager} if too many handlers should be
     * registered.
     *
     * @param lifecycleOwner {@link LifecycleOwner} managing the back press logic's lifecycle.
     * @param dispatcher {@link OnBackPressedDispatcher} that holds other callbacks.
     * @param handlers {@link BackPressHandler} observing back press state and consuming back press.
     */
    public static void create(
            LifecycleOwner lifecycleOwner,
            OnBackPressedDispatcher dispatcher,
            BackPressHandler[] handlers) {
        // OnBackPressedDispatcher triggers handlers in a reversed order.
        for (int i = handlers.length - 1; i >= 0; i--) {
            create(lifecycleOwner, dispatcher, handlers[i]);
        }
    }

    /**
     * Let the back press event be processed by next {@link OnBackPressedCallback}. A callback
     * needs to be in enabled state only when it plans to handle back press event. If the callback
     * cannot handle it for whatever reason, it can call this method to give the next callback
     * or a fallback runnable an opportunity to process it. If the callback needs to receive
     * back press events in the future again, it should enable itself after this call.
     * @param dispatcher {@link OnBackPressedDispatcher} holding other callbacks.
     * @param callback {@link OnBackPressedCallback} which just received the event but ended up
     *        not handling it.
     */
    public static void onBackPressed(
            OnBackPressedDispatcher dispatcher, OnBackPressedCallback callback) {
        callback.setEnabled(false);
        dispatcher.onBackPressed();
    }

    private BackPressHelper(
            LifecycleOwner lifecycleOwner,
            OnBackPressedDispatcher dispatcher,
            ObsoleteBackPressedHandler handler) {
        dispatcher.addCallback(
                lifecycleOwner,
                new OnBackPressedCallback(true) {
                    @Override
                    public void handleOnBackPressed() {
                        if (!handler.onBackPressed()) {
                            onBackPressed(dispatcher, this);
                        }
                    }
                });
    }
}
