// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;

import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.webapk.lib.client.DexOptimizer;
import org.chromium.webapk.lib.client.WebApkVersion;
import org.chromium.webapk.lib.common.WebApkCommonUtils;

import java.io.File;

/**
 * Updates installed WebAPKs after a Chrome update.
 */
public class WebApkVersionManager {
    /**
     * Name of the shared preference for the version number of the dynamically loaded dex.
     */
    private static final String EXTRACTED_DEX_VERSION_PREF =
            "org.chromium.chrome.browser.webapps.extracted_dex_version";

    /**
     * Name of the shared preference for the Android OS version at the time that the dex was last
     * extracted from Chrome's assets and optimized.
     */
    private static final String LAST_SDK_VERSION_PREF =
            "org.chromium.chrome.browser.webapps.last_sdk_version";

    /**
     * Tries to extract the WebAPK runtime dex from the Chrome APK if it has not tried already.
     * Should not be called on UI thread.
     */
    // TODO(crbug.com/635567): Fix this properly.
    @SuppressLint("SetWorldReadable")
    public static void updateWebApksIfNeeded() {
        assert !ThreadUtils.runningOnUiThread();

        SharedPreferences preferences = ContextUtils.getAppSharedPreferences();
        int extractedDexVersion = preferences.getInt(EXTRACTED_DEX_VERSION_PREF, -1);
        int lastSdkVersion = preferences.getInt(LAST_SDK_VERSION_PREF, -1);
        if (!CommandLine.getInstance().hasSwitch(
                    ChromeSwitches.ALWAYS_EXTRACT_WEBAPK_RUNTIME_DEX_ON_STARTUP)
                && extractedDexVersion == WebApkVersion.CURRENT_RUNTIME_DEX_VERSION
                && lastSdkVersion == Build.VERSION.SDK_INT) {
            return;
        }

        SharedPreferences.Editor editor = preferences.edit();
        editor.putInt(EXTRACTED_DEX_VERSION_PREF, WebApkVersion.CURRENT_RUNTIME_DEX_VERSION);
        editor.putInt(LAST_SDK_VERSION_PREF, Build.VERSION.SDK_INT);
        editor.apply();

        Context context = ContextUtils.getApplicationContext();
        File dexDir = context.getDir("dex", Context.MODE_PRIVATE);
        FileUtils.recursivelyDeleteFile(dexDir);

        // Recreate world-executable directory using {@link Context#getDir}.
        dexDir = context.getDir("dex", Context.MODE_PRIVATE);

        String dexName =
                WebApkCommonUtils.getRuntimeDexName(WebApkVersion.CURRENT_RUNTIME_DEX_VERSION);
        File dexFile = new File(dexDir, dexName);
        if (!FileUtils.extractAsset(context, dexName, dexFile)) {
            return;
        }

        // Disable VM detectLeakedClosableObjects due to Android SDK bug: https://crbug.com/750196
        try (StrictModeContext ignored = StrictModeContext.allowAllVmPolicies()) {
            if (!DexOptimizer.optimize(dexFile)) {
                return;
            }
        }

        // Make dex file world-readable so that WebAPK can use it.
        dexFile.setReadable(true, false);
    }
}
