// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.test.util;

import androidx.annotation.GuardedBy;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;

import java.util.ArrayList;
import java.util.List;

/**
 * A util class that records UserActions.
 */
public class UserActionTester implements RecordUserAction.UserActionCallback {
    @GuardedBy("mActions")
    private List<String> mActions;

    public UserActionTester() {
        mActions = new ArrayList<>();
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                RecordUserAction.setActionCallbackForTesting(UserActionTester.this);
            }
        });
    }

    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(new Runnable() {
            @Override
            public void run() {
                RecordUserAction.removeActionCallbackForTesting();
            }
        });
    }

    @Override
    public void onActionRecorded(String action) {
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

    @Override
    public String toString() {
        return "Actions: " + getActions();
    }
}
