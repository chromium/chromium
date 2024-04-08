// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.os.TransactionTooLargeException;

import java.util.Collections;
import java.util.List;

/** This class provides Android PackageManager related utility methods. */
public class PackageManagerUtils {
    public static final String XR_IMMERSIVE_FEATURE_NAME = "android.software.xr.immersive";

    private static final String TAG = "PackageManagerUtils";

    // This is the intent Android uses internally to detect browser apps.
    // See
    // https://cs.android.com/android/_/android/platform/packages/modules/Permission/+/android12-release:PermissionController/src/com/android/permissioncontroller/role/model/BrowserRoleBehavior.java;drc=86fa7d5dfa43f66b170f93ade4f59b9a770be32f;l=50
    public static final Intent BROWSER_INTENT =
            new Intent()
                    .setAction(Intent.ACTION_VIEW)
                    .addCategory(Intent.CATEGORY_BROWSABLE)
                    .setData(Uri.fromParts("http", "", null));

    /**
     * Retrieve information about the Activity that will handle the given Intent.
     *
     * Note: This function is expensive on KK and below and should not be called from main thread
     * when avoidable.
     *
     * @param intent Intent to resolve.
     * @param flags The PackageManager flags to pass to resolveActivity().
     * @return       ResolveInfo of the Activity that will handle the Intent, or null if it failed.
     */
    public static ResolveInfo resolveActivity(Intent intent, int flags) {
        // On KitKat, calling PackageManager#resolveActivity() causes disk reads and
        // writes. Temporarily allow this while resolving the intent.
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            return pm.resolveActivity(intent, flags);
        } catch (RuntimeException e) {
            handleExpectedExceptionsOrRethrow(e, intent);
        }
        return null;
    }

    /**
     * Get the list of component name of activities which can resolve |intent|.  If the request
     * fails, an empty list will be returned.
     *
     * See {@link PackageManager#queryIntentActivities(Intent, int)}
     */
    public static List<ResolveInfo> queryIntentActivities(Intent intent, int flags) {
        // Allowlist for Samsung. See http://crbug.com/613977 and https://crbug.com/894160 for more
        // context.
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
            return pm.queryIntentActivities(intent, flags);
        } catch (RuntimeException e) {
            handleExpectedExceptionsOrRethrow(e, intent);
        }
        return Collections.emptyList();
    }

    /**
     * Check if the given Intent can be resolved by any Activities on the system.
     *
     * See {@link PackageManagerUtils#queryIntentActivities(Intent, int)}
     */
    public static boolean canResolveActivity(Intent intent, int flags) {
        return !queryIntentActivities(intent, flags).isEmpty();
    }

    /**
     * Check if the given Intent can be resolved by any Activities on the system.
     *
     * See {@link PackageManagerUtils#canResolveActivity(Intent, int)}
     */
    public static boolean canResolveActivity(Intent intent) {
        return canResolveActivity(intent, 0);
    }

    /** Check if the system has the given system feature available. */
    public static boolean hasSystemFeature(String feature) {
        PackageManager pm = ContextUtils.getApplicationContext().getPackageManager();
        return pm.hasSystemFeature(feature);
    }

    /**
     * @return Intent to query a list of installed home launchers.
     */
    public static Intent getQueryInstalledHomeLaunchersIntent() {
        return new Intent(Intent.ACTION_MAIN).addCategory(Intent.CATEGORY_HOME);
    }

    /**
     * @return Default ResolveInfo to handle a VIEW intent for a url.
     */
    public static ResolveInfo resolveDefaultWebBrowserActivity() {
        return resolveActivity(BROWSER_INTENT, PackageManager.MATCH_DEFAULT_ONLY);
    }

    /**
     * @return The list of names of web browser applications available in the system. A browser
     *         may appear twice if it has multiple intent handlers.
     */
    public static List<ResolveInfo> queryAllWebBrowsersInfo() {
        // Copying these flags from Android source for detecting the list of installed browsers.
        // Apparently MATCH_ALL doesn't include MATCH_DIRECT_BOOT_*.
        // See
        // https://cs.android.com/android/_/android/platform/packages/modules/Permission/+/android12-release:PermissionController/src/com/android/permissioncontroller/role/model/BrowserRoleBehavior.java;drc=86fa7d5dfa43f66b170f93ade4f59b9a770be32f;l=114
        int flags =
                PackageManager.MATCH_ALL
                        | PackageManager.MATCH_DIRECT_BOOT_AWARE
                        | PackageManager.MATCH_DIRECT_BOOT_UNAWARE
                        | PackageManager.MATCH_DEFAULT_ONLY;
        return queryIntentActivities(BROWSER_INTENT, flags);
    }

    /**
     * @return The list of names of system launcher applications available in the system.
     */
    public static List<ResolveInfo> queryAllLaunchersInfo() {
        return queryIntentActivities(
                getQueryInstalledHomeLaunchersIntent(), PackageManager.MATCH_ALL);
    }

    // See https://crbug.com/700505 and https://crbug.com/369574.
    private static void handleExpectedExceptionsOrRethrow(RuntimeException e, Intent intent) {
        if (e instanceof NullPointerException
                || e.getCause() instanceof TransactionTooLargeException) {
            Log.e(TAG, "Could not resolve Activity for intent " + intent.toString(), e);
        } else {
            throw e;
        }
    }
}
