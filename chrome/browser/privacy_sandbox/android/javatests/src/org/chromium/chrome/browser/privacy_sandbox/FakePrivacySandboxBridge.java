// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.base.Callback;

import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.Set;

/**
 * Java implementation of PrivacySandboxBridge for testing.
 */
public class FakePrivacySandboxBridge implements PrivacySandboxBridge.Natives {
    private boolean mIsPrivacySandboxEnabled = true;
    private boolean mIsPrivacySandboxRestricted /* = false*/;
    private boolean mIsRestrictedNoticeEnabled /* = false*/;

    private final HashMap<String, Topic> mTopics = new HashMap<>();
    private final Set<Topic> mCurrentTopTopics = new LinkedHashSet<>();
    private final Set<Topic> mBlockedTopics = new LinkedHashSet<>();
    private final Set<String> mCurrentFledgeSites = new LinkedHashSet<>();
    private final Set<String> mBlockedFledgeSites = new LinkedHashSet<>();
    private @PromptType int mPromptType = PromptType.NONE;
    private Integer mLastPromptAction;
    private boolean mLastTopicsToggleValue;

    public void setCurrentTopTopics(String... topics) {
        mCurrentTopTopics.clear();
        for (String name : topics) {
            mCurrentTopTopics.add(getOrCreateTopic(name));
        }
    }

    public void setBlockedTopics(String... topics) {
        mBlockedTopics.clear();
        for (String name : topics) {
            mBlockedTopics.add(getOrCreateTopic(name));
        }
    }

    public void setCurrentFledgeSites(String... sites) {
        mCurrentFledgeSites.clear();
        mCurrentFledgeSites.addAll(Arrays.asList(sites));
    }

    public void setBlockedFledgeSites(String... sites) {
        mBlockedFledgeSites.clear();
        mBlockedFledgeSites.addAll(Arrays.asList(sites));
    }

    private Topic getOrCreateTopic(String name) {
        Topic t = mTopics.get(name);
        if (t == null) {
            t = new Topic(mTopics.size(), -1, name);
            mTopics.put(name, t);
        }
        return t;
    }

    @Override
    public boolean isPrivacySandboxEnabled() {
        return mIsPrivacySandboxEnabled;
    }

    @Override
    public boolean isPrivacySandboxManaged() {
        return false;
    }

    @Override
    public boolean isPrivacySandboxRestricted() {
        return mIsPrivacySandboxRestricted;
    }

    @Override
    public boolean isRestrictedNoticeEnabled() {
        return mIsRestrictedNoticeEnabled;
    }

    @Override
    public boolean isFirstPartySetsDataAccessEnabled() {
        return false;
    }

    @Override
    public boolean isFirstPartySetsDataAccessManaged() {
        return false;
    }

    @Override
    public boolean isPartOfManagedFirstPartySet(String origin) {
        return false;
    }

    @Override
    public void setPrivacySandboxEnabled(boolean enabled) {
        mIsPrivacySandboxEnabled = enabled;
    }

    @Override
    public void setFirstPartySetsDataAccessEnabled(boolean enabled) {}

    @Override
    public String getFirstPartySetOwner(String memberOrigin) {
        return null;
    }

    public void setPrivacySandboxRestricted(boolean restricted) {
        mIsPrivacySandboxRestricted = restricted;
    }

    public void setRestrictedNoticeEnabled(boolean restrictedNoticeEnabled) {
        mIsRestrictedNoticeEnabled = restrictedNoticeEnabled;
    }

    @Override
    public Topic[] getCurrentTopTopics() {
        return mCurrentTopTopics.toArray(new Topic[] {});
    }

    @Override
    public Topic[] getBlockedTopics() {
        return mBlockedTopics.toArray(new Topic[] {});
    }

    @Override
    public void setTopicAllowed(int topicId, int taxonomyVersion, boolean allowed) {
        Topic topic = null;
        for (Topic t : mTopics.values()) {
            if (t.getTopicId() == topicId) {
                topic = t;
            }
        }
        if (allowed) {
            mCurrentTopTopics.add(topic);
            mBlockedTopics.remove(topic);
        } else {
            mCurrentTopTopics.remove(topic);
            mBlockedTopics.add(topic);
        }
    }

    @Override
    public void getFledgeJoiningEtldPlusOneForDisplay(Callback<String[]> callback) {
        callback.onResult(mCurrentFledgeSites.toArray(new String[0]));
    }

    @Override
    public String[] getBlockedFledgeJoiningTopFramesForDisplay() {
        return mBlockedFledgeSites.toArray(new String[0]);
    }

    @Override
    public void setFledgeJoiningAllowed(String topFrameEtldPlus1, boolean allowed) {
        if (allowed) {
            mCurrentFledgeSites.add(topFrameEtldPlus1);
            mBlockedFledgeSites.remove(topFrameEtldPlus1);
        } else {
            mCurrentFledgeSites.remove(topFrameEtldPlus1);
            mBlockedFledgeSites.add(topFrameEtldPlus1);
        }
    }

    public void setRequiredPromptType(@PromptType int type) {
        mPromptType = type;
    }

    @Override
    public int getRequiredPromptType() {
        return mPromptType;
    }

    @Override
    public void promptActionOccurred(@PromptAction int action) {
        mLastPromptAction = action;
    }

    public Integer getLastPromptAction() {
        return mLastPromptAction;
    }

    public void resetLastPromptAction() {
        mLastPromptAction = null;
    }

    @Override
    public void topicsToggleChanged(boolean newValue) {
        mLastTopicsToggleValue = newValue;
    }

    public boolean getLastTopicsToggleValue() {
        return mLastTopicsToggleValue;
    }
}
