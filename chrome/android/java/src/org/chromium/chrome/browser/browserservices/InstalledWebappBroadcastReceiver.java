// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;

import org.chromium.base.Log;
import org.chromium.base.version_info.VersionInfo;
import org.chromium.chrome.browser.ChromeApplicationImpl;
import org.chromium.chrome.browser.browserservices.permissiondelegation.PermissionUpdater;
import org.chromium.chrome.browser.webapps.WebApkUninstallTracker;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.webapk.lib.common.WebApkConstants;

import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;

import javax.inject.Inject;

/**
 * A {@link android.content.BroadcastReceiver} that detects when an installed webapp (TWA or WebAPK)
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
public class InstalledWebappBroadcastReceiver extends BroadcastReceiver {
    private static final String TAG = "IWBroadcastReceiver";

    /**
     * An Action that will trigger clearing data on local builds only, for development. The adb
     * command to trigger is:
     * adb shell am broadcast \
     *   -n com.google.android.apps.chrome/\
     * org.chromium.chrome.browser.browserservices.InstalledWebappBroadcastReceiver \
     *   -a org.chromium.chrome.browser.browserservices.InstalledWebappBroadcastReceiver.DEBUG \
     *   --ei android.intent.extra.UID 23
     *
     * But replace 23 with the uid of a Trusted Web Activity Client app.
     */
    private static final String ACTION_DEBUG =
            "org.chromium.chrome.browser.browserservices.InstalledWebappBroadcastReceiver.DEBUG";

    private static final Set<String> BROADCASTS =
            new HashSet<>(
                    Arrays.asList(
                            Intent.ACTION_PACKAGE_DATA_CLEARED,
                            Intent.ACTION_PACKAGE_FULLY_REMOVED));

    private final ClearDataStrategy mClearDataStrategy;
    private final InstalledWebappDataRegister mDataRegister;
    private final BrowserServicesStore mStore;
    private final PermissionUpdater mPermissionUpdater;

    /** Constructor with default dependencies for Android. */
    @Inject
    public InstalledWebappBroadcastReceiver() {
        this(
                new ClearDataStrategy(),
                new InstalledWebappDataRegister(),
                new BrowserServicesStore(
                        ChromeApplicationImpl.getComponent().resolveChromeSharedPreferences()),
                ChromeApplicationImpl.getComponent().resolvePermissionUpdater());
    }

    /** Constructor to allow dependency injection in tests. */
    public InstalledWebappBroadcastReceiver(
            ClearDataStrategy strategy,
            InstalledWebappDataRegister dataRegister,
            BrowserServicesStore store,
            PermissionUpdater permissionUpdater) {
        mClearDataStrategy = strategy;
        mDataRegister = dataRegister;
        mStore = store;
        mPermissionUpdater = permissionUpdater;
    }

    @Override
    public void onReceive(Context context, Intent intent) {
        if (intent == null) return;
        // Since we only care about ACTION_PACKAGE_DATA_CLEARED and and ACTION_PACKAGE_FULLY_REMOVED
        // which are protected Intents, we can assume that anything that gets past here will be a
        // legitimate Intent sent by the system.
        boolean debug = VersionInfo.isLocalBuild() && ACTION_DEBUG.equals(intent.getAction());
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
                WebApkUninstallTracker.deferRecordWebApkUninstalled(packageName);
            }
        }

        // The {@link InstalledWebappDataRegister} (because it uses Preferences) is loaded
        // lazily, so to time opening the file we must include the first read as well.
        if (!mDataRegister.chromeHoldsDataForPackage(uid)) {
            Log.d(TAG, "Chrome holds no data for package.");
            return;
        }

        mClearDataStrategy.execute(context, mDataRegister, mPermissionUpdater, uid, uninstalled);
        clearPreferences(uid, uninstalled);
    }

    private void clearPreferences(int uid, boolean uninstalled) {
        String packageName = mDataRegister.getPackageNameForRegisteredUid(uid);
        mStore.removeTwaDisclosureAcceptanceForPackage(packageName);
        if (uninstalled) {
            mDataRegister.removePackage(uid);
        }
    }

    /** Implemented as a class partially for historic reasons, partially to help testing. */
    static class ClearDataStrategy {
        public void execute(
                Context context,
                InstalledWebappDataRegister dataRegister,
                PermissionUpdater permissionUpdater,
                int uid,
                boolean uninstalled) {
            // Retrieving domains and origins ahead of time, because the register is about to be
            // cleaned up.
            Set<String> domains = dataRegister.getDomainsForRegisteredUid(uid);
            Set<String> origins = dataRegister.getOriginsForRegisteredUid(uid);

            for (String originAsString : origins) {
                Origin origin = Origin.create(originAsString);
                if (origin != null) permissionUpdater.onClientAppUninstalled(origin);
            }

            String appName = dataRegister.getAppNameForRegisteredUid(uid);
            Intent intent =
                    ClearDataDialogActivity.createIntent(
                            context, appName, domains, origins, uninstalled);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT);
            context.startActivity(intent);
        }
    }
}
