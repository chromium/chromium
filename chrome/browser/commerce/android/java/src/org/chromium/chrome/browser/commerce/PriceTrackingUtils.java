// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import androidx.annotation.VisibleForTesting;

import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A set of utilities to facilitate price tracking features. This is the Java version of the
 * commerce component's core price tracking utils.
 */
// TODO(crbug.com/40234642): This should live in the commerce component once BookmarkModel is moved
// to its
//                appropriate component.
@JNINamespace("commerce")
public class PriceTrackingUtils {
    /** Private constructor to prevent initialization. */
    private PriceTrackingUtils() {}

    public static void setPriceTrackingStateForBookmark(
            Profile profile, long bookmarkId, boolean enabled, Callback<Boolean> callback) {
        setPriceTrackingStateForBookmark(profile, bookmarkId, enabled, callback, false);
    }

    public static void setPriceTrackingStateForBookmark(
            Profile profile,
            long bookmarkId,
            boolean enabled,
            Callback<Boolean> callback,
            boolean bookmarkCreatedForPriceTracking) {
        PriceTrackingUtilsJni.get()
                .setPriceTrackingStateForBookmark(
                        profile, bookmarkId, enabled, callback, bookmarkCreatedForPriceTracking);
    }

    public static void isBookmarkPriceTracked(
            Profile profile, long bookmarkId, Callback<Boolean> callback) {
        PriceTrackingUtilsJni.get().isBookmarkPriceTracked(profile, bookmarkId, callback);
    }

    @NativeMethods
    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    public interface Natives {
        void setPriceTrackingStateForBookmark(
                @JniType("Profile*") Profile profile,
                long bookmarkId,
                boolean enabled,
                Callback<Boolean> callback,
                boolean bookmarkCreatedForPriceTracking);

        void isBookmarkPriceTracked(
                @JniType("Profile*") Profile profile, long bookmarkId, Callback<Boolean> callback);
    }
}
