// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Provides access to {@link EntityDataManager}s for a given {@link Profile}. */
@NullMarked
public class EntityDataManagerFactory {
    private static final ProfileKeyedMap<EntityDataManager> sProfileMap =
            ProfileKeyedMap.createMapOfDestroyables(
                    ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
    private static @Nullable EntityDataManager sManagerForTesting;

    /** Return the {@link EntityDataManager} associated with the passed in {@link Profile}. */
    public static EntityDataManager getForProfile(Profile profile) {
        if (sManagerForTesting != null) return sManagerForTesting;
        ThreadUtils.assertOnUiThread();
        return sProfileMap.getForProfile(profile, EntityDataManager::new);
    }

    public static void setInstanceForTesting(EntityDataManager manager) {
        var oldValue = sManagerForTesting;
        sManagerForTesting = manager;
        ResettersForTesting.register(() -> sManagerForTesting = oldValue);
    }
}
