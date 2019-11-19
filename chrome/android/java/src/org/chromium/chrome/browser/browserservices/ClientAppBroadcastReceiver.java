// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.os.Build;

import org.chromium.base.Log;
import org.chromium.chrome.browser.ChromeApplication;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.browserservices.permissiondelegation.NotificationPermissionUpdater;
import org.chromium.chrome.browser.metrics.WebApkUma;
import org.chromium.chrome.browser.preferences.ChromePreferenceManager;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

/**
 * A {@link android.content.BroadcastReceiver} that detects when a Trusted Web Activity client app
 * has been uninstalled or has had its data cleared. When this happens we clear Chrome's data
 * corresponding to that app.
 *
 * Trusted Web Activities are registered to an origin (eg https://www.example.com), however because
 * cookies can be scoped more loosely, at eTLD+1 (or domain) level (eg *.example.com) [1], we need
 * to clear data at that level. This unfortunately can lead to too much data getting cleared - for
 * example if the https://maps.google.com TWA is cleared, you'll loose cookies for
 * https://mail.google.com too (since they both share the google.com domain).
 *
 * We find this acceptable for two reasons:
 * - The alternative is *not* clearing some related data - eg a TWA linked to
 *   https://maps.google.com sets a cookie with Domain=google.com. The TWA is uninstalled and
 *   reinstalled and it can access the cookie it stored before.
 * - We ask the user before clearing the data and while doing so display the scope of data we're
 *   going to wipe.
 *
 * [1] https://developer.mozilla.org/en-US/docs/Web/HTTP/Cookies#Scope_of_cookies
 *
 * Lifecycle: The lifecycle of this class is managed by Android.
 * Thread safety: {@link #onReceive} will be called on the UI thread.
 */
public class ClientAppBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "ClientAppBroadRec";

    /**
     * An Action that will trigger clearing data on local builds only, for development. The adb
     * command to trigger is:
     * adb shell am broadcast \
     *   -n com.google.android.apps.chrome/\
     * org.chromium.chrome.browser.browserservices.ClientAppBroadcastReceiver \
     *   -a org.chromium.chrome.browser.browserservices.ClientAppBroadcastReceiver.DEBUG \
     *   --ei android.intent.extra.UID 23
     *
     * But replace 23 with the uid of a Trusted Web Activity Client app.
     */
    private static final String ACTION_DEBUG =
            "org.chromium.chrome.browser.browserservices.ClientAppBroadcastReceiver.DEBUG";

    private static final Set<String> BROADCASTS = new HashSet<>(Arrays.asList(
            Intent.ACTION_PACKAGE_DATA_CLEARED,
            Intent.ACTION_PACKAGE_FULLY_REMOVED
    ));

    private final ClearDataStrategy mClearDataStrategy;
    private final ClientAppDataRegister mRegister;
    private final ChromePreferenceManager mChromePreferenceManager;
    private final NotificationPermissionUpdater mNotificationPermissionUpdater;

    /** Constructor with default dependencies for Android. */
    public ClientAppBroadcastReceiver() {
        this(new ClearDataStrategy(), new ClientAppDataRegister(),
                ChromeApplication.getComponent().resolvePreferenceManager(),
                ChromeApplication.getComponent().resolveTwaPermissionUpdater());
    }

    /** Constructor to allow dependency injection in tests. */
    public ClientAppBroadcastReceiver(ClearDataStrategy strategy, ClientAppDataRegister register,
            ChromePreferenceManager manager, NotificationPermissionUpdater permissionUpdater) {
        mClearDataStrategy = strategy;
        mRegister = register;
        mChromePreferenceManager = manager;
        mNotificationPermissionUpdater = permissionUpdater;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) return;
        // Since we only care about ACTION_PACKAGE_DATA_CLEARED and and ACTION_PACKAGE_FULLY_REMOVED
        // which are protected Intents, we can assume that anything that gets past here will be a
        // legitimate Intent sent by the system.
        boolean debug = ChromeVersionInfo.isLocalBuild() && ACTION_DEBUG.equals(intent.getAction());
        if (!debug && !BROADCASTS.contains(intent.getAction())) return;

        int uid = intent.getIntExtra(Intent.EXTRA_UID, -1);
        if (uid == -1) return;

        boolean uninstalled = Intent.ACTION_PACKAGE_FULLY_REMOVED.equals(intent.getAction());

        if (uninstalled && intent.getData() != null) {
            String packageName = intent.getData().getSchemeSpecificPart();
            if (packageName != null
                    && packageName.startsWith(WebApkConstants.WEBAPK_PACKAGE_PREFIX)) {
                // Native is likely not loaded. Defer recording UMA and UKM till the next browser
                // launch.
                WebApkUma.deferRecordWebApkUninstalled(packageName);
            }
        }

        try (BrowserServicesMetrics.TimingMetric unused =
                     BrowserServicesMetrics.getClientAppDataLoadTimingContext()) {

            // The ClientAppDataRegister (because it uses Preferences) is loaded lazily, so to time
            // opening the file we must include the first read as well.
            if (!mRegister.chromeHoldsDataForPackage(uid)) {
                Log.d(TAG, "Chrome holds no data for package.");
                return;
            }
        }

        mClearDataStrategy
                .execute(context, mRegister, mNotificationPermissionUpdater, uid, uninstalled);
        clearPreferences(uid, uninstalled);
    }

    private void clearPreferences(int uid, boolean uninstalled) {
        String packageName = mRegister.getPackageNameForRegisteredUid(uid);
        mChromePreferenceManager.removeTwaDisclosureAcceptanceForPackage(packageName);
        if (uninstalled) {
            mRegister.removePackage(uid);
        }
    }

    /** Implemented as a class partially for historic reasons, partially to help testing. */
    static class ClearDataStrategy {
        public void execute(Context context, ClientAppDataRegister register,
                NotificationPermissionUpdater permissionUpdater, int uid, boolean uninstalled) {
            // Retrieving domains and origins ahead of time, because the register is about to be
            // cleaned up.
            Set<String> domains = register.getDomainsForRegisteredUid(uid);
            Set<String> origins = register.getOriginsForRegisteredUid(uid);

            for (String originAsString : origins) {
                Origin origin = Origin.create(originAsString);
                if (origin != null) permissionUpdater.onClientAppUninstalled(origin);
            }

            String appName = register.getAppNameForRegisteredUid(uid);
            Intent intent = ClearDataDialogActivity
                    .createIntent(context, appName, domains, origins, uninstalled);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
                intent.addFlags(Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
            }
            context.startActivity(intent);
        }
    }
}
