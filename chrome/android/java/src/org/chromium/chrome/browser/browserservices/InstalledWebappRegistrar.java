// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.browserservices.permissiondelegation.PermissionUpdater;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.HashSet;
import java.util.Set;

/**
 * Records in all the appropriate places that an installed webapp (TWA or WebAPK) has successfully
 * been verified.
 */
public class InstalledWebappRegistrar {
    private static final String TAG = "WebappRegistrar";

    // These origins have already been registered so we don't need to do so again.
    private final Set<Origin> mRegisteredOrigins = new HashSet<>();

    /**
     * Cache so we don't send the same request multiple times. {@link #register} is called on each
     * navigation and each call to {@link InstalledWebappDataRegister#registerPackageForOrigin}
     * modifies SharedPreferences, so we need to cut down on the number of calls.
     */
    private final Set<String> mCache = new HashSet<>();

    private static InstalledWebappRegistrar sInstance;

    public static InstalledWebappRegistrar getInstance() {
        if (sInstance == null) sInstance = new InstalledWebappRegistrar();
        return sInstance;
    }

    /* pacakge */ InstalledWebappRegistrar() {
        ThreadUtils.assertOnUiThread();
    }

    /**
     * Registers to various stores that the app is linked with the origin.
     *
     * <p>We do this here, when the Trusted Web Activity UI is shown instead of in
     * ChromeOriginVerifier when verification completes because when an origin is being verified, we
     * don't know whether it is for the purposes of Trusted Web Activities or for Post Message
     * (where this behaviour is not required).
     *
     * <p>Additionally we do it on every page navigation because an app can be verified for more
     * than one Origin, eg: <br>
     * 1) App verifies with https://www.myfirsttwa.com/. <br>
     * 2) App verifies with https://www.mysecondtwa.com/. <br>
     * 3) App launches a TWA to https://www.myfirsttwa.com/. <br>
     * 4) App navigates to https://www.mysecondtwa.com/.
     *
     * <p>At step 2, we don't know why the app is verifying with that origin (it could be for TWAs
     * or for PostMessage). Only at step 4 do we know that Chrome should associate the browsing data
     * for that origin with that app.
     */
    public void registerClient(String packageName, Origin origin, String pageUrl) {
        ThreadUtils.assertOnUiThread();
        if (mRegisteredOrigins.contains(origin)) return;

        // Register that we should wipe data for this origin when the client app is uninstalled.
        register(packageName, origin);
        // Register that we trust the client app to handle permission for this origin and update
        // Chrome's permission for the origin.
        PermissionUpdater.onOriginVerified(origin, pageUrl, packageName);

        mRegisteredOrigins.add(origin);
    }

    /**
     * Calls {@link InstalledWebappDataRegister#registerPackageForOrigin}, looking up the uid and
     * app name for the |packageName|, extracting the domain from the origin and deduplicating
     * multiple requests with the same parameters. Requires native to be loaded.
     */
    private void register(String packageName, Origin origin) {
        if (!mCache.add(combine(packageName, origin))) return;

        try {
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            ApplicationInfo ai = pm.getApplicationInfo(packageName, 0);
            String appLabel = pm.getApplicationLabel(ai).toString();

            if (TextUtils.isEmpty(appLabel) || ai.uid == -1) {
                Log.e(
                        TAG,
                        "Invalid details for client package %s: %d, %s",
                        packageName,
                        ai.uid,
                        appLabel);
                return;
            }

            String domain =
                    UrlUtilities.getDomainAndRegistry(
                            origin.toString(), /* includePrivateRegistries= */ true);

            Log.d(TAG, "Registering %d (%s) for %s", ai.uid, appLabel, origin);
            InstalledWebappDataRegister.registerPackageForOrigin(
                    ai.uid, appLabel, packageName, domain, origin);
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Couldn't find name for client package %s", packageName);
        }
    }

    private static String combine(String packageName, Origin origin) {
        return packageName + ":" + origin.toString();
    }
}
