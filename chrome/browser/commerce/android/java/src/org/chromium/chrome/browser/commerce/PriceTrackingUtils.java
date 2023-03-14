// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.profiles.Profile;

/**
 * A set of utilities to facilitate price tracking features. This is the Java version of the
 * commerce component's core price tracking utils.
 */
// TODO(1351830): This should live in the commerce component once BookmarkModel is moved to its
//                appropriate component.
@JNINamespace("commerce")
public class PriceTrackingUtils {
    /** Private constructor to prevent initialization. */
    private PriceTrackingUtils() {}

    public static void setPriceTrackingStateForBookmark(
            Profile profile, long bookmarkId, boolean enabled, Callback<Boolean> callback) {
        setPriceTrackingStateForBookmark(profile, bookmarkId, enabled, callback, false);
    }

    public static void setPriceTrackingStateForBookmark(Profile profile, long bookmarkId,
            boolean enabled, Callback<Boolean> callback, boolean bookmarkCreatedForPriceTracking) {
        PriceTrackingUtilsJni.get().setPriceTrackingStateForBookmark(
                profile, bookmarkId, enabled, callback, bookmarkCreatedForPriceTracking);
    }

    public static void isBookmarkPriceTracked(
            Profile profile, long bookmarkId, Callback<Boolean> callback) {
        PriceTrackingUtilsJni.get().isBookmarkPriceTracked(profile, bookmarkId, callback);
    }

    @NativeMethods
    @VisibleForTesting
    public interface Natives {
        void setPriceTrackingStateForBookmark(Profile profile, long bookmarkId, boolean enabled,
                Callback<Boolean> callback, boolean bookmarkCreatedForPriceTracking);
        void isBookmarkPriceTracked(Profile profile, long bookmarkId, Callback<Boolean> callback);
    }
}
