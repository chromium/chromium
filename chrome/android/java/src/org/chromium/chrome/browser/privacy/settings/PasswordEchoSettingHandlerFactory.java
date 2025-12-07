// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy.settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Factory for creating {@link PasswordEchoSettingHandler}. */
@NullMarked
public class PasswordEchoSettingHandlerFactory {
    private static @Nullable ProfileKeyedMap<PasswordEchoSettingHandler> sProfileMap;

    /**
     * Return the {@link PasswordEchoSettingHandler} associated with the passed in {@link Profile}.
     */
    public static PasswordEchoSettingHandler getForProfile(Profile profile) {
        if (sProfileMap == null) {
            sProfileMap =
                    ProfileKeyedMap.createMapOfDestroyables(
                            ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);
        }
        return sProfileMap.getForProfile(profile, PasswordEchoSettingHandler::new);
    }
}
