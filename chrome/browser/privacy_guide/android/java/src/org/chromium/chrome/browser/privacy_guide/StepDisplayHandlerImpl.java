// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.sync.SyncService;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.user_prefs.UserPrefs;

/**
 * Computes for each privacy guide step whether it should be displayed or not.
 */
class StepDisplayHandlerImpl implements StepDisplayHandler {
    @Override
    public boolean shouldDisplaySync() {
        SyncService syncService = SyncService.get();
        return syncService != null && syncService.isSyncFeatureEnabled();
    }

    @Override
    public boolean shouldDisplaySafeBrowsing() {
        return SafeBrowsingBridge.getSafeBrowsingState() != SafeBrowsingState.NO_SAFE_BROWSING;
    }

    @Override
    public boolean shouldDisplayCookies() {
        boolean allowCookies = WebsitePreferenceBridge.isCategoryEnabled(
                Profile.getLastUsedRegularProfile(), ContentSettingsType.COOKIES);
        @CookieControlsMode
        int cookieControlsMode = UserPrefs.get(Profile.getLastUsedRegularProfile())
                                         .getInteger(PrefNames.COOKIE_CONTROLS_MODE);
        return allowCookies && cookieControlsMode != CookieControlsMode.OFF;
    }
}
