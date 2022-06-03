// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.common;

import android.os.StrictMode;

import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;

/**
 * Utility class for WebView's CommandLine - this is compiled into a separate target that can be
 * reached from WebView's separate minidump-uploading Services.
 */
public class CommandLineUtil {
    protected CommandLineUtil() {}

    public static final String WEBVIEW_COMMAND_LINE_FILE = "/data/local/tmp/webview-command-line";

    private static final String COMMAND_LINE_UTIL_INTERNAL =
            "org.chromium.android_webview.common.CommandLineUtilInternal";

    private static CommandLineUtil sInstance;

    private static CommandLineUtil getInstance() {
        if (sInstance != null) return sInstance;
        try {
            sInstance = (CommandLineUtil) Class.forName(COMMAND_LINE_UTIL_INTERNAL).newInstance();
        } catch (ClassNotFoundException | InstantiationException | IllegalAccessException
                | IllegalArgumentException e) {
            sInstance = new CommandLineUtil();
        }
        return sInstance;
    }

    /**
     * Initialize the CommandLine for WebView - this should be initialized on the same thread where
     * we subsequently access CommandLine.
     */
    public static void initCommandLine() {
        if (BuildInfo.isDebugAndroid()) {
            // Suppress the StrictMode violation as this codepath is only hit on debuggable builds.
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
            CommandLine.initFromFile(CommandLineUtil.WEBVIEW_COMMAND_LINE_FILE);
            StrictMode.setThreadPolicy(oldPolicy);
        } else {
            CommandLine.init(null);
        }
        getInstance().initCommandLineInternal(CommandLine.getInstance());
    }

    protected void initCommandLineInternal(CommandLine commandLine) {}
}
