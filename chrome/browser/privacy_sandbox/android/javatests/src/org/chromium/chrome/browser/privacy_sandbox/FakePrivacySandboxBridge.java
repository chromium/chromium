// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Java implementation of PrivacySandboxBridge for testing. */
public class FakePrivacySandboxBridge implements PrivacySandboxBridge.Natives {
    private boolean mIsPrivacySandboxRestricted /* = false*/;
    private boolean mIsRestrictedNoticeEnabled /* = false*/;
    private boolean mIsRwsManaged /* = false*/;

    private final Set<String> mCurrentFledgeSites = new LinkedHashSet<>();
    private final Set<String> mBlockedFledgeSites = new LinkedHashSet<>();
    private static final String GOOGLE_EMBEDDED_PRIVACY_POLICY_U_R_L =
            "https://policies.google.com/privacy/embedded";

    public void setCurrentFledgeSites(String... sites) {
        mCurrentFledgeSites.clear();
        mCurrentFledgeSites.addAll(Arrays.asList(sites));
    }

    public void setBlockedFledgeSites(String... sites) {
        mBlockedFledgeSites.clear();
        mBlockedFledgeSites.addAll(Arrays.asList(sites));
    }

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
    public void getFledgeJoiningEtldPlusOneForDisplay(
            Profile profile, Callback<String[]> callback) {
        callback.onResult(mCurrentFledgeSites.toArray(new String[0]));
    }

    @Override
    public List<String> getBlockedFledgeJoiningTopFramesForDisplay(Profile profile) {
        return new ArrayList<>(mBlockedFledgeSites);
    }

    @Override
    public void setFledgeJoiningAllowed(
            Profile profile, String topFrameEtldPlus1, boolean allowed) {
        setFledgeJoiningAllowed(topFrameEtldPlus1, allowed);
    }

    public void setFledgeJoiningAllowed(String topFrameEtldPlus1, boolean allowed) {
        if (allowed) {
            mCurrentFledgeSites.add(topFrameEtldPlus1);
            mBlockedFledgeSites.remove(topFrameEtldPlus1);
        } else {
            mCurrentFledgeSites.remove(topFrameEtldPlus1);
            mBlockedFledgeSites.add(topFrameEtldPlus1);
        }
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
