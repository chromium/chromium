// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.jni_zero.NativeMethods;

/**
 * Handles initialization for the downloads system, i.e. creating in-progress download manager or
 * full download manager depending on whether we are in reduced mode or full browser mode.
 */
public class DownloadStartupUtils {
    /**
     * Initializes the downloads system if not already initialized.
     * @param isFullBrowserStarted Whether full browser process has been started.
     * @param isOffTheRecord Whether the system is for incognito profile.
     */
    public static void ensureDownloadSystemInitialized(
            boolean isFullBrowserStarted, boolean isOffTheRecord) {
        DownloadStartupUtilsJni.get()
                .ensureDownloadSystemInitialized(isFullBrowserStarted, isOffTheRecord);
    }

    @NativeMethods
    interface Natives {
        void ensureDownloadSystemInitialized(boolean isFullBrowserStarted, boolean isInCognito);
    }
}
