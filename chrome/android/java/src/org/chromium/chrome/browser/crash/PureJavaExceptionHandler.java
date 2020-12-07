// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.MainDex;
import org.chromium.components.crash.CrashKeys;

/**
 * This UncaughtExceptionHandler will upload the stacktrace when there is an uncaught exception.
 *
 * This happens before native is loaded, and will replace by JavaExceptionReporter after native
 * finishes loading.
 */
@MainDex
public class PureJavaExceptionHandler implements Thread.UncaughtExceptionHandler {
    private final Thread.UncaughtExceptionHandler mParent;
    private boolean mHandlingException;
    private static boolean sIsDisabled;

    private PureJavaExceptionHandler(Thread.UncaughtExceptionHandler parent) {
        mParent = parent;
    }

    @Override
    public void uncaughtException(Thread t, Throwable e) {
        if (!mHandlingException && !sIsDisabled) {
            mHandlingException = true;
            reportJavaException(e);
        }
        if (mParent != null) {
            mParent.uncaughtException(t, e);
        }
    }

    public static void installHandler() {
        if (!sIsDisabled) {
            Thread.setDefaultUncaughtExceptionHandler(
                    new PureJavaExceptionHandler(Thread.getDefaultUncaughtExceptionHandler()));
        }
    }

    @CalledByNative
    private static void uninstallHandler() {
        // The current handler can be in the middle of an exception handler chain. We do not know
        // about handlers before it. If resetting the uncaught exception handler to mParent, we lost
        // all the handlers before mParent. In order to disable this handler, globally setting a
        // flag to ignore it seems to be the easiest way.
        sIsDisabled = true;
        CrashKeys.getInstance().flushToNative();
    }

    private void reportJavaException(Throwable e) {
        PureJavaExceptionReporter.reportJavaException(e);
    }
}
