// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing;

import androidx.annotation.Nullable;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Creates/provides {@link InstantMessageDelegateImpl} by {@link Profile}. */
public final class InstantMessageDelegateFactory {
    private static ProfileKeyedMap<InstantMessageDelegateImpl> sProfileMap;
    private static @Nullable InstantMessageDelegateImpl sInstantMessageDelegateImplForTesting;

    // No instantiation.
    private InstantMessageDelegateFactory() {}

    /**
     * A factory method to create or retrieve a {@link InstantMessageDelegateImpl} object for a
     * given profile.
     *
     * @return The {@link InstantMessageDelegateImpl} for the given profile.
     */
    public static InstantMessageDelegateImpl getForProfile(Profile profile) {
        ThreadUtils.assertOnUiThread();
        if (sInstantMessageDelegateImplForTesting != null) {
            return sInstantMessageDelegateImplForTesting;
        }

        if (sProfileMap == null) {
            sProfileMap = new ProfileKeyedMap<>(ProfileKeyedMap.NO_REQUIRED_CLEANUP_ACTION);
        }

        return sProfileMap.getForProfile(profile, InstantMessageDelegateImpl::new);
    }

    /**
     * @param testService The {@InstantMessageDelegateImpl} to use for testing.
     */
    public static void setForTesting(@Nullable InstantMessageDelegateImpl testService) {
        sInstantMessageDelegateImplForTesting = testService;
        ResettersForTesting.register(() -> sInstantMessageDelegateImplForTesting = null);
    }
}
