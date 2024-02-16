// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.jni_zero.NativeMethods;

/** Utilities for interacting with ProfileKeys. */
public class ProfileKeyUtil {
    private ProfileKeyUtil() {}

    /**
     * Returns the regular (i.e., not off-the-record) profile key.
     *
     * <p>Note: The function name uses the "last used" terminology for consistency with
     * profile_manager.cc which supports multiple regular profiles.
     */
    public static ProfileKey getLastUsedRegularProfileKey() {
        // TODO(mheikal): Assert at least reduced mode is started when https://crbug.com/973241 is
        // fixed.
        return ProfileKeyUtilJni.get().getLastUsedRegularProfileKey();
    }

    @NativeMethods
    interface Natives {
        ProfileKey getLastUsedRegularProfileKey();
    }
}
