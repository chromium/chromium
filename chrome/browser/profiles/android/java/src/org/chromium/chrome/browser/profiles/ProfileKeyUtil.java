// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.BrowserStartupController;

/** Utilities for interacting with ProfileKeys. */
@NullMarked
public class ProfileKeyUtil {
    private ProfileKeyUtil() {}

    /**
     * Returns the regular (i.e., not off-the-record) profile key.
     *
     * <p>Note: The function name uses the "last used" terminology for consistency with
     * profile_manager.cc which supports multiple regular profiles.
     *
     * @deprecated There is no single regular profile once Chrome supports multiple profiles. Use
     *     {@link Profile#getProfileKey()} when a {@link Profile} is available, or resolve the
     *     caller's profile token with {@link ProfileResolver#resolveProfileKey} in reduced mode.
     */
    @Deprecated
    public static ProfileKey getLastUsedRegularProfileKey() {
        assert BrowserStartupController.getInstance().isNativeStarted();
        return ProfileKeyUtilJni.get().getLastUsedRegularProfileKey();
    }

    @NativeMethods
    interface Natives {
        ProfileKey getLastUsedRegularProfileKey();
    }
}
