// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_sync;

import org.chromium.base.Log;
import org.chromium.build.BuildConfig;

/** Utility methods for logging. All logs should be removed before launch. */
public final class LogUtils {
    /**
     * Called to log a message.
     *
     * @param tag Used to identify the source of a log message.
     * @param message The log message.
     */
    public static void log(String tag, String message) {
        if (BuildConfig.ENABLE_ASSERTS) {
            Log.d(tag, message);
        }
    }
}
