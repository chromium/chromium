// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

/** Provider of the appropriate Profiles for the given application context. */
public interface ProfileProvider {
    /** Return the original profile. */
    @NonNull
    Profile getOriginalProfile();

    /**
     * Return the OffTheRecord profile associated with {@link #getOriginalProfile()}.
     *
     * @param createIfNeeded Pass true if the OffTheRecord profile should be created if it does not
     *     already exist. If false is passed and the profile has not yet been created, this will
     *     return null.
     */
    @Nullable
    Profile getOffTheRecordProfile(boolean createIfNeeded);

    /** Return whether the OffTheRecord has been created. */
    boolean hasOffTheRecordProfile();
}
