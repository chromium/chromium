// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.shell_apk;

import android.content.ComponentName;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.os.Bundle;

import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.shadows.ShadowPackageManager;

import java.util.HashSet;
import java.util.Set;

/**
 * Helper class for JUnit tests. Contains utility methods for installing and uninstalling mock
 * browsers.
 */
public class TestBrowserInstaller {
    public static final String COMPONENT_CLASS = "com.component.class.name";

    Set<String> mInstalledBrowsers = new HashSet<>();

    // Intent filter priority for the next installed browser. Each browser gets a lower priority
    // than the browsers installed before it, so the first installed browser resolves first and is
    // the default browser.
    private int mNextBrowserPriority;

    /** Changes the installed browsers to the passed-in list. */
    public void setInstalledModernBrowsers(String defaultBrowserPackage, String[] newPackages) {
        uninstallAllBrowsers();

        installModernBrowser(defaultBrowserPackage);
        if (newPackages != null) {
            for (String newPackage : newPackages) {
                installModernBrowser(newPackage);
            }
        }
    }

    /** Changes the installed browser to a browser with the passed-in package and version name. */
    public void setInstalledBrowserWithVersion(String browser, String versionName) {
        uninstallAllBrowsers();
        installBrowserWithVersion(browser, versionName);
    }

    /** Installs browser with the passed-in package name and large version name. */
    public void installModernBrowser(String packageName) {
        installBrowserWithVersion(packageName, "10000.0.0.0");
    }

    /** Installs browser with the passed-in package name and version name. */
    public void installBrowserWithVersion(String packageName, String versionName) {
        if (mInstalledBrowsers.contains(packageName)) return;

        ShadowPackageManager packageManager = getShadowPackageManager();
        packageManager.installPackage(newPackageInfo(packageName, versionName));
        ComponentName component = new ComponentName(packageName, COMPONENT_CLASS);
        packageManager.addActivityIfNotPresent(component);
        packageManager.addIntentFilterForActivity(
                component, newBrowserIntentFilter(mNextBrowserPriority));
        mNextBrowserPriority--;

        mInstalledBrowsers.add(packageName);
    }

    /** Uninstalls all browsers. */
    public void uninstallAllBrowsers() {
        while (!mInstalledBrowsers.isEmpty()) {
            uninstallBrowser(mInstalledBrowsers.iterator().next());
        }
    }

    /** Uninstalls browser with the given package name. */
    public void uninstallBrowser(String packageName) {
        if (!mInstalledBrowsers.contains(packageName)) return;

        ShadowPackageManager packageManager = getShadowPackageManager();
        // Also drops the intent filter, which removePackage() leaves behind.
        packageManager.removeActivity(new ComponentName(packageName, COMPONENT_CLASS));
        packageManager.removePackage(packageName);

        mInstalledBrowsers.remove(packageName);
    }

    private static ShadowPackageManager getShadowPackageManager() {
        return Shadows.shadowOf(RuntimeEnvironment.getApplication().getPackageManager());
    }

    /** Returns a filter which matches {@link WebApkUtils#getQueryInstalledBrowsersIntent()}. */
    private static IntentFilter newBrowserIntentFilter(int priority) {
        IntentFilter filter = new IntentFilter(Intent.ACTION_VIEW);
        filter.addCategory(Intent.CATEGORY_BROWSABLE);
        // HostBrowserUtils resolves the default browser with MATCH_DEFAULT_ONLY.
        filter.addCategory(Intent.CATEGORY_DEFAULT);
        filter.addDataScheme("http");
        filter.setPriority(priority);
        return filter;
    }

    private static PackageInfo newPackageInfo(String packageName, String versionName) {
        PackageInfo packageInfo = new PackageInfo();
        packageInfo.packageName = packageName;
        packageInfo.versionName = versionName;
        packageInfo.applicationInfo = new ApplicationInfo();
        packageInfo.applicationInfo.packageName = packageName;
        packageInfo.applicationInfo.enabled = true;
        packageInfo.applicationInfo.metaData = new Bundle();
        return packageInfo;
    }
}
