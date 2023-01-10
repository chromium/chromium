// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.back_press;

import androidx.activity.OnBackPressedCallback;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler.Type;

import java.util.HashMap;
import java.util.Map;

/**
 * A central manager class to handle the back gesture. Every component/feature which is going to
 * intercept the back press event must implement the {@link BackPressHandler} and be registered
 * in a proper order.
 * In order to register a Handler:
 * 1. Implement {@link BackPressHandler}.
 * 2. Add a new {@link Type} which implies the order of intercepting.
 * 3. Add a new value in {@link #sMetricsMap} which stands for the histograms.
 * 4. Call {@link #addHandler(BackPressHandler, int)} to register the implementer of
 * {@link BackPressHandler} with the new defined {@link Type}.
 */
public class BackPressManager implements Destroyable {
    @VisibleForTesting
    static final Map<Integer, Integer> sMetricsMap = new HashMap() {
        {
            put(Type.TEXT_BUBBLE, 0);
            put(Type.VR_DELEGATE, 1);
            put(Type.AR_DELEGATE, 2);
            put(Type.SCENE_OVERLAY, 3);
            put(Type.START_SURFACE_MEDIATOR, 4);
            put(Type.SELECTION_POPUP, 5);
            put(Type.MANUAL_FILLING, 6);
            put(Type.FULLSCREEN, 7);
            put(Type.BOTTOM_SHEET, 8);
            put(Type.TAB_MODAL_HANDLER, 9);
            put(Type.TAB_SWITCHER_TO_BROWSING, 10);
            put(Type.CLOSE_WATCHER, 11);
            put(Type.TAB_HISTORY, 12);
            put(Type.TAB_RETURN_TO_CHROME_START_SURFACE, 13);
            put(Type.SHOW_READING_LIST, 14);
            put(Type.MINIMIZE_APP_AND_CLOSE_TAB, 15);
            put(Type.FIND_TOOLBAR, 16);
            put(Type.LOCATION_BAR, 17);
            // Add new one here.
        }
    };

    private final OnBackPressedCallback mCallback = new OnBackPressedCallback(false) {
        @Override
        public void handleOnBackPressed() {
            BackPressManager.this.handleBackPress();
        }
    };

    static final String HISTOGRAM = "Android.BackPress.Intercept";

    private final BackPressHandler[] mHandlers = new BackPressHandler[Type.NUM_TYPES];

    private final Callback<Boolean>[] mObserverCallbacks = new Callback[Type.NUM_TYPES];
    private final boolean[] mStates = new boolean[Type.NUM_TYPES];
    private int mEnabledCount;
    private int mLastCalledHandlerForTesting = -1;

    /**
     * @return True if the back gesture refactor is enabled.
     */
    public static boolean isEnabled() {
        return ChromeFeatureList.sBackGestureRefactorAndroid.isEnabled();
    }

    /**
     * Record when the back press is consumed by a certain feature.
     * @param type The {@link Type} which consumes the back press event.
     */
    public static void record(@Type int type) {
        RecordHistogram.recordEnumeratedHistogram(HISTOGRAM, sMetricsMap.get(type), Type.NUM_TYPES);
    }

    /**
     * Register the handler to intercept the back gesture.
     * @param handler Implementer of {@link BackPressHandler}.
     * @param type The {@link Type} of the handler.
     */
    public void addHandler(BackPressHandler handler, @Type int type) {
        assert mHandlers[type] == null : "Each type can have at most one handler";
        mHandlers[type] = handler;
        mObserverCallbacks[type] = (t) -> backPressStateChanged(type);
        handler.getHandleBackPressChangedSupplier().addObserver(mObserverCallbacks[type]);
    }

    /**
     * Remove a registered handler. The methods of handler will not be called any more.
     * @param handler {@link BackPressHandler} to be removed.
     */
    public void removeHandler(@NonNull BackPressHandler handler) {
        for (int i = 0; i < mHandlers.length; i++) {
            if (mHandlers[i] == handler) {
                removeHandler(i);
                return;
            }
        }
    }

    /**
     * Remove a registered handler. The methods of handler will not be called any more.
     * @param type {@link Type} to be removed.
     */
    public void removeHandler(@Type int type) {
        BackPressHandler handler = mHandlers[type];
        Boolean enabled = mHandlers[type].getHandleBackPressChangedSupplier().get();
        if (enabled != null && enabled) {
            mEnabledCount--;
            mStates[type] = false;
            mCallback.setEnabled(mEnabledCount != 0);
        }
        handler.getHandleBackPressChangedSupplier().removeObserver(mObserverCallbacks[type]);
        mObserverCallbacks[type] = null;
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

    private void backPressStateChanged(@Type int type) {
        Boolean enabled = mHandlers[type].getHandleBackPressChangedSupplier().get();
        if (enabled == null || enabled == mStates[type]) return;
        if (enabled) {
            mEnabledCount++;
        } else {
            mEnabledCount--;
        }
        mStates[type] = enabled;
        mCallback.setEnabled(mEnabledCount != 0);
    }

    private void handleBackPress() {
        for (int i = 0; i < mHandlers.length; i++) {
            BackPressHandler handler = mHandlers[i];
            if (handler == null) continue;
            Boolean enabled = handler.getHandleBackPressChangedSupplier().get();
            if (enabled != null && enabled) {
                // Record before #handleBackPress; otherwise, histograms may be missing if
                // #handleBackPress throws an error.
                record(i);
                handler.handleBackPress();
                mLastCalledHandlerForTesting = i;
                return;
            }
        }
    }

    @Override
    public void destroy() {
        for (int i = 0; i < mHandlers.length; i++) {
            if (has(i)) {
                removeHandler(i);
            }
        }
    }

    @VisibleForTesting
    public BackPressHandler[] getHandlersForTesting() {
        return mHandlers;
    }

    @VisibleForTesting
    public int getLastCalledHandlerForTesting() {
        return mLastCalledHandlerForTesting;
    }

    @VisibleForTesting
    public void resetLastCalledHandlerForTesting() {
        mLastCalledHandlerForTesting = -1;
    }
}
