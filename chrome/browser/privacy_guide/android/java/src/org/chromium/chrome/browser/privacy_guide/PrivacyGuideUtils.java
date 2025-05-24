// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_guide;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.privacy_guide.PrivacyGuideFragment.FragmentType;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.UnifiedConsentServiceBridge;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.content_settings.CookieControlsMode;
import org.chromium.components.content_settings.PrefNames;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.UserSelectableType;
import org.chromium.components.user_prefs.UserPrefs;

import java.util.Set;

/**
 * A utility class for Privacy Guide that fetches the current state of {@link
 * PrivacyGuideFragment.FragmentType}s.
 */
@NullMarked
class PrivacyGuideUtils {
    static boolean isMsbbEnabled(Profile profile) {
        return UnifiedConsentServiceBridge.isUrlKeyedAnonymizedDataCollectionEnabled(profile);
    }

    static boolean isHistorySyncEnabled(Profile profile) {
        Set<Integer> syncTypes =
                assumeNonNull(SyncServiceFactory.getForProfile(profile)).getSelectedTypes();

        // The toggle represents both History and Tabs.
        // History and Tabs should usually have the same value, but in some
        // cases they may not, e.g. if one of them is disabled by policy. In that
        // case, show the toggle as on if at least one of them is enabled. The
        // toggle should reflect the value of the non-disabled type.
        return syncTypes.contains(UserSelectableType.HISTORY)
                || syncTypes.contains(UserSelectableType.TABS);
    }

    static boolean isUserSignedIn(Profile profile) {
        IdentityManager identityManager =
                IdentityServicesProvider.get().getIdentityManager(profile);
        assumeNonNull(identityManager);
        return identityManager.hasPrimaryAccount(ConsentLevel.SIGNIN);
    }

    static boolean isAdTopicsEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.PRIVACY_SANDBOX_M1_TOPICS_ENABLED);
    }

    static @SafeBrowsingState int getSafeBrowsingState(Profile profile) {
        return new SafeBrowsingBridge(profile).getSafeBrowsingState();
    }

    static @CookieControlsMode int getCookieControlsMode(Profile profile) {
        return UserPrefs.get(profile).getInteger(PrefNames.COOKIE_CONTROLS_MODE);
    }

    static boolean trackingProtectionUiEnabled(Profile profile) {
        return UserPrefs.get(profile).getBoolean(Pref.TRACKING_PROTECTION3PCD_ENABLED)
                || ChromeFeatureList.isEnabled(ChromeFeatureList.TRACKING_PROTECTION_3PCD);
    }

    static boolean canUpdateHistorySyncValue(Profile profile) {
        SyncService syncService = SyncServiceFactory.getForProfile(profile);
        if (syncService == null) {
            return false;
        }

        if (!isUserSignedIn(profile)) {
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

    static int getFragmentFocusViewId(@FragmentType int fragmentType) {
        switch (fragmentType) {
            case FragmentType.WELCOME:
                return R.id.welcome_view;
            case FragmentType.MSBB:
                return R.id.msbb_switch;
            case FragmentType.HISTORY_SYNC:
                return R.id.history_sync_switch;
            case FragmentType.SAFE_BROWSING:
                return R.id.sb_step_header;
            case FragmentType.COOKIES:
                return R.id.cookies_step_header;
            case FragmentType.AD_TOPICS:
                return R.id.ad_topics_switch;
            case FragmentType.DONE:
                return R.id.done_step_header;
            default:
                assert false : "Unexpected fragment type: " + fragmentType;
                return -1;
        }
    }
}
