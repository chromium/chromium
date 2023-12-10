// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.Context;
import android.content.SharedPreferences;

/**
 * Helper for using application level {@link SharedPreferences} in a consistent way, with the same
 * file name and using the application context.
 */
public class PrefUtils {
    private PrefUtils() {}

    private static final String SHARED_PREFERENCES_NAME = "org.chromium.webapk.shell_apk.PrefUtils";
    private static final String KEY_HAS_REQUESTED_NOTIFICATION_PERMISSION =
            "HAS_REQUESTED_NOTIFICATION_PERMISSION";

    /** Returns the application level {@link SharedPreferences} using the application context. */
    public static SharedPreferences getAppSharedPreferences(Context context) {
        return context.getApplicationContext()
                .getSharedPreferences(SHARED_PREFERENCES_NAME, Context.MODE_PRIVATE);
    }

    public static boolean hasRequestedNotificationPermission(Context context) {
        return getAppSharedPreferences(context)
                .getBoolean(KEY_HAS_REQUESTED_NOTIFICATION_PERMISSION, false);
    }

    public static void setHasRequestedNotificationPermission(Context context) {
        getAppSharedPreferences(context)
                .edit()
                .putBoolean(KEY_HAS_REQUESTED_NOTIFICATION_PERMISSION, true)
                .apply();
    }
}
