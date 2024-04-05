// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import org.chromium.base.test.util.CallbackHelper;

/** A {@link Condition} that checks if a single callback was received. */
public class CallbackCondition extends Condition {
    private final CallbackHelper mCallbackHelper;
    private final String mDescription;
    private int mStartingCount;

    /**
     * Use {@link #instrumentationThread(CallbackHelper, String) or {@link #uiThread(CallbackHelper, String)}
     *
     * @param callbackHelper the {@link CallbackHelper} to wait for.
     * @param description the user-visible name for the Condition.
     */
    private CallbackCondition(
            boolean runOnUiThread, CallbackHelper callbackHelper, String description) {
        super(runOnUiThread);
        mCallbackHelper = callbackHelper;
        mDescription = description;
    }

    public static CallbackCondition instrumentationThread(
            CallbackHelper callbackHelper, String description) {
        return new CallbackCondition(/* runOnUiThread= */ false, callbackHelper, description);
    }

    public static CallbackCondition uiThread(CallbackHelper callbackHelper, String description) {
        return new CallbackCondition(/* runOnUiThread= */ true, callbackHelper, description);
    }

    @Override
    public String buildDescription() {
        return mDescription;
    }

    @Override
    public void onStartMonitoring() {
        mStartingCount = mCallbackHelper.getCallCount();
    }

    @Override
    public ConditionStatus check() {
        int currentCount = mCallbackHelper.getCallCount();
        if (mStartingCount > 0) {
            return whether(
                    currentCount > mStartingCount,
                    "Called %d/1 times (%d - %d)",
                    currentCount - mStartingCount,
                    currentCount,
                    mStartingCount);
        } else {
            return whether(currentCount > 0, "Called %d/1 times", currentCount);
        }
    }
}
