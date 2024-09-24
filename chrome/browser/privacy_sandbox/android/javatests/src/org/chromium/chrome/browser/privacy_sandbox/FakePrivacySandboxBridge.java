// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashMap;
import java.util.LinkedHashSet;
import java.util.List;
import java.util.Set;

/** Java implementation of PrivacySandboxBridge for testing. */
public class FakePrivacySandboxBridge implements PrivacySandboxBridge.Natives {
    private boolean mIsPrivacySandboxRestricted /* = false*/;
    private boolean mIsRestrictedNoticeEnabled /* = false*/;

    private final HashMap<String, Topic> mTopics = new HashMap<>();
    private final Set<Topic> mCurrentTopTopics = new LinkedHashSet<>();
    private final Set<Topic> mBlockedTopics = new LinkedHashSet<>();
    private final Set<Topic> mFirstLevelTopics = new LinkedHashSet<>();
    private final Set<Topic> mChildTopics = new LinkedHashSet<>();
    private final Set<String> mCurrentFledgeSites = new LinkedHashSet<>();
    private final Set<String> mBlockedFledgeSites = new LinkedHashSet<>();
    private @PromptType int mPromptType = PromptType.NONE;
    private Integer mLastPromptAction;
    private Integer mLastSurfaceType;
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

    public void setFirstLevelTopics(String... topics) {
        mFirstLevelTopics.clear();
        for (String name : topics) {
            mFirstLevelTopics.add(getOrCreateTopic(name));
        }
    }

    public void setChildTopics(String... topics) {
        mChildTopics.clear();
        for (String name : topics) {
            mChildTopics.add(getOrCreateTopic(name));
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
    public boolean isPrivacySandboxRestricted(Profile profile) {
        return mIsPrivacySandboxRestricted;
    }

    @Override
    public boolean isRestrictedNoticeEnabled(Profile profile) {
        return mIsRestrictedNoticeEnabled;
    }

    @Override
    public boolean isFirstPartySetsDataAccessEnabled(Profile profile) {
        return false;
    }

    @Override
    public boolean isFirstPartySetsDataAccessManaged(Profile profile) {
        return false;
    }

    @Override
    public boolean isPartOfManagedFirstPartySet(Profile profile, String origin) {
        return false;
    }

    @Override
    public void setFirstPartySetsDataAccessEnabled(Profile profile, boolean enabled) {}

    @Override
    public String getFirstPartySetOwner(Profile profile, String memberOrigin) {
        return null;
    }

    public void setPrivacySandboxRestricted(boolean restricted) {
        mIsPrivacySandboxRestricted = restricted;
    }

    public void setRestrictedNoticeEnabled(boolean restrictedNoticeEnabled) {
        mIsRestrictedNoticeEnabled = restrictedNoticeEnabled;
    }

    @Override
    public List<Topic> getCurrentTopTopics(Profile profile) {
        return new ArrayList<>(mCurrentTopTopics);
    }

    @Override
    public List<Topic> getBlockedTopics(Profile profile) {
        return new ArrayList<>(mBlockedTopics);
    }

    @Override
    public List<Topic> getFirstLevelTopics(Profile profile) {
        return new ArrayList<>(mFirstLevelTopics);
    }

    @Override
    public List<Topic> getChildTopicsCurrentlyAssigned(
            Profile profile, int topicId, int taxonomyVersion) {
        return new ArrayList<>(mChildTopics);
    }

    @Override
    public void setTopicAllowed(
            Profile profile, int topicId, int taxonomyVersion, boolean allowed) {
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

    public void setRequiredPromptType(@PromptType int type) {
        mPromptType = type;
    }

    public int getRequiredPromptType(@SurfaceType int surfaceType) {
        return mPromptType;
    }

    @Override
    public int getRequiredPromptType(Profile profile, @SurfaceType int surfaceType) {
        return getRequiredPromptType(surfaceType);
    }

    @Override
    public void promptActionOccurred(
            Profile profile, @PromptAction int action, @SurfaceType int surfaceType) {
        mLastPromptAction = action;
        mLastSurfaceType = surfaceType;
    }

    public Integer getLastPromptAction() {
        return mLastPromptAction;
    }

    public Integer getLastSurfaceType() {
        return mLastSurfaceType;
    }

    public void resetLastPromptAction() {
        mLastPromptAction = null;
    }

    @Override
    public void topicsToggleChanged(Profile profile, boolean newValue) {
        mLastTopicsToggleValue = newValue;
    }

    public boolean getLastTopicsToggleValue() {
        return mLastTopicsToggleValue;
    }

    @Override
    public void setAllPrivacySandboxAllowedForTesting(Profile profile) {}

    @Override
    public void recordActivityType(Profile profile, int activityType) {}

    @Override
    public boolean privacySandboxPrivacyGuideShouldShowAdTopicsCard(Profile profile) {
        return false;
    }
}
