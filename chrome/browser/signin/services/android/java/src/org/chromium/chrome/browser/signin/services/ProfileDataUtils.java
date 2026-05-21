// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.signin.services;

import org.chromium.base.Promise;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

import java.util.Collections;
import java.util.List;

/** ProfileDataUtils groups some static util methods for DisplayableProfileData. */
@NullMarked
public class ProfileDataUtils {
    private ProfileDataUtils() {}

    /**
     * Gets the cached list of {@link DisplayableProfileData} from the given {@link Promise}. If the
     * cache is not yet populated, return an empty list.
     */
    public static List<DisplayableProfileData> getProfileDataIfFulfilledOrEmpty(
            Promise<List<DisplayableProfileData>> promise) {
        return promise.isFulfilled() ? promise.getResult() : Collections.emptyList();
    }

    /**
     * Gets the cached default {@link DisplayableProfileData} from the given {@link Promise}. If the
     * cache is not yet populated or no accounts exist, return null.
     *
     * @param promise The promise containing the list of profile data.
     * @return The first {@link DisplayableProfileData} if the promise is fulfilled and not empty,
     *     otherwise null.
     */
    public static @Nullable DisplayableProfileData getFirstIfFulfilledAndNotEmpty(
            Promise<List<DisplayableProfileData>> promise) {
        if (promise.isFulfilled()) {
            List<DisplayableProfileData> profileData = promise.getResult();
            if (!profileData.isEmpty()) {
                return profileData.get(0);
            }
        }
        return null;
    }
}
