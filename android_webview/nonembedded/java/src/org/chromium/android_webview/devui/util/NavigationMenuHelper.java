// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.android_webview.devui.util;

import android.app.Activity;
import android.content.Intent;
import android.view.Menu;
import android.view.MenuItem;

import org.chromium.android_webview.devui.CrashesListActivity;
import org.chromium.android_webview.devui.FlagsActivity;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;

/**
 * Helper class for navigation menu between activities.
 *
 * TODO(crbug.com/1017532) should be replaced with a navigation drawer.
 */
public final class NavigationMenuHelper {
    /**
     * Inflate the navigation menu in the given {@code activity} options menu.
     *
     * This should be called inside {@code Activity#onCreateOptionsMenu} method.
     */
    public static void inflate(Activity activity, Menu menu) {
        activity.getMenuInflater().inflate(R.menu.navigation_menu, menu);
    }

    /**
     * Handle {@code onOptionsItemSelected} event for navigation menu.
     *
     * This should be called inside {@code Activity#onOptionsItemSelected} method.
     * @return {@code true} if the item selection event is consumed.
     */
    public static boolean onOptionsItemSelected(Activity activity, MenuItem item) {
        if (item.getItemId() == R.id.nav_menu_crash_ui) {
            activity.startActivity(new Intent(activity, CrashesListActivity.class));
        } else if (item.getItemId() == R.id.nav_menu_flags_ui) {
            activity.startActivity(new Intent(activity, FlagsActivity.class));
        } else if (item.getItemId() == R.id.nav_menu_main_ui) {
            activity.startActivity(new Intent(activity, MainActivity.class));
        } else {
            return false;
        }
        return true;
    }

    // Do not instantiate this class.
    private NavigationMenuHelper() {}
}
