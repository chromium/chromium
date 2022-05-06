// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.Type;

/**
 * A central manager class to handle the back gesture. Every component/feature which is going to
 * intercept the back press event must implement the {@link BackPressHandler} and be registered
 * in a proper order.
 * In order to register a Handler:
 * 1. Implement {@link BackPressHandler}.
 * 2. Add a new {@link Type} which implies the order of intercepting.
 * 3. Call {@link #addHandler(BackPressHandler, int)} to register the implementer of
 * {@link BackPressHandler} with the new defined {@link Type}.
 */
public class BackPressManager {
    private final OnBackPressedCallback mCallback = new OnBackPressedCallback(false) {
        @Override
        public void handleOnBackPressed() {
            BackPressManager.this.handleBackPress();
        }
    };

    private final BackPressHandler[] mHandlers = new BackPressHandler[Type.NUM_TYPES];

    private final Callback<Boolean> mObserverCallback = (t) -> backPressStateChanged();

    /**
     * @return True if the back gesture refactor is enabled.
     */
    public static boolean isEnabled() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.BACK_GESTURE_REFACTOR);
    }

    /**
     * Register the handler to intercept the back gesture.
     * @param handler Implementer of {@link BackPressHandler}.
     * @param type The {@link Type} of the handler.
     */
    public void addHandler(BackPressHandler handler, @Type int type) {
        assert mHandlers[type] == null : "Each type can have at most one handler";
        mHandlers[type] = handler;
        handler.getHandleBackPressChangedSupplier().addObserver(mObserverCallback);
    }

    /**
     * Remove a registered handler. The methods of handler will not be called any more.
     * @param handler {@link BackPressHandler} to be removed.
     */
    public void removeHandler(@NonNull BackPressHandler handler) {
        for (int i = 0; i < mHandlers.length; i++) {
            if (mHandlers[i] == handler) {
                handler.getHandleBackPressChangedSupplier().removeObserver(mObserverCallback);
                mHandlers[i] = null;
            }
        }
    }

    /**
     * Remove a registered handler. The methods of handler will not be called any more.
     * @param type {@link Type} to be removed.
     */
    public void removeHandler(@Type int type) {
        mHandlers[type] = null;
    }

    /**
     * @param type The {@link Type} which needs to check.
     * @return True if a handler of this type has been registered.
     */
    public boolean has(@Type int type) {
        return mHandlers[type] != null;
    }

    /**
     * @return A {@link OnBackPressedCallback} which should be added to
     * {@link androidx.activity.OnBackPressedDispatcher}.
     */
    public OnBackPressedCallback getCallback() {
        return mCallback;
    }

    private void backPressStateChanged() {
        boolean isEnabled = false;
        for (BackPressHandler handler : mHandlers) {
            if (handler == null) continue;
            Boolean enabled = handler.getHandleBackPressChangedSupplier().get();
            if (enabled != null && enabled) {
                isEnabled = true;
                break;
            }
        }
        mCallback.setEnabled(isEnabled);
    }

    private void handleBackPress() {
        for (BackPressHandler handler : mHandlers) {
            if (handler == null) continue;
            Boolean enabled = handler.getHandleBackPressChangedSupplier().get();
            if (enabled != null && enabled) {
                handler.handleBackPress();
                return;
            }
        }
    }

    BackPressHandler[] getHandlersForTesting() {
        return mHandlers;
    }
}