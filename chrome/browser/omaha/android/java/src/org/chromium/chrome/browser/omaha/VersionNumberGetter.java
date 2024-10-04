// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha;

import android.content.SharedPreferences;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;

/**
 * Stubbed class for getting version numbers from the rest of Chrome. Override the functions for
 * unit tests.
 */
public class VersionNumberGetter {
    private static final String MIN_SDK_VERSION_PARAM = "min_sdk_version";
    public static final IntCachedFieldTrialParameter MIN_SDK_VERSION =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.OMAHA_MIN_SDK_VERSION_ANDROID,
                    MIN_SDK_VERSION_PARAM,
                    ContextUtils.getApplicationContext().getApplicationInfo().minSdkVersion);

    private static VersionNumberGetter sInstance = new VersionNumberGetter();

    /** If true, OmahaClient will never report that a newer version is available. */
    private static boolean sDisableUpdateDetectionForTesting;

    @VisibleForTesting
    static VersionNumberGetter getInstance() {
        assert !ThreadUtils.runningOnUiThread();
        return sInstance;
    }

    static void setInstanceForTests(VersionNumberGetter getter) {
        var prevInstance = sInstance;
        sInstance = getter;
        ResettersForTesting.register(() -> sInstance = prevInstance);
    }

    public static void setEnableUpdateDetectionForTesting(boolean state) {
        sDisableUpdateDetectionForTesting = !state;
        ResettersForTesting.register(() -> sDisableUpdateDetectionForTesting = false);
    }

    protected VersionNumberGetter() {}

    /**
     * Retrieve the latest version we know about from disk.
     * This function incurs I/O, so make sure you don't use it from the main thread.
     * @return The latest version if we retrieved one from the Omaha server, or "" if we haven't.
     */
    public String getLatestKnownVersion() {
        assert !ThreadUtils.runningOnUiThread();
        SharedPreferences prefs = OmahaPrefUtils.getSharedPreferences();
        return prefs.getString(OmahaPrefUtils.PREF_LATEST_VERSION, "");
    }

    /**
     * Retrieve the version of Chrome we're using.
     * @return The latest version if we retrieved one from the Omaha server, or "" if we haven't.
     */
    public String getCurrentlyUsedVersion() {
        return BuildInfo.getInstance().versionName;
    }

    /**
     * @return Whether the current Android OS version is supported.
     */
    public static boolean isCurrentOsVersionSupported() {
        return Build.VERSION.SDK_INT >= MIN_SDK_VERSION.getValue();
    }

    /**
     * Checks if we know about a newer version available than the one we're using.  This does not
     * actually fire any requests over to the server: it just checks the version we stored the last
     * time we talked to the Omaha server.
     *
     * NOTE: This function incurs I/O, so don't use it on the main thread.
     */
    static boolean isNewerVersionAvailable() {
        assert !ThreadUtils.runningOnUiThread();

        // This may be explicitly enabled for some channels and for unit tests.
        if (sDisableUpdateDetectionForTesting) {
            return false;
        }

        // If the market link is bad, don't show an update to avoid frustrating users trying to
        // hit the "Update" button.
        if ("".equals(MarketURLGetter.getMarketUrl())) {
            return false;
        }

        // Compare version numbers.
        VersionNumberGetter getter = getInstance();
        String currentStr = getter.getCurrentlyUsedVersion();
        String latestStr = getter.getLatestKnownVersion();

        VersionNumber currentVersionNumber = VersionNumber.fromString(currentStr);
        VersionNumber latestVersionNumber = VersionNumber.fromString(latestStr);

        if (currentVersionNumber == null || latestVersionNumber == null) {
            return false;
        }

        return currentVersionNumber.isSmallerThan(latestVersionNumber);
    }
}
