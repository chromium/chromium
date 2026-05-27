// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.lens;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.identitymanager.IdentityManager;

/** Static utility methods to support Lens identity matching. */
@NullMarked
public class LensIdentityUtils {
    private LensIdentityUtils() {}

    /**
     * Returns the account name for the given profile, or null if the profile is incognito or signed
     * out.
     *
     * @param profile The profile to get the account name for.
     * @return The account name, or null.
     */
    public static @Nullable String getAccountName(@Nullable Profile profile) {
        if (profile == null) {
            return null;
        }

        if (profile.isOffTheRecord()) {
            return null;
        }

        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        if (identityManager == null) {
            return null;
        }

        return CoreAccountInfo.getEmailFrom(identityManager.getPrimaryAccountInfo());
    }
}
