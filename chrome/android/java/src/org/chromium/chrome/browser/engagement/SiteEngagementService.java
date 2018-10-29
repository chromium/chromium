// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.engagement;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * Provides access to the Site Engagement Service for a profile.
 *
 * Site engagement measures the level of engagement that a user has with an origin. This class
 * allows Java to retrieve and modify engagement scores for URLs.
 */
public class SiteEngagementService {

    /** Pointer to the native side SiteEngagementServiceAndroid shim. */
    private long mNativePointer;

    /**
     * Returns a SiteEngagementService for the provided profile.
     * Must be called on the UI thread.
     */
    public static SiteEngagementService getForProfile(Profile profile) {
        assert ThreadUtils.runningOnUiThread();
        return nativeSiteEngagementServiceForProfile(profile);
    }

    /**
     * Returns the engagement score for the provided URL.
     * Must be called on the UI thread.
     */
    public double getScore(String url) {
        assert ThreadUtils.runningOnUiThread();
        if (mNativePointer == 0) return 0.0;
        return nativeGetScore(mNativePointer, url);
    }

    /**
     * Sets the provided URL to have the provided engagement score.
     * Must be called on the UI thread.
     */
    public void resetBaseScoreForUrl(String url, double score) {
        assert ThreadUtils.runningOnUiThread();
        if (mNativePointer == 0) return;
        nativeResetBaseScoreForURL(mNativePointer, url, score);
    }

    /**
     * Sets site engagement param values to constants for testing.
     */
    public static void setParamValuesForTesting() {
        nativeSetParamValuesForTesting();
    }

    @CalledByNative
    private static SiteEngagementService create(long nativePointer) {
        return new SiteEngagementService(nativePointer);
    }

    /** This object may only be created via the static getForProfile method. */
    private SiteEngagementService(long nativePointer) {
        mNativePointer = nativePointer;
    }

    @CalledByNative
    private void onNativeDestroyed() {
        mNativePointer = 0;
    }

    private static native SiteEngagementService nativeSiteEngagementServiceForProfile(
            Profile profile);
    private static native void nativeSetParamValuesForTesting();
    private native double nativeGetScore(long nativeSiteEngagementServiceAndroid, String url);
    private native void nativeResetBaseScoreForURL(
            long nativeSiteEngagementServiceAndroid, String url, double score);
}
