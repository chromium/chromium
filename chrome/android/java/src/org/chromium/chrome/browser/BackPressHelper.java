// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import androidx.activity.OnBackPressedCallback;
import androidx.activity.OnBackPressedDispatcher;
import androidx.lifecycle.LifecycleOwner;

/**
 * Helper class for back press event handling via {@link OnBackPressedDisptacher}. This is
 * a recommended way over {@link Activity#onBackPressed}. Refer to the Android developer's guide
 * {@link https://developer.android.com/guide/navigation/navigation-custom-back}.
 */
public final class BackPressHelper {
    /**
     * Handles back press event.
     */
    public interface BackPressedHandler {
        /**
         * @return {@code true} if the event was consumed. {@code false} otherwise.
         */
        boolean onBackPressed();
    }

    /**
     * Create a {@link BackPressHelper} that can handle a chain of handlers.
     * @param lifecycleOwner {@link LifecycleOwner} managing the back press logic's lifecycle.
     * @param dispatcher {@link OnBackPressedDispatcher} that holds other callbacks.
     * @param handler {@link BackPressedHandler} implementing the caller's back press handler.
     */
    public static void create(LifecycleOwner lifecycleOwner, OnBackPressedDispatcher dispatcher,
            BackPressedHandler handler) {
        new BackPressHelper(lifecycleOwner, dispatcher, handler);
    }

    /**
     * Let the back press event be processed by next {@link OnBackPressedCallback}. A callback
     * needs to be in enabled state only when it plans to handle back press event. If the callback
     * cannot handle it for whatever reason, it can call this method to give the next callback
     * or a fallback runnable an opportunity to process it. If the callback needs to receive
     * back press events in the future again, it should enable itself after this call.
     * @param dispatcher {@link OnBackPressedDispatcher} holding other callbacks.
     * @param callback {@link OnBackkPressedCallback} which just received the event but ended up
     *        not handling it.
     */
    public static void onBackPressed(
            OnBackPressedDispatcher dispatcher, OnBackPressedCallback callback) {
        callback.setEnabled(false);
        dispatcher.onBackPressed();
    }

    private BackPressHelper(LifecycleOwner lifecycleOwner, OnBackPressedDispatcher dispatcher,
            BackPressedHandler handler) {
        dispatcher.addCallback(lifecycleOwner, new OnBackPressedCallback(true) {
            @Override
            public void handleOnBackPressed() {
                if (!handler.onBackPressed()) onBackPressed(dispatcher, this);
            }
        });
    }
}
