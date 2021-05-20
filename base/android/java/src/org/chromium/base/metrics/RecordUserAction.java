// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.metrics;

import android.os.SystemClock;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;

/**
 * Java API for recording UMA actions.
 * <p>
 * WARNINGS: JNI calls are relatively costly - avoid using in performance-critical code.
 * <p>
 * Action names must be documented in {@code actions.xml}. See {@link
 * https://source.chromium.org/chromium/chromium/src/+/main:tools/metrics/actions/README.md} <p>
 * We use a script ({@code extract_actions.py{}) to scan the source code and extract actions. A
 * string literal (not a variable) must be passed to {@link #record(String)}.
 */
@JNINamespace("base::android")
public class RecordUserAction {
    /**
     * Similar to {@code base::RecordAction()} in C++.
     * <p>
     * Record that the user performed an action. See tools/metrics/actions/README.md
     */
    public static void record(final String action) {
        UmaRecorderHolder.get().recordUserAction(action, SystemClock.elapsedRealtime());
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
        long addActionCallbackForTesting(UserActionCallback callback);
        void removeActionCallbackForTesting(long callbackId);
    }
}
