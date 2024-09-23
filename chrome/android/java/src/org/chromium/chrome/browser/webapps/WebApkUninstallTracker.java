// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.text.TextUtils;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUkmRecorder;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.components.webapps.WebappsUtils;

import java.util.HashSet;
import java.util.Set;

/** Track WebAPKs uninstalls. */
public class WebApkUninstallTracker {
    /** Makes recordings that were deferred in order to not load native. */
    public static void runDeferredTasks() {
        SharedPreferencesManager preferencesManager = ChromeSharedPreferences.getInstance();
        Set<String> uninstalledPackages =
                preferencesManager.readStringSet(ChromePreferenceKeys.WEBAPK_UNINSTALLED_PACKAGES);
        if (uninstalledPackages.isEmpty()) return;

        long fallbackUninstallTimestamp = System.currentTimeMillis();
        WebappRegistry.warmUpSharedPrefs();
        WebappsUtils.prepareIsRequestPinShortcutSupported();
        for (String uninstalledPackage : uninstalledPackages) {
            RecordHistogram.recordBooleanHistogram("WebApk.Uninstall.Browser", true);

            String webApkId = WebappIntentUtils.getIdForWebApkPackage(uninstalledPackage);
            WebappDataStorage webappDataStorage =
                    WebappRegistry.getInstance().getWebappDataStorage(webApkId);
            if (webappDataStorage != null) {
                String manifestId = webappDataStorage.getWebApkManifestId();
                if (!TextUtils.isEmpty(manifestId)) {
                    WebApkSyncService.onWebApkUninstalled(manifestId);
                }

                long uninstallTimestamp = webappDataStorage.getWebApkUninstallTimestamp();
                if (uninstallTimestamp == 0) {
                    uninstallTimestamp = fallbackUninstallTimestamp;
                }
                WebApkUkmRecorder.recordWebApkUninstall(
                        manifestId,
                        WebApkDistributor.BROWSER,
                        webappDataStorage.getWebApkVersionCode(),
                        webappDataStorage.getLaunchCount(),
                        uninstallTimestamp - webappDataStorage.getWebApkInstallTimestamp());
            }
        }
        preferencesManager.writeStringSet(
                ChromePreferenceKeys.WEBAPK_UNINSTALLED_PACKAGES, new HashSet<String>());
    }

    /** Sets WebAPK uninstall to be recorded next time that native is loaded. */
    public static void deferRecordWebApkUninstalled(String packageName) {
        ChromeSharedPreferences.getInstance()
                .addToStringSet(ChromePreferenceKeys.WEBAPK_UNINSTALLED_PACKAGES, packageName);
        String webApkId = WebappIntentUtils.getIdForWebApkPackage(packageName);
        WebappRegistry.warmUpSharedPrefsForId(webApkId);
        WebappDataStorage webappDataStorage =
                WebappRegistry.getInstance().getWebappDataStorage(webApkId);
        if (webappDataStorage != null) {
            webappDataStorage.setWebApkUninstallTimestamp();
        }
    }

    private WebApkUninstallTracker() {}
}
