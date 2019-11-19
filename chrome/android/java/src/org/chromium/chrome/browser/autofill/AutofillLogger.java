// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;

/**
* JNI call glue for AutofillExternalDelagate C++ and Java objects.
*/
@JNINamespace("autofill")
public class AutofillLogger {
    /**
     * An entry to be sent to Logger.
     */
    public static class LogEntry {
        private final String mAutofilledValue;
        private final String mProfileFullName;

        private LogEntry(String autofilledValue, String profileFullName) {
            mAutofilledValue = autofilledValue;
            mProfileFullName = profileFullName;
        }

        @VisibleForTesting
        public String getAutofilledValue() {
            return mAutofilledValue;
        }

        @VisibleForTesting
        public String getProfileFullName() {
            return mProfileFullName;
        }
    }

    /**
     * A logger interface. Uses LogItem instead of individual fields to allow
     * changing the items that are logged without breaking the embedder.
     */
    public interface Logger {
        public void didFillField(LogEntry logItem);
    }

    private static Logger sLogger;
    private static Logger sLoggerForTest;

    @VisibleForTesting
    public static void setLogger(Logger logger) {
        sLogger = logger;
    }

    @VisibleForTesting
    public static void setLoggerForTesting(Logger logger) {
        sLoggerForTest = logger;
    }

    @CalledByNative
    private static void didFillField(String autofilledValue, String profileFullName) {
        if (sLogger != null) {
            sLogger.didFillField(new LogEntry(autofilledValue, profileFullName));
        }

        if (sLoggerForTest != null) {
            sLoggerForTest.didFillField(new LogEntry(autofilledValue, profileFullName));
        }
    }
}
