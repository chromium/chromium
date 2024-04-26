// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.text.TextUtils;

import androidx.annotation.IntDef;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.BackgroundOnlyAsyncTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.content_public.browser.BrowserStartupController;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.RejectedExecutionException;

/**
 * A utility class for querying information about the default browser setting.\
 * TODO(crbug.com/40709747): Remove this and replace with DefaultBrowserInfo2.
 */
public final class DefaultBrowserInfo {
    /**
     * A list of potential default browser states. To add a type to this list please update
     * MobileDefaultBrowserState in histograms.xml and make sure to keep this list in sync.
     * Additions should be treated as APPEND ONLY to keep the UMA metric semantics the same over
     * time.
     */
    @IntDef({
        MobileDefaultBrowserState.NO_DEFAULT,
        MobileDefaultBrowserState.CHROME_SYSTEM_DEFAULT,
        MobileDefaultBrowserState.CHROME_INSTALLED_DEFAULT,
        MobileDefaultBrowserState.OTHER_SYSTEM_DEFAULT,
        MobileDefaultBrowserState.OTHER_INSTALLED_DEFAULT
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface MobileDefaultBrowserState {
        int NO_DEFAULT = 0;
        int CHROME_SYSTEM_DEFAULT = 1;
        int CHROME_INSTALLED_DEFAULT = 2;
        int OTHER_SYSTEM_DEFAULT = 3;
        int OTHER_INSTALLED_DEFAULT = 4;
        int NUM_ENTRIES = 5;
    }

    /**
     * Helper class for passing information about the system's default browser settings back from a
     * worker task.
     */
    private static class DefaultInfo {
        public boolean isChromeSystem;
        public boolean isChromeDefault;
        public boolean isDefaultSystem;
        public boolean hasDefault;
        public int browserCount;
        public int systemCount;
    }

    /** A lock to synchronize background tasks to retrieve browser information. */
    private static final Object sDirCreationLock = new Object();

    private static AsyncTask<ArrayList<String>> sDefaultBrowserFetcher;

    /** Don't instantiate me. */
    private DefaultBrowserInfo() {}

    /** Initialize an AsyncTask for getting menu title of opening a link in default browser. */
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

    private static String getTitleFromPackageLabel(Context context, String packageLabel) {
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

    /** Log statistics about the current default browser to UMA. */
    public static void logDefaultBrowserStats() {
        assert BrowserStartupController.getInstance().isFullBrowserStarted();

        try {
            new AsyncTask<DefaultInfo>() {
                @Override
                protected DefaultInfo doInBackground() {
                    Context context = ContextUtils.getApplicationContext();

                    DefaultInfo info = new DefaultInfo();

                    // Query the default handler first.
                    ResolveInfo defaultRi = PackageManagerUtils.resolveDefaultWebBrowserActivity();
                    if (defaultRi != null && defaultRi.match != 0) {
                        info.hasDefault = true;
                        info.isChromeDefault = isSamePackage(context, defaultRi);
                        info.isDefaultSystem = isSystemPackage(defaultRi);
                    }

                    // Query all other intent handlers.
                    Set<String> uniquePackages = new HashSet<>();
                    List<ResolveInfo> ris = PackageManagerUtils.queryAllWebBrowsersInfo();
                    if (ris != null) {
                        for (ResolveInfo ri : ris) {
                            String packageName = ri.activityInfo.packageName;
                            if (!uniquePackages.add(packageName)) continue;

                            if (isSystemPackage(ri)) {
                                if (isSamePackage(context, ri)) info.isChromeSystem = true;
                                info.systemCount++;
                            }
                        }
                    }

                    info.browserCount = uniquePackages.size();

                    return info;
                }

                @Override
                protected void onPostExecute(DefaultInfo info) {
                    if (info == null) return;

                    RecordHistogram.recordCount100Histogram(
                            getSystemBrowserCountUmaName(info), info.systemCount);
                    RecordHistogram.recordCount100Histogram(
                            getDefaultBrowserCountUmaName(info), info.browserCount);
                    RecordHistogram.recordEnumeratedHistogram(
                            "Mobile.DefaultBrowser.State",
                            getDefaultBrowserUmaState(info),
                            MobileDefaultBrowserState.NUM_ENTRIES);
                }
            }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
        } catch (RejectedExecutionException ex) {
            // Fail silently here since this is not a critical task.
        }
    }

    private static String getSystemBrowserCountUmaName(DefaultInfo info) {
        if (info.isChromeSystem) return "Mobile.DefaultBrowser.SystemBrowserCount.ChromeSystem";
        return "Mobile.DefaultBrowser.SystemBrowserCount.ChromeNotSystem";
    }

    private static String getDefaultBrowserCountUmaName(DefaultInfo info) {
        if (!info.hasDefault) return "Mobile.DefaultBrowser.BrowserCount.NoDefault";
        if (info.isChromeDefault) return "Mobile.DefaultBrowser.BrowserCount.ChromeDefault";
        return "Mobile.DefaultBrowser.BrowserCount.OtherDefault";
    }

    private static @MobileDefaultBrowserState int getDefaultBrowserUmaState(DefaultInfo info) {
        if (!info.hasDefault) return MobileDefaultBrowserState.NO_DEFAULT;

        if (info.isChromeDefault) {
            if (info.isDefaultSystem) return MobileDefaultBrowserState.CHROME_SYSTEM_DEFAULT;
            return MobileDefaultBrowserState.CHROME_INSTALLED_DEFAULT;
        }

        if (info.isDefaultSystem) return MobileDefaultBrowserState.OTHER_SYSTEM_DEFAULT;
        return MobileDefaultBrowserState.OTHER_INSTALLED_DEFAULT;
    }

    private static boolean isSamePackage(Context context, ResolveInfo info) {
        return TextUtils.equals(
                context.getPackageName(), info.activityInfo.applicationInfo.packageName);
    }

    private static boolean isSystemPackage(ResolveInfo info) {
        return (info.activityInfo.applicationInfo.flags & ApplicationInfo.FLAG_SYSTEM) != 0;
    }
}
