// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import android.os.Handler;

import org.chromium.base.Log;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;

import java.util.HashMap;
import java.util.Map;

/**
 * Convenient wrapper to keep track of the feature backend results and trigger the next step after
 * all the feature backends have responded or a time out has happened.
 */
@NullMarked
public class SignalAccumulator {
    private static final String TAG = "SegmentationPlatform";
    private static final long DEFAULT_ACTION_PROVIDER_TIMEOUT_MS = 300;

    // List of signals to query.
    private final HashMap<Integer, Boolean> mSignals = new HashMap<>();

    // Whether the backends didn't respond within the time limit. Any further response from the
    // backends will be ignored.
    private boolean mHasTimedOut;

    // Whether the results from the backends have already been used for computing the action to
    // show.
    private boolean mIsInValid;

    // The callback to be invoked at the end of getting all the signals or time out. After it is
    // run, the accumulator becomes invalid.
    private @Nullable Runnable mCompletionCallback;
    private long mGetSignalsStartMs;

    private final Map<Integer, ActionProvider> mActionProviders;
    private final Tab mTab;
    private final Handler mHandler;

    /**
     * Constructor.
     *
     * @param handler A handler for posting task.
     * @param tab The given tab.
     * @param actionProviders List of action providers to get signals from.
     */
    public SignalAccumulator(
            Handler handler, Tab tab, Map<Integer, ActionProvider> actionProviders) {
        mHandler = handler;
        mTab = tab;
        mActionProviders = actionProviders;
        for (var actionType : mActionProviders.keySet()) {
            mSignals.put(actionType, null);
        }
    }

    /**
     * Called to start getting signals from the respective action providers. The callback will be
     * invoked at the end of signal collection or timeout.
     */
    void getSignals(Runnable callback) {
        mCompletionCallback = callback;
        mGetSignalsStartMs = System.currentTimeMillis();
        for (ActionProvider actionProvider : mActionProviders.values()) {
            actionProvider.getAction(mTab, this);
        }
        mHandler.postDelayed(
                () -> {
                    mHasTimedOut = true;
                    proceedToNextStepIfReady();
                },
                DEFAULT_ACTION_PROVIDER_TIMEOUT_MS);
    }

    public boolean hasTimedOut() {
        return mHasTimedOut;
    }

    /**
     * Gets the value of a specific signal type.
     *
     * @param signalType Signal type to set, must belong to an ActionProvider given in the
     *     constructor.
     * @return Boolean signal value, or false if signal is not set.
     */
    public boolean getSignal(@AdaptiveToolbarButtonVariant int signalType) {
        if (!mSignals.containsKey(signalType)) {
            Log.w(TAG, "Signal type not found: " + signalType);
            return false;
        }
        var value = mSignals.get(signalType);
        return value == null ? false : value.booleanValue();
    }

    /**
     * Sets the value of a specific signal type and checks if the completion callback can be
     * executed.
     *
     * @param signalType Signal type to set, must belong to an ActionProvider given in the
     *     constructor.
     * @param signalValue Signal value to set.
     */
    public void setSignal(@AdaptiveToolbarButtonVariant int signalType, boolean signalValue) {
        assert mSignals.containsKey(signalType) : "Signal type not found: " + signalType;
        mSignals.put(signalType, signalValue);
        proceedToNextStepIfReady();
    }

    /** Returns the time when the signal timeout started in milliseconds. */
    public long getSignalStartTimeMs() {
        return mGetSignalsStartMs;
    }

    /** Central method invoked whenever a backend responds or time out happens. */
    private void proceedToNextStepIfReady() {
        boolean isReady = mHasTimedOut || hasAllSignals();
        if (!isReady || mIsInValid || mCompletionCallback == null) return;
        mIsInValid = true;
        mCompletionCallback.run();
    }

    private boolean hasAllSignals() {
        for (Boolean value : mSignals.values()) {
            if (value == null) return false;
        }
        return true;
    }
}
