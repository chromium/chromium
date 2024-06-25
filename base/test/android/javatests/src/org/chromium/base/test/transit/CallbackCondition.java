// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.CallSuper;

import org.chromium.base.test.util.CallbackHelper;

/** A {@link Condition} that checks if a single callback was received. */
public class CallbackCondition extends Condition {
    protected final CallbackHelper mCallbackHelper;
    protected final String mCallbackDescription;
    protected final int mNumCallsExpected;
    protected int mStartingCount;

    /**
     * Constructor.
     *
     * @param callbackDescription the user-visible name for the callback.
     * @param numCallsExpected the number of callbacks expected for the Condition to be fulfilled.
     */
    protected CallbackCondition(String callbackDescription, int numCallsExpected) {
        super(/* isRunOnUiThread= */ false);
        mCallbackHelper = new CallbackHelper();
        mCallbackDescription = callbackDescription;
        mNumCallsExpected = numCallsExpected;
    }

    /**
     * Constructor.
     *
     * @param callbackHelper the {@link CallbackHelper} to wait for.
     * @param callbackDescription the user-visible name for the callback.
     * @param numCallsExpected the number of callbacks expected for the Condition to be fulfilled.
     */
    protected CallbackCondition(
            CallbackHelper callbackHelper, String callbackDescription, int numCallsExpected) {
        super(/* isRunOnUiThread= */ false);
        mCallbackHelper = callbackHelper;
        mCallbackDescription = callbackDescription;
        mNumCallsExpected = numCallsExpected;
    }

    @Override
    public String buildDescription() {
        return String.format("Received %d %s callbacks", mNumCallsExpected, mCallbackDescription);
    }

    @CallSuper
    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        mStartingCount = mCallbackHelper.getCallCount();
    }

    @Override
    protected ConditionStatus checkWithSuppliers() {
        int currentCount = mCallbackHelper.getCallCount();
        if (mStartingCount > 0) {
            return whether(
                    currentCount >= mStartingCount + mNumCallsExpected,
                    "Called %d/%d times (%d - %d)",
                    currentCount - mStartingCount,
                    mNumCallsExpected,
                    currentCount,
                    mStartingCount);
        } else {
            return whether(currentCount > 0, "Called %d/%d times", currentCount, mNumCallsExpected);
        }
    }

    protected void notifyCalled() {
        mCallbackHelper.notifyCalled();
    }
}
