// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Factory for creating and managing OtherDevicesShortcutController instances. */
@NullMarked
public class OtherDevicesShortcutControllerFactory {
    private static final ProfileKeyedMap<OtherDevicesShortcutController> sProfileMap =
            ProfileKeyedMap.createMapOfDestroyables(
                    ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);

    /**
     * @param profile The profile to get the OtherDevicesShortcutController for.
     * @return The OtherDevicesShortcutController for the given profile.
     */
    public static OtherDevicesShortcutController getForProfile(Profile profile) {
        return sProfileMap.getForProfile(profile, OtherDevicesShortcutController::new);
    }
}
