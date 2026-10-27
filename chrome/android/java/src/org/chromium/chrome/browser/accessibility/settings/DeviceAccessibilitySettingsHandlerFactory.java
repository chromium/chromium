// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.accessibility.settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;
import org.chromium.components.browser_ui.accessibility.DeviceAccessibilitySettingsHandler;
import org.chromium.components.user_prefs.UserPrefs;

/** Factory for creating {@link DeviceAccessibilitySettingsHandler}. */
@NullMarked
public class DeviceAccessibilitySettingsHandlerFactory {
    private static @Nullable ProfileKeyedMap<DeviceAccessibilitySettingsHandler> sProfileMap;

    private DeviceAccessibilitySettingsHandlerFactory() {}

    /**
     * Return the {@link DeviceAccessibilitySettingsHandler} associated with the passed in {@link
     * Profile}.
     */
    public static DeviceAccessibilitySettingsHandler getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        }
        return sProfileMap.getForProfile(
                profile, p -> new DeviceAccessibilitySettingsHandler(UserPrefs.get(p)));
    }
}
