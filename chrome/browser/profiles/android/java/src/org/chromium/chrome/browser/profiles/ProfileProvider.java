// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Provider of the appropriate Profiles for the given application context. */
@NullMarked
public interface ProfileProvider {
    /** Return the original profile. */
    Profile getOriginalProfile();

    /**
     * Return the OffTheRecord profile associated with {@link #getOriginalProfile()}.
     *
     * @param createIfNeeded Pass true if the OffTheRecord profile should be created if it does not
     *     already exist. If false is passed and the profile has not yet been created, this will
     *     return null.
     */
    @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded);

    /** Return whether the OffTheRecord has been created. */
    boolean hasOffTheRecordProfile();

    /**
     * Utility for getting (and creating if necessary) the appropriate {@link Profile} from the
     * given {@link ProfileProvider} based on the desired incognito state.
     */
    static Profile getOrCreateProfile(ProfileProvider profileProvider, boolean incognito) {
        assert profileProvider != null;
        Profile profile =
                incognito
                        ? profileProvider.getOffTheRecordProfile(true)
                        : profileProvider.getOriginalProfile();
        if (incognito != assumeNonNull(profile).isOffTheRecord()) {
            throw new IllegalStateException("Incognito mismatch");
        }
        return profile;
    }
}
