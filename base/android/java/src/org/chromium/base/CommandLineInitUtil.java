// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.annotation.SuppressLint;
import android.content.Context;
import android.os.Build;
import android.provider.Settings;

import androidx.annotation.Nullable;

import java.io.File;

/**
 * Provides implementation of command line initialization for Android.
 */
public final class CommandLineInitUtil {
    /**
     * The location of the command line file needs to be in a protected
     * directory so requires root access to be tweaked, i.e., no other app in a
     * regular (non-rooted) device can change this file's contents.
     * See below for debugging on a regular (non-rooted) device.
     */
    private static final String COMMAND_LINE_FILE_PATH = "/data/local";

    /**
     * This path (writable by the shell in regular non-rooted "user" builds) is used when:
     * 1) The "debug app" is set to the application calling this.
     * and
     * 2) ADB is enabled.
     * 3) Force enabled by the embedder.
     */
    private static final String COMMAND_LINE_FILE_PATH_DEBUG_APP = "/data/local/tmp";

    private CommandLineInitUtil() {
    }

    /**
     * Initializes the CommandLine class, pulling command line arguments from {@code fileName}.
     * @param fileName The name of the command line file to pull arguments from.
     */
    public static void initCommandLine(String fileName) {
        initCommandLine(fileName, null);
    }

    /**
     * Initializes the CommandLine class, pulling command line arguments from {@code fileName}.
     * @param fileName The name of the command line file to pull arguments from.
     * @param shouldUseDebugFlags If non-null, returns whether debug flags are allowed to be used.
     */
    public static void initCommandLine(
            String fileName, @Nullable Supplier<Boolean> shouldUseDebugFlags) {
        assert !CommandLine.isInitialized();
        File commandLineFile = new File(COMMAND_LINE_FILE_PATH_DEBUG_APP, fileName);
        // shouldUseDebugCommandLine() uses IPC, so don't bother calling it if no flags file exists.
        boolean debugFlagsExist = commandLineFile.exists();
        if (!debugFlagsExist || !shouldUseDebugCommandLine(shouldUseDebugFlags)) {
            commandLineFile = new File(COMMAND_LINE_FILE_PATH, fileName);
        }
        CommandLine.initFromFile(commandLineFile.getPath());
    }

    /**
     * Use an alternative path if:
     * - The current build is "eng" or "userdebug", OR
     * - adb is enabled and this is the debug app, OR
     * - Force enabled by the embedder.
     * @param shouldUseDebugFlags If non-null, returns whether debug flags are allowed to be used.
     */
    private static boolean shouldUseDebugCommandLine(
            @Nullable Supplier<Boolean> shouldUseDebugFlags) {
        if (shouldUseDebugFlags != null && shouldUseDebugFlags.get()) return true;
        Context context = ContextUtils.getApplicationContext();
        String debugApp = Build.VERSION.SDK_INT < Build.VERSION_CODES.JELLY_BEAN_MR1
                ? getDebugAppPreJBMR1(context)
                : getDebugAppJBMR1(context);
        // Check isDebugAndroid() last to get full code coverage when using userdebug devices.
        return context.getPackageName().equals(debugApp) || BuildInfo.isDebugAndroid();
    }

    @SuppressLint("NewApi")
    private static String getDebugAppJBMR1(Context context) {
        boolean adbEnabled = Settings.Global.getInt(context.getContentResolver(),
                Settings.Global.ADB_ENABLED, 0) == 1;
        if (adbEnabled) {
            return Settings.Global.getString(context.getContentResolver(),
                    Settings.Global.DEBUG_APP);
        }
        return null;
    }

    @SuppressWarnings("deprecation")
    private static String getDebugAppPreJBMR1(Context context) {
        boolean adbEnabled = Settings.System.getInt(context.getContentResolver(),
                Settings.System.ADB_ENABLED, 0) == 1;
        if (adbEnabled) {
            return Settings.System.getString(context.getContentResolver(),
                    Settings.System.DEBUG_APP);
        }
        return null;
    }
}
