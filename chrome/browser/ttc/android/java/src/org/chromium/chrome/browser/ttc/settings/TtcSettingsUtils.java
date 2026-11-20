// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ttc.settings;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ttc.TtcSessionCoordinator;

/** Decides whether the TTC settings page is offered. */
@NullMarked
public final class TtcSettingsUtils {
    private TtcSettingsUtils() {}

    /**
     * Returns whether the TTC settings page is offered for {@code profile}: whenever TTC can be
     * used. TTC settings aren't specific to Incognito, so the page follows the original profile.
     */
    public static boolean shouldShowSettingsPage(@Nullable Profile profile) {
        return profile != null
                && TtcSessionCoordinator.getServiceIfAvailable(profile.getOriginalProfile())
                        != null;
    }
}
