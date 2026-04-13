// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.TabStateStore.TabStateStoreCleaner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.chrome.browser.tabmodel.TabPersistentStoreImpl.TabPersistentStoreImplCleaner;

/** Factory for creating {@link PersistentStoreCleaner} instances. */
@NullMarked
public class PersistentStoreCleanerFactory {
    private PersistentStoreCleanerFactory() {}

    private static final ProfileKeyedMap<PersistentStoreCleaner> sProfileMap =
            new ProfileKeyedMap<>(ProfileKeyedMap.noRequiredCleanupAction());
    private static @Nullable PersistentStoreCleaner sCleanerForTesting;

    /**
     * Return a {@link PersistentStoreCleaner} instance for the provided profile. Redirects to the
     * original profile if an off the record profile is passed in.
     *
     * @param profile The profile used to initialize the cleaner.
     */
    public static PersistentStoreCleaner getForProfile(Profile profile) {
        if (sCleanerForTesting != null) {
            return sCleanerForTesting;
        }
        return sProfileMap.getForProfile(
                profile.getOriginalProfile(),
                localProfile ->
                        new PersistentStoreCleaner(
                                localProfile,
                                new TabStateStoreCleaner(),
                                new TabPersistentStoreImplCleaner()));
    }

    /**
     * @param cleaner The cleaner to override with. Pass null to remove override.
     */
    public static void setForTesting(PersistentStoreCleaner cleaner) {
        sCleanerForTesting = cleaner;
        ResettersForTesting.register(() -> sCleanerForTesting = null);
    }
}
