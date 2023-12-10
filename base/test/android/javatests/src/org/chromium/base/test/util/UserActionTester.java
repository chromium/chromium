// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.annotation.GuardedBy;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.UmaRecorderHolder;

import java.util.ArrayList;
import java.util.List;

/** A util class that records UserActions. */
public class UserActionTester implements Callback<String> {
    @GuardedBy("mActions")
    private List<String> mActions;

    public UserActionTester() {
        mActions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        UmaRecorderHolder.get()
                                .addUserActionCallbackForTesting(UserActionTester.this);
                    }
                });
    }

    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(
                new Runnable() {
                    @Override
                    public void run() {
                        UmaRecorderHolder.get()
                                .removeUserActionCallbackForTesting(UserActionTester.this);
                    }
                });
    }

    @Override
    public void onResult(String action) {
        synchronized (mActions) {
            mActions.add(action);
        }
    }

    /**
     * @return A copy of the current list of recorded UserActions.
     */
    public List<String> getActions() {
        synchronized (mActions) {
            return new ArrayList<>(mActions);
        }
    }

    /**
     * @return How many times the |actionToCount| was recorded.
     */
    public int getActionCount(String actionToCount) {
        int count = 0;
        for (String action : getActions()) {
            if (action.equals(actionToCount)) count++;
        }
        return count;
    }

    @Override
    public String toString() {
        return "Actions: " + getActions();
    }
}
