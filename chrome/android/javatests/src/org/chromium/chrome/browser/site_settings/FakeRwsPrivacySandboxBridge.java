// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.site_settings;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.privacy_sandbox.PrivacySandboxBridge;
import org.chromium.chrome.browser.privacy_sandbox.Topic;

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
    public boolean isPrivacySandboxRestricted() {
        return false;
    }

    @Override
    public boolean isRestrictedNoticeEnabled() {
        return false;
    }

    @Override
    public boolean isFirstPartySetsDataAccessEnabled() {
        return true;
    }

    @Override
    public boolean isFirstPartySetsDataAccessManaged() {
        return true;
    }

    @Override
    public boolean isPartOfManagedFirstPartySet(String origin) {
        return mRwsMembers.contains(origin);
    }

    @Override
    public void setFirstPartySetsDataAccessEnabled(boolean enabled) {}

    @Override
    public String getFirstPartySetOwner(String memberOrigin) {
        return mRwsMembers.contains(memberOrigin.replace("http://", "")) ? mRwsOwner : "";
    }

    @Override
    public Topic[] getCurrentTopTopics() {
        return null;
    }

    @Override
    public Topic[] getBlockedTopics() {
        return null;
    }

    @Override
    public Topic[] getFirstLevelTopics() {
        return null;
    }

    @Override
    public Topic[] getChildTopicsCurrentlyAssigned(int topicId, int taxonomyVersion) {
        return null;
    }

    @Override
    public void setTopicAllowed(int topicId, int taxonomyVersion, boolean allowed) {}

    @Override
    public void getFledgeJoiningEtldPlusOneForDisplay(Callback<String[]> callback) {}

    @Override
    public String[] getBlockedFledgeJoiningTopFramesForDisplay() {
        return null;
    }

    @Override
    public void setFledgeJoiningAllowed(String topFrameEtldPlus1, boolean allowed) {}

    @Override
    public int getRequiredPromptType() {
        return 0;
    }

    @Override
    public void promptActionOccurred(int action) {}

    @Override
    public void topicsToggleChanged(boolean newValue) {}

    @Override
    public void setAllPrivacySandboxAllowedForTesting() {}
}
