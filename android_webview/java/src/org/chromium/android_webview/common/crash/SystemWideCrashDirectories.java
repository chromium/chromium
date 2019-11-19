// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common.crash;

import org.chromium.base.ContextUtils;

import java.io.File;

/**
 * Contains getters for the folders where crash information will be stored.
 *
 * This class should NOT be called in a non-WebView UID.
 */
public class SystemWideCrashDirectories {
    private static final String WEBVIEW_CRASH_LOG_DIR = "crash_logs";
    private static final String WEBVIEW_CRASH_DIR = "WebView_Crashes";
    private static final String WEBVIEW_TMP_CRASH_DIR = "WebView_Crashes_Tmp";

    /**
     * Create the directory in which WebView will log crashes info.
     * @return a reference to the created directory, or null if the creation failed.
     */
    public static File getWebViewCrashLogDir() {
        return new File(getWebViewCrashDir(), WEBVIEW_CRASH_LOG_DIR);
    }

    /**
     * Create the directory in which WebView will log crashes info.
     * @return a reference to the created directory, or null if the creation failed.
     */
    public static File getOrCreateWebViewCrashLogDir() {
        File dir = new File(getOrCreateWebViewCrashDir(), WEBVIEW_CRASH_LOG_DIR);
        return getOrCreateDir(dir);
    }

    /**
     * Fetch the crash directory where WebView stores its minidumps.
     * @return a File pointing to the crash directory.
     */
    public static File getWebViewCrashDir() {
        return new File(ContextUtils.getApplicationContext().getCacheDir(), WEBVIEW_CRASH_DIR);
    }

    /**
     * Create the directory in which WebView will store its minidumps.
     * WebView needs a crash directory different from Chrome's to ensure Chrome's and WebView's
     * minidump handling won't clash in cases where both Chrome and WebView are provided by the
     * same app (Monochrome).
     * @return a reference to the created directory, or null if the creation failed.
     */
    public static File getOrCreateWebViewCrashDir() {
        return getOrCreateDir(getWebViewCrashDir());
    }

    /**
     * Directory where we store files temporarily when copying from an app process.
     */
    public static File getWebViewTmpCrashDir() {
        return new File(ContextUtils.getApplicationContext().getCacheDir(), WEBVIEW_TMP_CRASH_DIR);
    }

    private static File getOrCreateDir(File dir) {
        // Call mkdir before isDirectory to ensure that if another thread created the directory
        // just before the call to mkdir, the current thread fails mkdir, but passes isDirectory.
        if (dir.mkdir() || dir.isDirectory()) {
            return dir;
        }
        return null;
    }

    private SystemWideCrashDirectories() {}
}
