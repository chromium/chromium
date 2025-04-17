// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Bridge, providing access to the native-side Privacy Sandbox configuration. */
@NullMarked
public class PrivacySandboxBridge {

    private final Profile mProfile;

    public PrivacySandboxBridge(Profile profile) {
        mProfile = profile;
    }

    public boolean isPrivacySandboxRestricted() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxRestricted(mProfile);
    }

    public boolean isRestrictedNoticeEnabled() {
        return PrivacySandboxBridgeJni.get().isRestrictedNoticeEnabled(mProfile);
    }

    public List<Topic> getCurrentTopTopics() {
        return sortTopics(PrivacySandboxBridgeJni.get().getCurrentTopTopics(mProfile));
    }

    public List<Topic> getBlockedTopics() {
        return sortTopics(PrivacySandboxBridgeJni.get().getBlockedTopics(mProfile));
    }

    public List<Topic> getFirstLevelTopics() {
        return sortTopics(PrivacySandboxBridgeJni.get().getFirstLevelTopics(mProfile));
    }

    public List<Topic> getChildTopicsCurrentlyAssigned(Topic topic) {
        return sortTopics(
                PrivacySandboxBridgeJni.get()
                        .getChildTopicsCurrentlyAssigned(
                                mProfile, topic.getTopicId(), topic.getTaxonomyVersion()));
    }

    public void setTopicAllowed(Topic topic, boolean allowed) {
        PrivacySandboxBridgeJni.get()
                .setTopicAllowed(mProfile, topic.getTopicId(), topic.getTaxonomyVersion(), allowed);
    }

    @CalledByNative
    private static Topic createTopic(
            int topicId, int taxonomyVersion, String name, String description) {
        return new Topic(topicId, taxonomyVersion, name, description);
    }

    private static List<Topic> sortTopics(List<Topic> topics) {
        Collections.sort(
                topics,
                (o1, o2) -> {
                    return ((Topic) o1).getName().compareTo(((Topic) o2).getName());
                });
        return topics;
    }

    public void getFledgeJoiningEtldPlusOneForDisplay(Callback<List<String>> callback) {
        Callback<String[]> arrayCallback =
                (String[] domains) -> callback.onResult(Arrays.asList(domains));
        PrivacySandboxBridgeJni.get()
                .getFledgeJoiningEtldPlusOneForDisplay(mProfile, arrayCallback);
    }

    public List<String> getBlockedFledgeJoiningTopFramesForDisplay() {
        return PrivacySandboxBridgeJni.get().getBlockedFledgeJoiningTopFramesForDisplay(mProfile);
    }

    public void setFledgeJoiningAllowed(String topFrameEtldPlus1, boolean allowed) {
        PrivacySandboxBridgeJni.get().setFledgeJoiningAllowed(mProfile, topFrameEtldPlus1, allowed);
    }

    public @PromptType int getRequiredPromptType(@SurfaceType int surfaceType) {
        return PrivacySandboxBridgeJni.get().getRequiredPromptType(mProfile, surfaceType);
    }

    public void promptActionOccurred(@PromptAction int action, @SurfaceType int surfaceType) {
        PrivacySandboxBridgeJni.get().promptActionOccurred(mProfile, action, surfaceType);
    }

    public boolean isRelatedWebsiteSetsDataAccessEnabled() {
        return PrivacySandboxBridgeJni.get().isRelatedWebsiteSetsDataAccessEnabled(mProfile);
    }

    public boolean isRelatedWebsiteSetsDataAccessManaged() {
        return PrivacySandboxBridgeJni.get().isRelatedWebsiteSetsDataAccessManaged(mProfile);
    }

    public boolean isPartOfManagedRelatedWebsiteSet(String origin) {
        return PrivacySandboxBridgeJni.get().isPartOfManagedRelatedWebsiteSet(mProfile, origin);
    }

    public void setRelatedWebsiteSetsDataAccessEnabled(boolean enabled) {
        PrivacySandboxBridgeJni.get().setRelatedWebsiteSetsDataAccessEnabled(mProfile, enabled);
    }

    /**
     * Gets the Related Website Sets owner hostname given a RWS member origin.
     *
     * @param memberOrigin RWS member origin.
     * @return A string containing the owner hostname, null if it doesn't exist.
     */
    public String getRelatedWebsiteSetOwner(String memberOrigin) {
        return PrivacySandboxBridgeJni.get().getRelatedWebsiteSetOwner(mProfile, memberOrigin);
    }

    public void topicsToggleChanged(boolean newValue) {
        PrivacySandboxBridgeJni.get().topicsToggleChanged(mProfile, newValue);
    }

    public void setAllPrivacySandboxAllowedForTesting() {
        PrivacySandboxBridgeJni.get().setAllPrivacySandboxAllowedForTesting(mProfile); // IN-TEST
    }

    public void recordActivityType(@PrivacySandboxStorageActivityType int activityType) {
        PrivacySandboxBridgeJni.get().recordActivityType(mProfile, activityType);
    }

    public boolean privacySandboxPrivacyGuideShouldShowAdTopicsCard() {
        return PrivacySandboxBridgeJni.get()
                .privacySandboxPrivacyGuideShouldShowAdTopicsCard(mProfile);
    }

    public boolean shouldUsePrivacyPolicyChinaDomain() {
        return PrivacySandboxBridgeJni.get().shouldUsePrivacyPolicyChinaDomain(mProfile);
    }

    public String getEmbeddedPrivacyPolicyURL(
            @PrivacyPolicyDomainType int domainType,
            @PrivacyPolicyColorScheme int colorScheme,
            String locale) {
        return PrivacySandboxBridgeJni.get()
                .getEmbeddedPrivacyPolicyURL(domainType, colorScheme, locale);
    }

    @NativeMethods
    public interface Natives {
        boolean isPrivacySandboxRestricted(Profile profile);

        boolean isRestrictedNoticeEnabled(Profile profile);

        boolean isRelatedWebsiteSetsDataAccessEnabled(Profile profile);

        boolean isRelatedWebsiteSetsDataAccessManaged(Profile profile);

        boolean isPartOfManagedRelatedWebsiteSet(Profile profile, String origin);

        void setRelatedWebsiteSetsDataAccessEnabled(Profile profile, boolean enabled);

        String getRelatedWebsiteSetOwner(Profile profile, String memberOrigin);

        @JniType("std::vector")
        List<Topic> getCurrentTopTopics(Profile profile);

        @JniType("std::vector")
        List<Topic> getBlockedTopics(Profile profile);

        @JniType("std::vector")
        List<Topic> getFirstLevelTopics(Profile profile);

        @JniType("std::vector")
        List<Topic> getChildTopicsCurrentlyAssigned(
                Profile profile, int topicId, int taxonomyVersion);

        void setTopicAllowed(Profile profile, int topicId, int taxonomyVersion, boolean allowed);

        void getFledgeJoiningEtldPlusOneForDisplay(Profile profile, Callback<String[]> callback);

        @JniType("std::vector<std::string>")
        List<String> getBlockedFledgeJoiningTopFramesForDisplay(Profile profile);

        void setFledgeJoiningAllowed(Profile profile, String topFrameEtldPlus1, boolean allowed);

        int getRequiredPromptType(Profile profile, int surfaceType);

        void promptActionOccurred(Profile profile, int action, int surfaceType);

        void topicsToggleChanged(Profile profile, boolean newValue);

        void setAllPrivacySandboxAllowedForTesting(Profile profile); // IN-TEST

        void recordActivityType(Profile profile, int activityType);

        boolean privacySandboxPrivacyGuideShouldShowAdTopicsCard(Profile profile);

        boolean shouldUsePrivacyPolicyChinaDomain(Profile profile);

        String getEmbeddedPrivacyPolicyURL(int domainType, int colorScheme, String locale);
    }
}
