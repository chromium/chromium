// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.host_zoom;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileKeyedMap;

/** Factory for creating and managing HostZoomListener instances. */
@NullMarked
public class HostZoomListenerFactory {
    // The REDIRECTED_TO_ORIGINAL selection is used so that incognito profiles
    // share the same HostZoomListener instance with their original profile. This
    // is because zoom levels are not separated for incognito.
    private static final ProfileKeyedMap<HostZoomListener> sProfileMap =
            ProfileKeyedMap.createMapOfDestroyables(
                    ProfileKeyedMap.ProfileSelection.REDIRECTED_TO_ORIGINAL);

    /**
     * @param profile The profile to get the HostZoomListener for.
     * @return The HostZoomListener for the given profile.
     */
    public static HostZoomListener getForProfile(Profile profile) {
        return sProfileMap.getForProfile(profile, HostZoomListener::new);
    }
}
