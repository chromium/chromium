// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill;

import org.chromium.base.ResettersForTesting;
import org.chromium.base.ThreadUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Provides access to {@link PersonalDataManager}s for a given {@link Profile}. */
public class PersonalDataManagerFactory {
    private static ProfileKeyedMap<PersonalDataManager> sProfileMap =
            ProfileKeyedMap.createMapOfDestroyables(
                    ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
    private static PersonalDataManager sManagerForTesting;

    /** Return the {@link PersonalDataManager} associated with the passed in {@link Profile}. */
    public static PersonalDataManager getForProfile(Profile profile) {
        if (sManagerForTesting != null) return sManagerForTesting;
        ThreadUtils.assertOnUiThread();
        return sProfileMap.getForProfile(profile, PersonalDataManager::new);
    }

    public static void setInstanceForTesting(PersonalDataManager manager) {
        var oldValue = sManagerForTesting;
        sManagerForTesting = manager;
        ResettersForTesting.register(() -> sManagerForTesting = oldValue);
    }
}
