// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

import java.util.Set;

/**
 * A utility class for Privacy Guide that fetches the current state of {@link
 * PrivacyGuideFragment.FragmentType}s.
 *
 * TODO(crbug.com/1393960): Utilize the methods on this class on the PG fragments
 */
class PrivacyGuideUtils {
    public static boolean isMsbbEnabled() {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(
                Profile.getLastUsedRegularProfile());
    }
    public static boolean isHistorySyncEnabled() {
        Set<Integer> syncTypes = SyncService.get().getSelectedTypes();
        return syncTypes.contains(UserSelectableType.HISTORY);
    }
}
