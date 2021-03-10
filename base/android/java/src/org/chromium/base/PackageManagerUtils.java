// Copyright 2019 The Chromium Authors. All rights reserved.
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

/**
 * This class provides Android PackageManager related utility methods.
 */
public class PackageManagerUtils {
    private static final String TAG = "PackageManagerUtils";
    private static final String SAMPLE_URL = "http://";

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
     * @return Intent to query a list of installed web browsers.
     */
    public static Intent getQueryInstalledBrowsersIntent() {
        return new Intent(Intent.ACTION_VIEW, Uri.parse(SAMPLE_URL))
                .addCategory(Intent.CATEGORY_BROWSABLE);
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
        return resolveActivity(getQueryInstalledBrowsersIntent(), 0);
    }

    /**
     * @return The list of names of web browser applications available in the system. A browser
     *         may appear twice if it has multiple intent handlers.
     */
    public static List<ResolveInfo> queryAllWebBrowsersInfo() {
        return queryIntentActivities(getQueryInstalledBrowsersIntent(), PackageManager.MATCH_ALL);
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
