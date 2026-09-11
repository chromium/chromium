// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** An in-memory implementation of {@link ProfileProvider} for testing. */
@NullMarked
public class TestProfileProvider implements ProfileProvider {
    private final Profile mOriginalProfile;
    private final @Nullable Profile mOffTheRecordProfile;

    /**
     * Constructs a {@link TestProfileProvider} with explicit original and optional OTR profiles.
     *
     * @param originalProfile The original regular profile.
     * @param offTheRecordProfile The OTR profile, or null.
     */
    public TestProfileProvider(Profile originalProfile, @Nullable Profile offTheRecordProfile) {
        TestProfile.assertBrowserProcessNotStarted();
        mOriginalProfile = originalProfile;
        mOffTheRecordProfile = offTheRecordProfile;
    }

    @Override
    public Profile getOriginalProfile() {
        return mOriginalProfile;
    }

    @Override
    public @Nullable Profile getOffTheRecordProfile(boolean createIfNeeded) {
        if (mOffTheRecordProfile != null) {
            return mOffTheRecordProfile;
        }
        return mOriginalProfile.getOffTheRecordProfile(
                OtrProfileId.getPrimaryOtrProfileId(), createIfNeeded);
    }
}
