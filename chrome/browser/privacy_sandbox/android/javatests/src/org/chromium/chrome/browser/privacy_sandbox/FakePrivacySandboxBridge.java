// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.chrome.browser.profiles.Profile;

/** Java implementation of PrivacySandboxBridge for testing. */
public class FakePrivacySandboxBridge implements PrivacySandboxBridge.Natives {
    private boolean mIsPrivacySandboxRestricted /* = false*/;
    private boolean mIsRestrictedNoticeEnabled /* = false*/;
    private boolean mIsRwsManaged /* = false*/;

    private static final String GOOGLE_EMBEDDED_PRIVACY_POLICY_U_R_L =
            "https://policies.google.com/privacy/embedded";

    @Override
    public boolean isPrivacySandboxRestricted(Profile profile) {
        return mIsPrivacySandboxRestricted;
    }

    @Override
    public boolean isRestrictedNoticeEnabled(Profile profile) {
        return mIsRestrictedNoticeEnabled;
    }

    @Override
    public boolean isRelatedWebsiteSetsDataAccessEnabled(Profile profile) {
        return true;
    }

    @Override
    public boolean isRelatedWebsiteSetsDataAccessManaged(Profile profile) {
        return false;
    }

    @Override
    public boolean isPartOfManagedRelatedWebsiteSet(Profile profile, String origin) {
        return mIsRwsManaged;
    }

    @Override
    public void setRelatedWebsiteSetsDataAccessEnabled(Profile profile, boolean enabled) {}

    @Override
    public String getRelatedWebsiteSetOwner(Profile profile, String memberOrigin) {
        return null;
    }

    public void setIsRwsManaged(boolean managed) {
        mIsRwsManaged = managed;
    }

    public void setPrivacySandboxRestricted(boolean restricted) {
        mIsPrivacySandboxRestricted = restricted;
    }

    public void setRestrictedNoticeEnabled(boolean restrictedNoticeEnabled) {
        mIsRestrictedNoticeEnabled = restrictedNoticeEnabled;
    }

    @Override
    public void setAllPrivacySandboxAllowedForTesting(Profile profile) {}

    @Override
    public boolean shouldUsePrivacyPolicyChinaDomain(Profile profile) {
        return false;
    }

    @Override
    public String getEmbeddedPrivacyPolicyURL(
            @PrivacyPolicyDomainType int domainType,
            @PrivacyPolicyColorScheme int colorScheme,
            String locale) {
        return GOOGLE_EMBEDDED_PRIVACY_POLICY_U_R_L;
    }
}
