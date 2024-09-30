// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.segmentation_platform;

import android.os.Handler;

import org.chromium.chrome.browser.segmentation_platform.ContextualPageActionController.ActionProvider;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/**
 * Convenient wrapper to keep track of the feature backend results and trigger the next step after
 * all the feature backends have responded or a time out has happened.
 */
public class SignalAccumulator {
    private static final long ACTION_PROVIDER_TIMEOUT_MS = 100;

    // List of signals to query. Modify hasAllSignals() when adding signals to this list.
    // TODO(crbug.com/40242243): Introduce a key set and directly populate InputContext.
    private Boolean mHasPriceTracking;
    private Boolean mHasReaderMode;
    private Boolean mHasPriceInsights;
    private Boolean mHasDiscounts;

    // Whether the backends didn't respond within the time limit. Any further response from the
    // backends will be ignored.
    private boolean mHasTimedOut;

    // Whether the results from the backends have already been used for computing the action to
    // show.
    private boolean mIsInValid;

    // The callback to be invoked at the end of getting all the signals or time out. After it is
    // run, the accumulator becomes invalid.
    private Runnable mCompletionCallback;

    private final List<ActionProvider> mActionProviders;
    private final Tab mTab;
    private final Handler mHandler;

    /**
     * Constructor.
     * @param handler A handler for posting task.
     * @param tab The given tab.
     * @param actionProviders List of action providers to get signals from.
     */
    public SignalAccumulator(Handler handler, Tab tab, List<ActionProvider> actionProviders) {
        mHandler = handler;
        mTab = tab;
        mActionProviders = actionProviders;
    }

    /**
     * Called to start getting signals from the respective action providers. The callback will be
     * invoked at the end of signal collection or timeout.
     */
    public void getSignals(Runnable callback) {
        mCompletionCallback = callback;
        for (ActionProvider actionProvider : mActionProviders) {
            actionProvider.getAction(mTab, this);
        }
        mHandler.postDelayed(
                () -> {
                    mHasTimedOut = true;
                    proceedToNextStepIfReady();
                },
                ACTION_PROVIDER_TIMEOUT_MS);
    }

    /**
     * Called to notify that one of the backends is ready and the controller can proceed to next
     * step if it has all the signals.
     */
    public void notifySignalAvailable() {
        proceedToNextStepIfReady();
    }

    /**
     * @return Whether the page is price tracking eligible. Default is false.
     */
    public Boolean hasPriceTracking() {
        return mHasPriceTracking == null ? false : mHasPriceTracking;
    }

    /** Called to set whether the page can be price tracked. */
    public void setHasPriceTracking(Boolean hasPriceTracking) {
        mHasPriceTracking = hasPriceTracking;
    }

    /** @return Whether the page is reader mode eligible. Default is false. */
    public Boolean hasReaderMode() {
        return mHasReaderMode == null ? false : mHasReaderMode;
    }

    /** Called to set whether the page can be viewed in reader mode. */
    public void setHasReaderMode(Boolean hasReaderMode) {
        mHasReaderMode = hasReaderMode;
    }

    /**
     * @return Whether the page is price insights eligible. Default is false.
     */
    public Boolean hasPriceInsights() {
        return mHasPriceInsights == null ? false : mHasPriceInsights;
    }

    /** Called to set whether the page is price insights eligible. */
    public void setHasPriceInsights(Boolean hasPriceInsights) {
        mHasPriceInsights = hasPriceInsights;
    }

    /**
     * @return Whether the page is discounts eligible. Default is false.
     */
    public Boolean hasDiscounts() {
        return mHasDiscounts == null ? false : mHasDiscounts;
    }

    /** Called to set whether the page is discounts eligible. */
    public void setHasDiscounts(Boolean hasDiscounts) {
        mHasDiscounts = hasDiscounts;
    }

    /** Central method invoked whenever a backend responds or time out happens. */
    private void proceedToNextStepIfReady() {
        boolean isReady = mHasTimedOut || hasAllSignals();
        if (!isReady || mIsInValid) return;
        mIsInValid = true;
        mCompletionCallback.run();
    }

    private boolean hasAllSignals() {
        return mHasPriceTracking != null
                && mHasReaderMode != null
                && mHasPriceInsights != null
                && mHasDiscounts != null;
    }
}
