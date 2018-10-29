// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.Context;
import android.content.SharedPreferences;
import android.os.Build;

import org.chromium.base.BuildInfo;
import org.chromium.base.ThreadUtils;
import org.chromium.base.VisibleForTesting;
import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * Stubbed class for getting version numbers from the rest of Chrome.  Override the functions for
 * unit tests.
 */
public class VersionNumberGetter {

    private static final class LazyHolder {
        private static final VersionNumberGetter INSTANCE = new VersionNumberGetter();
    }

    @VisibleForTesting
    static VersionNumberGetter getInstance() {
        assert !ThreadUtils.runningOnUiThread();
        return sInstanceForTests == null ? LazyHolder.INSTANCE : sInstanceForTests;
    }

    @VisibleForTesting
    static void setInstanceForTests(VersionNumberGetter getter) {
        sInstanceForTests = getter;
    }

    @VisibleForTesting
    public static void setEnableUpdateDetection(boolean state) {
        sEnableUpdateDetection = state;
    }

    private static VersionNumberGetter sInstanceForTests;

    /** If false, OmahaClient will never report that a newer version is available. */
    private static boolean sEnableUpdateDetection = true;

    protected VersionNumberGetter() { }

    /**
     * Retrieve the latest version we know about from disk.
     * This function incurs I/O, so make sure you don't use it from the main thread.
     * @return The latest version if we retrieved one from the Omaha server, or "" if we haven't.
     */
    public String getLatestKnownVersion(Context context) {
        assert !ThreadUtils.runningOnUiThread();
        SharedPreferences prefs = OmahaBase.getSharedPreferences(context);
        return prefs.getString(OmahaBase.PREF_LATEST_VERSION, "");
    }

    /**
     * Retrieve the version of Chrome we're using.
     * @return The latest version if we retrieved one from the Omaha server, or "" if we haven't.
     */
    public String getCurrentlyUsedVersion(Context context) {
        return BuildInfo.getInstance().versionName;
    }

    /**
     * Gets the milestone from an AboutVersionStrings#getApplicationVersion string. These strings
     * are of the format "ProductName xx.xx.xx.xx".
     *
     * @param version The version to extract the milestone number from.
     * @return The milestone of the given version string.
     */
    public static int getMilestoneFromVersionNumber(String version) {
        if (version.isEmpty()) {
            throw new IllegalArgumentException("Application version incorrectly formatted");
        }

        version = version.replaceAll("[^\\d.]", "");

        // Parse out the version numbers.
        String[] pieces = version.split("\\.");
        if (pieces.length != 4) {
            throw new IllegalArgumentException("Application version incorrectly formatted");
        }

        try {
            return Integer.parseInt(pieces[0]);
        } catch (NumberFormatException e) {
            throw new IllegalArgumentException("Application version incorrectly formatted");
        }
    }

    /**
     * @return Whether the current Android OS version is supported.
     */
    public static boolean isCurrentOsVersionSupported() {
        // By default, only Android KitKat and above is supported.
        int oldestSupportedVersion = Build.VERSION_CODES.KITKAT;

        if (ChromeFeatureList.isEnabled(ChromeFeatureList.JELLY_BEAN_SUPPORTED)) {
            oldestSupportedVersion = Build.VERSION_CODES.JELLY_BEAN;
        }
        return Build.VERSION.SDK_INT >= oldestSupportedVersion;
    }

    /**
     * Checks if we know about a newer version available than the one we're using.  This does not
     * actually fire any requests over to the server: it just checks the version we stored the last
     * time we talked to the Omaha server.
     *
     * NOTE: This function incurs I/O, so don't use it on the main thread.
     */
    static boolean isNewerVersionAvailable(Context context) {
        assert !ThreadUtils.runningOnUiThread();

        // This may be explicitly enabled for some channels and for unit tests.
        if (!sEnableUpdateDetection) {
            return false;
        }

        // If the market link is bad, don't show an update to avoid frustrating users trying to
        // hit the "Update" button.
        if ("".equals(MarketURLGetter.getMarketUrl(context))) {
            return false;
        }

        // Compare version numbers.
        VersionNumberGetter getter = getInstance();
        String currentStr = getter.getCurrentlyUsedVersion(context);
        String latestStr = getter.getLatestKnownVersion(context);

        VersionNumber currentVersionNumber = VersionNumber.fromString(currentStr);
        VersionNumber latestVersionNumber = VersionNumber.fromString(latestStr);

        if (currentVersionNumber == null || latestVersionNumber == null) {
            return false;
        }

        return currentVersionNumber.isSmallerThan(latestVersionNumber);
    }
}
