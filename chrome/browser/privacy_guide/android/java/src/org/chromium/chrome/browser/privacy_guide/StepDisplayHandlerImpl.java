// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesSettingsBridge;
import org.chromium.chrome.browser.prefetch.settings.PreloadPagesState;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.browser_ui.site_settings.WebsitePreferenceBridge;
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;

/** Computes for each privacy guide step whether it should be displayed or not. */
class StepDisplayHandlerImpl implements StepDisplayHandler {
    private final Profile mProfile;
    private PrivacySandboxBridge mPrivacySandboxBridge;

    StepDisplayHandlerImpl(Profile profile) {
        mProfile = profile;
        mPrivacySandboxBridge = new PrivacySandboxBridge(mProfile);
    }

    @Override
    public boolean shouldDisplayHistorySync() {
        SyncService syncService = SyncServiceFactory.getForProfile(mProfile);
        if (syncService == null) {
            return false;
        }
        if (!ChromeFeatureList.isEnabled(
                ChromeFeatureList.REPLACE_SYNC_PROMOS_WITH_SIGN_IN_PROMOS)) {
            return syncService.isSyncFeatureEnabled();
        }

        if (!IdentityServicesProvider.get()
                .getIdentityManager(mProfile)
                .hasPrimaryAccount(ConsentLevel.SIGNIN)) {
            return false;
        }
        if (syncService.isSyncDisabledByEnterprisePolicy()) {
            return false;
        }
        if (syncService.isTypeManagedByPolicy(UserSelectableType.HISTORY)
                && syncService.isTypeManagedByPolicy(UserSelectableType.TABS)) {
            return false;
        }
        if (syncService.isTypeManagedByCustodian(UserSelectableType.HISTORY)
                && syncService.isTypeManagedByCustodian(UserSelectableType.TABS)) {
            return false;
        }
        return true;
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
        return allowCookies && cookieControlsMode != CookieControlsMode.OFF;
    }

    @Override
    public boolean shouldDisplayPreload() {
        return PreloadPagesSettingsBridge.getState(mProfile)
                        == PreloadPagesState.STANDARD_PRELOADING
                || PreloadPagesSettingsBridge.getState(mProfile) == PreloadPagesState.NO_PRELOADING;
    }

    @Override
    public boolean shouldDisplayAdTopics() {
        return mPrivacySandboxBridge.privacySandboxPrivacyGuideShouldShowAdTopicsCard();
    }
}
