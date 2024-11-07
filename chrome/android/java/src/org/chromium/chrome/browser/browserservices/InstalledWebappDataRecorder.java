// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.text.TextUtils;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.components.embedder_support.util.Origin;
import org.chromium.components.embedder_support.util.UrlUtilities;

import java.util.HashSet;
import java.util.Set;

import javax.inject.Inject;

/**
 * Takes care of recording that Chrome contains data for the installed webapp (TWA or WebAPK) in the
 * {@link InstalledWebappDataRegister}. It performs three main duties: - Holds a cache to
 * deduplicate requests (for performance not correctness). - Transforming the package name into a
 * uid and app label. - Transforming the origin into a domain (requires native).
 *
 * <p>Lifecycle: There should be a 1-1 relationship between this class and {@link
 * CurrentPageVerifier}. Having more instances won't effect correctness, but will limit the
 * performance benefits of the cache. Thread safety: All methods on this class should be called from
 * the same thread.
 */
@ActivityScope
public class InstalledWebappDataRecorder {
    private static final String TAG = "DataRecorder";

    /** Underlying data register. */
    private final InstalledWebappDataRegister mDataRegister;

    /**
     * Cache so we don't send the same request multiple times. {@link #register} is called on each
     * navigation and each call to {@link InstalledWebappDataRegister#registerPackageForOrigin}
     * modifies SharedPreferences, so we need to cut down on the number of calls.
     */
    private final Set<String> mCache = new HashSet<>();

    @Inject
    InstalledWebappDataRecorder(InstalledWebappDataRegister dataRegister) {
        mDataRegister = dataRegister;
    }

    /**
     * Calls {@link InstalledWebappDataRegister#registerPackageForOrigin}, looking up the uid
     * and app name for the |packageName|, extracting the domain from the origin and deduplicating
     * multiple requests with the same parameters.
     * Requires native to be loaded.
     */
    /* package */ void register(String packageName, Origin origin) {
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
            mDataRegister.registerPackageForOrigin(ai.uid, appLabel, packageName, domain, origin);
        } catch (PackageManager.NameNotFoundException e) {
            Log.e(TAG, "Couldn't find name for client package %s", packageName);
        }
    }

    private static String combine(String packageName, Origin origin) {
        return packageName + ":" + origin.toString();
    }
}
