// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.transit;

import androidx.annotation.CallSuper;

import org.chromium.base.test.util.PayloadCallbackHelper;
import org.chromium.build.annotations.NullMarked;

/**
 * A {@link Condition} that checks if a single callback with payload was received.
 *
 * @param <T> the type of the payload
 */
@NullMarked
public class PayloadCallbackCondition<T> extends ConditionWithResult<T> {
    protected final PayloadCallbackHelper<T> mCallbackHelper;
    protected final String mPayloadDescription;
    protected int mStartingCount;

    /**
     * Constructor.
     *
     * @param callbackDescription the user-visible name for the callback.
     */
    public PayloadCallbackCondition(String callbackDescription) {
        super(/* isRunOnUiThread= */ false);
        mCallbackHelper = new PayloadCallbackHelper<>();
        mPayloadDescription = callbackDescription;
    }

    @Override
    public String buildDescription() {
        return String.format("Received a callback with payload %s", mPayloadDescription);
    }

    @CallSuper
    @Override
    public void onStartMonitoring() {
        super.onStartMonitoring();
        mStartingCount = mCallbackHelper.getCallCount();
    }

    @Override
    protected ConditionStatusWithResult<T> resolveWithSuppliers() throws Exception {
        int currentCount = mCallbackHelper.getCallCount();
        String differenceMessage;
        if (mStartingCount > 0) {
            differenceMessage = String.format(" (%d - %d)", currentCount, mStartingCount);
        } else {
            differenceMessage = "";
        }
        if (currentCount >= mStartingCount + 1) {
            T payload = mCallbackHelper.getOnlyPayloadBlocking();
            if (payload == null) {
                return notFulfilled(
                                "Called %d/1 times%s, but payload was null",
                                currentCount - mStartingCount, differenceMessage)
                        .withoutResult();
            }

            return fulfilled(
                            "Called %d/1 times%s", currentCount - mStartingCount, differenceMessage)
                    .withResult(payload);
        } else {
            return notFulfilled(
                            "Called %d/1 times%s", currentCount - mStartingCount, differenceMessage)
                    .withoutResult();
        }
    }

    public void notifyCalled(T payload) {
        mCallbackHelper.notifyCalled(payload);
    }
}
