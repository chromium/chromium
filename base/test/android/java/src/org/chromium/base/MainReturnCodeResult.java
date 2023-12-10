// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;

/** Contains the result of a native main method that ran in a child process. */
@JNINamespace("base::android")
public final class MainReturnCodeResult {
    private final int mMainReturnCode;
    private final boolean mTimedOut;

    public static MainReturnCodeResult createMainResult(int returnCode) {
        return new MainReturnCodeResult(returnCode, /* timedOut= */ false);
    }

    public static MainReturnCodeResult createTimeoutMainResult() {
        return new MainReturnCodeResult(0, /* timedOut= */ true);
    }

    private MainReturnCodeResult(int mainReturnCode, boolean timedOut) {
        mMainReturnCode = mainReturnCode;
        mTimedOut = timedOut;
    }

    @CalledByNative
    public int getReturnCode() {
        return mMainReturnCode;
    }

    @CalledByNative
    public boolean hasTimedOut() {
        return mTimedOut;
    }
}
