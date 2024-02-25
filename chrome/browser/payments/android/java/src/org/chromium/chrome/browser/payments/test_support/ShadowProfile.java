// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.test_support;

import androidx.annotation.Nullable;

import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.Resetter;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.content_public.browser.WebContents;

/** The shadow of Profile. */
@Implements(Profile.class)
public class ShadowProfile {
    private static Profile sProfile;

    /**
     * Set the profile to be returned for {@link #fromWebContents}.
     * @param profile The profile to be returned.
     */
    public static void setProfile(Profile profile) {
        sProfile = profile;
    }

    @Resetter
    public static void reset() {
        sProfile = null;
    }

    @Implementation
    public static @Nullable Profile fromWebContents(WebContents webContents) {
        return sProfile;
    }
}
