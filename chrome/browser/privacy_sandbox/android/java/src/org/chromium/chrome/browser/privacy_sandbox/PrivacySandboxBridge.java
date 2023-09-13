// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.base.Callback;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Bridge, providing access to the native-side Privacy Sandbox configuration. */
// TODO(crbug.com/1410601): Pass in the profile and remove GetActiveUserProfile in C++.
public class PrivacySandboxBridge {
    public static boolean isPrivacySandboxEnabled() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxEnabled();
    }

    public static boolean isPrivacySandboxManaged() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxManaged();
    }

    public static boolean isPrivacySandboxRestricted() {
        return PrivacySandboxBridgeJni.get().isPrivacySandboxRestricted();
    }

    public static boolean isRestrictedNoticeEnabled() {
        return PrivacySandboxBridgeJni.get().isRestrictedNoticeEnabled();
    }

    public static void setPrivacySandboxEnabled(boolean enabled) {
        PrivacySandboxBridgeJni.get().setPrivacySandboxEnabled(enabled);
    }

    public static List<Topic> getCurrentTopTopics() {
        return sortTopics(Arrays.asList(PrivacySandboxBridgeJni.get().getCurrentTopTopics()));
    }

    public static List<Topic> getBlockedTopics() {
        return sortTopics(Arrays.asList(PrivacySandboxBridgeJni.get().getBlockedTopics()));
    }

    public static void setTopicAllowed(Topic topic, boolean allowed) {
        PrivacySandboxBridgeJni.get().setTopicAllowed(
                topic.getTopicId(), topic.getTaxonomyVersion(), allowed);
    }

    @CalledByNative
    private static Topic createTopic(int topicId, int taxonomyVersion, String name) {
        return new Topic(topicId, taxonomyVersion, name);
    }

    private static List<Topic> sortTopics(List<Topic> topics) {
        Collections.sort(topics, (o1, o2) -> { return o1.getName().compareTo(o2.getName()); });
        return topics;
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

    @NativeMethods
    public interface Natives {
        boolean isPrivacySandboxEnabled();
        boolean isPrivacySandboxManaged();
        boolean isPrivacySandboxRestricted();
        boolean isRestrictedNoticeEnabled();
        boolean isFirstPartySetsDataAccessEnabled();
        boolean isFirstPartySetsDataAccessManaged();
        boolean isPartOfManagedFirstPartySet(String origin);
        void setPrivacySandboxEnabled(boolean enabled);
        void setFirstPartySetsDataAccessEnabled(boolean enabled);
        String getFirstPartySetOwner(String memberOrigin);
        Topic[] getCurrentTopTopics();
        Topic[] getBlockedTopics();
        void setTopicAllowed(int topicId, int taxonomyVersion, boolean allowed);
        void getFledgeJoiningEtldPlusOneForDisplay(Callback<String[]> callback);
        String[] getBlockedFledgeJoiningTopFramesForDisplay();
        void setFledgeJoiningAllowed(String topFrameEtldPlus1, boolean allowed);
        int getRequiredPromptType();
        void promptActionOccurred(int action);
        void topicsToggleChanged(boolean newValue);
    }
}
