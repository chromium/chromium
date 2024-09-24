// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.Topic;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.List;
import java.util.Set;

/**
 * Java implementation of PrivacySandboxBridge for RWS testing.
 *
 * <p>It enables RWS (Related Website Sets - formerly called first party sets) and creates only one
 * RWS group with an owner provided by the client. Every website provided in the rwsMembers set is
 * treated as a RWS member.
 */
public class FakeRwsPrivacySandboxBridge implements PrivacySandboxBridge.Natives {
    // Owner of the one RWS group represented by the fake bridge
    private final String mRwsOwner;
    private final Set<String> mRwsMembers;

    public FakeRwsPrivacySandboxBridge(String rwsOwner, Set<String> rwsMembers) {
        this.mRwsOwner = rwsOwner;
        this.mRwsMembers = rwsMembers;
    }

    @Override
    public boolean isPrivacySandboxRestricted(Profile profile) {
        return false;
    }

    @Override
    public boolean isRestrictedNoticeEnabled(Profile profile) {
        return false;
    }

    @Override
    public boolean isFirstPartySetsDataAccessEnabled(Profile profile) {
        return true;
    }

    @Override
    public boolean isFirstPartySetsDataAccessManaged(Profile profile) {
        return true;
    }

    @Override
    public boolean isPartOfManagedFirstPartySet(Profile profile, String origin) {
        return mRwsMembers.contains(origin);
    }

    @Override
    public void setFirstPartySetsDataAccessEnabled(Profile profile, boolean enabled) {}

    @Override
    public String getFirstPartySetOwner(Profile profile, String memberOrigin) {
        return mRwsMembers.contains(memberOrigin.replace("http://", "")) ? mRwsOwner : "";
    }

    @Override
    public List<Topic> getCurrentTopTopics(Profile profile) {
        return null;
    }

    @Override
    public List<Topic> getBlockedTopics(Profile profile) {
        return null;
    }

    @Override
    public List<Topic> getFirstLevelTopics(Profile profile) {
        return null;
    }

    @Override
    public List<Topic> getChildTopicsCurrentlyAssigned(
            Profile profile, int topicId, int taxonomyVersion) {
        return null;
    }

    @Override
    public void setTopicAllowed(
            Profile profile, int topicId, int taxonomyVersion, boolean allowed) {}

    @Override
    public void getFledgeJoiningEtldPlusOneForDisplay(
            Profile profile, Callback<String[]> callback) {}

    @Override
    public List<String> getBlockedFledgeJoiningTopFramesForDisplay(Profile profile) {
        return null;
    }

    @Override
    public void setFledgeJoiningAllowed(
            Profile profile, String topFrameEtldPlus1, boolean allowed) {}

    @Override
    public int getRequiredPromptType(Profile profile, int surfaceType) {
        return 0;
    }

    @Override
    public void promptActionOccurred(Profile profile, int action, int surfaceType) {}

    @Override
    public void topicsToggleChanged(Profile profile, boolean newValue) {}

    @Override
    public void setAllPrivacySandboxAllowedForTesting(Profile profile) {}

    @Override
    public void recordActivityType(Profile profile, int activityType) {}

    @Override
    public boolean privacySandboxPrivacyGuideShouldShowAdTopicsCard(Profile profile) {
        return false;
    }
}
