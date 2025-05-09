// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;


import android.content.Context;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

import java.util.ArrayList;
import java.util.concurrent.ExecutionException;

/**
 * A utility class for querying information about the default browser setting.\
 * TODO(crbug.com/40709747): Consolidate this with DefaultBrowserInfo in c/b/util.
 */
@NullMarked
public final class DefaultBrowserInfo {

    /** A lock to synchronize background tasks to retrieve browser information. */
    private static final Object sDirCreationLock = new Object();

    private static @Nullable AsyncTask<ArrayList<String>> sDefaultBrowserFetcher;

    /** Don't instantiate me. */
    private DefaultBrowserInfo() {}

    /** Initialize an AsyncTask for getting menu title of opening a link in default browser. */
    @EnsuresNonNull("sDefaultBrowserFetcher")
    public static void initBrowserFetcher() {
        synchronized (sDirCreationLock) {
            if (sDefaultBrowserFetcher == null) {
                sDefaultBrowserFetcher =
                        new BackgroundOnlyAsyncTask<ArrayList<String>>() {
                            @Override
                            protected ArrayList<String> doInBackground() {
                                Context context = ContextUtils.getApplicationContext();
                                ArrayList<String> menuTitles = new ArrayList<String>(2);
                                // Store the package label of current application.
                                menuTitles.add(
                                        getTitleFromPackageLabel(
                                                context, BuildInfo.getInstance().hostPackageLabel));

                                PackageManager pm = context.getPackageManager();
                                ResolveInfo info =
                                        PackageManagerUtils.resolveDefaultWebBrowserActivity();

                                // Caches whether Chrome is set as a default browser on the device.
                                boolean isDefault =
                                        info != null
                                                && info.match != 0
                                                && TextUtils.equals(
                                                        context.getPackageName(),
                                                        info.activityInfo.packageName);
                                ChromeSharedPreferences.getInstance()
                                        .writeBoolean(
                                                ChromePreferenceKeys.CHROME_DEFAULT_BROWSER,
                                                isDefault);

                                // Check if there is a default handler for the Intent.  If so, store
                                // its label.
                                String packageLabel = null;
                                if (info != null && info.match != 0 && info.loadLabel(pm) != null) {
                                    packageLabel = info.loadLabel(pm).toString();
                                }
                                menuTitles.add(getTitleFromPackageLabel(context, packageLabel));
                                return menuTitles;
                            }
                        };
                // USER_BLOCKING since we eventually .get() this.
                sDefaultBrowserFetcher.executeWithTaskTraits(TaskTraits.USER_BLOCKING_MAY_BLOCK);
            }
        }
    }

    private static String getTitleFromPackageLabel(Context context, @Nullable String packageLabel) {
        return packageLabel == null
                ? context.getString(R.string.menu_open_in_product_default)
                : context.getString(R.string.menu_open_in_product, packageLabel);
    }

    /**
     * @return Title of the menu item for opening a link in the default browser.
     * @param forceChromeAsDefault Whether the Custom Tab is created by Chrome.
     */
    public static String getTitleOpenInDefaultBrowser(final boolean forceChromeAsDefault) {
        if (sDefaultBrowserFetcher == null) {
            initBrowserFetcher();
        }
        try {
            // If the Custom Tab was created by Chrome, Chrome should handle the action for the
            // overflow menu.
            return forceChromeAsDefault
                    ? sDefaultBrowserFetcher.get().get(0)
                    : sDefaultBrowserFetcher.get().get(1);
        } catch (InterruptedException | ExecutionException e) {
            return ContextUtils.getApplicationContext()
                    .getString(R.string.menu_open_in_product_default);
        }
    }
}
