// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.google_apis.gaia.CoreAccountId;
import org.chromium.google_apis.gaia.GaiaId;

import java.util.ArrayList;
import java.util.List;

/** Fake {@link ProfileAttributesStorage} with one signed-in profile per primary account. */
@NullMarked
public class FakeProfileAttributesStorage extends ProfileAttributesStorage {
    private List<CoreAccountId> mPrimaryAccountIds = List.of();

    /** Must be called on the UI thread. */
    public FakeProfileAttributesStorage() {}

    @Override
    List<ProfileAttributesEntry> getAllProfilesAttributes() {
        List<ProfileAttributesEntry> entries = new ArrayList<>();
        for (int i = 0; i < mPrimaryAccountIds.size(); i++) {
            GaiaId gaiaId = mPrimaryAccountIds.get(i).getId();
            entries.add(
                    new ProfileAttributesEntry(getProfilePath(i)) {
                        @Override
                        @Nullable GaiaId getGaiaId() {
                            return gaiaId;
                        }

                        @Override
                        boolean isOmitted() {
                            return false;
                        }
                    });
        }
        return entries;
    }

    /** Replaces the profiles with one signed-in profile per account and notifies the observers. */
    public void setPrimaryAccountIds(List<CoreAccountId> accountIds) {
        int profileCount = Math.max(mPrimaryAccountIds.size(), accountIds.size());
        mPrimaryAccountIds = List.copyOf(accountIds);
        for (int i = 0; i < profileCount; i++) {
            onProfileAuthInfoChanged(getProfilePath(i));
        }
    }

    /** Mirrors the profile directory names: "Default", then "Profile 1", "Profile 2"... */
    private static String getProfilePath(int index) {
        return index == 0 ? "Default" : "Profile " + index;
    }
}
