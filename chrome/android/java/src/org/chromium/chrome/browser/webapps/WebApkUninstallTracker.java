// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.text.TextUtils;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.browserservices.intents.WebappIntentUtils;
import org.chromium.chrome.browser.browserservices.metrics.WebApkUkmRecorder;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.webapps.WebApkDistributor;
import org.chromium.components.webapps.WebappsUtils;

import java.util.HashSet;
import java.util.Set;

/** Track WebAPKs uninstalls. */
@NullMarked
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
        boolean changed = false;
        for (String uninstalledPackage : uninstalledPackages) {
            RecordHistogram.recordBooleanHistogram("WebApk.Uninstall.Browser", true);

            String webApkId = WebappIntentUtils.getIdForWebApkPackage(uninstalledPackage);
            WebappDataStorage webappDataStorage =
                    WebappRegistry.getInstance().getWebappDataStorage(webApkId);
            if (webappDataStorage != null) {
                String manifestId = webappDataStorage.getWebApkManifestId();
                if (!TextUtils.isEmpty(manifestId)) {
                    WebApkSyncService.onWebApkUninstalled(manifestId);
                    notifyAppBannerManagersOfUninstall(webappDataStorage.getScope());
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
                changed = true;
            }
        }
        preferencesManager.writeStringSet(
                ChromePreferenceKeys.WEBAPK_UNINSTALLED_PACKAGES, new HashSet<>());
        if (changed) {
            WebappRegistry.getInstance().notifyOriginsWithInstalledAppChanged();
        }
    }

    /**
     * Defers recording WebAPK uninstall, or records it immediately if native is already loaded.
     *
     * @param packageName The package name of the uninstalled WebAPK.
     */
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
        if (LibraryLoader.getInstance().isInitialized()) {
            ThreadUtils.runOnUiThread(WebApkUninstallTracker::runDeferredTasks);
        }
    }

    private static void notifyAppBannerManagersOfUninstall(String scope) {
        WebappTabUtils.recheckInstallabilityForMatchingTabs(
                tab -> {
                    if (tab.getWebContents() == null) return false;
                    String url = tab.getWebContents().getLastCommittedUrl().getSpec();
                    return url != null && url.startsWith(scope);
                });
    }

    private WebApkUninstallTracker() {}
}
