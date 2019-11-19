// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java API for recording UMA actions.
 *
 * WARNINGS:
 * JNI calls are relatively costly - avoid using in performance-critical code.
 *
 * We use a script (extract_actions.py) to scan the source code and extract actions. A string
 * literal (not a variable) must be passed to record().
 */
@JNINamespace("base::android")
public class RecordUserAction {
    private static Throwable sDisabledBy;

    /**
     * Tests may not have native initialized, so they may need to disable metrics. The value should
     * be reset after the test done, to avoid carrying over state to unrelated tests.
     */
    @VisibleForTesting
    public static void setDisabledForTests(boolean disabled) {
        if (disabled && sDisabledBy != null) {
            throw new IllegalStateException("UserActions are already disabled.", sDisabledBy);
        }
        sDisabledBy = disabled ? new Throwable() : null;
    }

    public static void record(final String action) {
        if (sDisabledBy != null) return;

        if (ThreadUtils.runningOnUiThread()) {
            RecordUserActionJni.get().recordUserAction(action);
            return;
        }

        ThreadUtils.runOnUiThread(new Runnable() {
            @Override
            public void run() {
                RecordUserActionJni.get().recordUserAction(action);
            }
        });
    }

    /**
     * Interface to a class that receives a callback for each UserAction that is recorded.
     */
    public interface UserActionCallback {
        @CalledByNative("UserActionCallback")
        void onActionRecorded(String action);
    }

    private static long sNativeActionCallback;

    /**
     * Register a callback that is executed for each recorded UserAction.
     * Only one callback can be registered at a time.
     * The callback has to be unregistered using removeActionCallbackForTesting().
     */
    public static void setActionCallbackForTesting(UserActionCallback callback) {
        assert sNativeActionCallback == 0;
        sNativeActionCallback = RecordUserActionJni.get().addActionCallbackForTesting(callback);
    }

    /**
     * Unregister the UserActionCallback.
     */
    public static void removeActionCallbackForTesting() {
        assert sNativeActionCallback != 0;
        RecordUserActionJni.get().removeActionCallbackForTesting(sNativeActionCallback);
        sNativeActionCallback = 0;
    }

    @NativeMethods
    interface Natives {
        void recordUserAction(String action);
        long addActionCallbackForTesting(UserActionCallback callback);
        void removeActionCallbackForTesting(long callbackId);
    }
}
