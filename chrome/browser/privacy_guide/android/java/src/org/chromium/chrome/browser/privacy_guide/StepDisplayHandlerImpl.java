// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.chromium.chrome.browser.privacy_guide.PrivacyGuideUtils.canUpdateHistorySyncValue;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;

/** Computes for each privacy guide step whether it should be displayed or not. */
@NullMarked
class StepDisplayHandlerImpl implements StepDisplayHandler {
    private final Profile mProfile;
    private final PrivacySandboxBridge mPrivacySandboxBridge;

    StepDisplayHandlerImpl(Profile profile) {
        mProfile = profile;
        mPrivacySandboxBridge = new PrivacySandboxBridge(mProfile);
    }

    @Override
    public boolean shouldDisplayHistorySync() {
        return canUpdateHistorySyncValue(mProfile);
    }

    @Override
    public boolean shouldDisplaySafeBrowsing() {
        return PrivacyGuideUtils.getSafeBrowsingState(mProfile)
                != SafeBrowsingState.NO_SAFE_BROWSING;
    }

    @Override
    public boolean shouldDisplayCookies() {
        boolean allowCookies =
                WebsitePreferenceBridge.isCategoryEnabled(mProfile, ContentSettingsType.COOKIES);
        @CookieControlsMode
        int cookieControlsMode = PrivacyGuideUtils.getCookieControlsMode(mProfile);
        return !PrivacyGuideUtils.trackingProtectionUiEnabled(mProfile)
                && allowCookies
                && (cookieControlsMode != CookieControlsMode.OFF
                        || ChromeFeatureList.isEnabled(
                                ChromeFeatureList.ALWAYS_BLOCK_3PCS_INCOGNITO));
    }

    @Override
    public boolean shouldDisplayAdTopics() {
        return mPrivacySandboxBridge.privacySandboxPrivacyGuideShouldShowAdTopicsCard();
    }
}
