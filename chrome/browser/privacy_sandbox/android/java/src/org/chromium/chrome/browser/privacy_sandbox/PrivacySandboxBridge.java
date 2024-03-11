// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;

import java.util.Arrays;
import java.util.List;

/** Bridge, providing access to the native-side Privacy Sandbox configuration. */
// TODO(crbug.com/1410601): Pass in the profile and remove GetActiveUserProfile in C++.
public class PrivacySandboxBridge {

    public static boolean isPrivacySandboxRestricted() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxRestricted();
    }

    public static boolean isRestrictedNoticeEnabled() {
        return PrivacySandboxBridgeJni.get().isRestrictedNoticeEnabled();
    }

    public static List<Topic> getCurrentTopTopics() {
        return sortTopics(PrivacySandboxBridgeJni.get().getCurrentTopTopics());
    }

    public static List<Topic> getBlockedTopics() {
        return sortTopics(PrivacySandboxBridgeJni.get().getBlockedTopics());
    }

    public static List<Topic> getFirstLevelTopics() {
        return sortTopics(PrivacySandboxBridgeJni.get().getFirstLevelTopics());
    }

    public static List<Topic> getChildTopicsCurrentlyAssigned(Topic topic) {
        return sortTopics(
                PrivacySandboxBridgeJni.get()
                        .getChildTopicsCurrentlyAssigned(
                                topic.getTopicId(), topic.getTaxonomyVersion()));
    }

    public static void setTopicAllowed(Topic topic, boolean allowed) {
        PrivacySandboxBridgeJni.get()
                .setTopicAllowed(topic.getTopicId(), topic.getTaxonomyVersion(), allowed);
    }

    @CalledByNative
    private static Topic createTopic(
            int topicId, int taxonomyVersion, String name, String description) {
        return new Topic(topicId, taxonomyVersion, name, description);
    }

    private static List<Topic> sortTopics(Object[] topics) {
        Arrays.sort(
                topics,
                (o1, o2) -> {
                    return ((Topic) o1).getName().compareTo(((Topic) o2).getName());
                });
        return (List<Topic>) (List<?>) Arrays.asList(topics);
    }

    public static void getFledgeJoiningEtldPlusOneForDisplay(Callback<List<String>> callback) {
        Callback<String[]> arrayCallback =
                (String[] domains) -> callback.onResult(Arrays.asList(domains));
        PrivacySandboxBridgeJni.get().getFledgeJoiningEtldPlusOneForDisplay(arrayCallback);
    }

    public static List<String> getBlockedFledgeJoiningTopFramesForDisplay() {
        return Arrays.asList(
                PrivacySandboxBridgeJni.get().getBlockedFledgeJoiningTopFramesForDisplay());
    }

    public static void setFledgeJoiningAllowed(String topFrameEtldPlus1, boolean allowed) {
        PrivacySandboxBridgeJni.get().setFledgeJoiningAllowed(topFrameEtldPlus1, allowed);
    }

    public static @PromptType int getRequiredPromptType() {
        return PrivacySandboxBridgeJni.get().getRequiredPromptType();
    }

    public static void promptActionOccurred(@PromptAction int action) {
        PrivacySandboxBridgeJni.get().promptActionOccurred(action);
    }

    public static boolean isFirstPartySetsDataAccessEnabled() {
        return PrivacySandboxBridgeJni.get().isFirstPartySetsDataAccessEnabled();
    }

    public static boolean isFirstPartySetsDataAccessManaged() {
        return PrivacySandboxBridgeJni.get().isFirstPartySetsDataAccessManaged();
    }

    public static boolean isPartOfManagedFirstPartySet(String origin) {
        return PrivacySandboxBridgeJni.get().isPartOfManagedFirstPartySet(origin);
    }

    public static void setFirstPartySetsDataAccessEnabled(boolean enabled) {
        PrivacySandboxBridgeJni.get().setFirstPartySetsDataAccessEnabled(enabled);
    }

    /**
     * Gets the First Party Sets owner hostname given a FPS member origin.
     * @param memberOrigin FPS member origin.
     * @return A string containing the owner hostname, null if it doesn't exist.
     */
    public static String getFirstPartySetOwner(String memberOrigin) {
        return PrivacySandboxBridgeJni.get().getFirstPartySetOwner(memberOrigin);
    }

    public static void topicsToggleChanged(boolean newValue) {
        PrivacySandboxBridgeJni.get().topicsToggleChanged(newValue);
    }

    public static void setAllPrivacySandboxAllowedForTesting() {
        PrivacySandboxBridgeJni.get().setAllPrivacySandboxAllowedForTesting(); // IN-TEST
    }

    @NativeMethods
    public interface Natives {
        boolean isPrivacySandboxRestricted();

        boolean isRestrictedNoticeEnabled();

        boolean isFirstPartySetsDataAccessEnabled();

        boolean isFirstPartySetsDataAccessManaged();

        boolean isPartOfManagedFirstPartySet(String origin);

        void setFirstPartySetsDataAccessEnabled(boolean enabled);

        String getFirstPartySetOwner(String memberOrigin);

        @JniType("std::vector")
        Object[] getCurrentTopTopics();

        @JniType("std::vector")
        Object[] getBlockedTopics();

        @JniType("std::vector")
        Object[] getFirstLevelTopics();

        @JniType("std::vector")
        Object[] getChildTopicsCurrentlyAssigned(int topicId, int taxonomyVersion);

        void setTopicAllowed(int topicId, int taxonomyVersion, boolean allowed);

        void getFledgeJoiningEtldPlusOneForDisplay(Callback<String[]> callback);

        String[] getBlockedFledgeJoiningTopFramesForDisplay();

        void setFledgeJoiningAllowed(String topFrameEtldPlus1, boolean allowed);

        int getRequiredPromptType();

        void promptActionOccurred(int action);

        void topicsToggleChanged(boolean newValue);

        void setAllPrivacySandboxAllowedForTesting(); // IN-TEST
    }
}
