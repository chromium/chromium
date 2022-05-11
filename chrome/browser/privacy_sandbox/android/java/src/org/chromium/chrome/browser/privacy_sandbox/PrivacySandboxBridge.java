// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/** Bridge, providing access to the native-side Privacy Sandbox configuration. */
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

    public static void setPrivacySandboxEnabled(boolean enabled) {
        PrivacySandboxBridgeJni.get().setPrivacySandboxEnabled(enabled);
    }

    public static boolean isFlocEnabled() {
        return PrivacySandboxBridgeJni.get().isFlocEnabled();
    }

    public static void setFlocEnabled(boolean enabled) {
        PrivacySandboxBridgeJni.get().setFlocEnabled(enabled);
    }

    public static boolean isFlocIdResettable() {
        return PrivacySandboxBridgeJni.get().isFlocIdResettable();
    }

    public static void resetFlocId() {
        PrivacySandboxBridgeJni.get().resetFlocId();
    }

    public static String getFlocStatusString() {
        return PrivacySandboxBridgeJni.get().getFlocStatusString();
    }

    public static String getFlocGroupString() {
        return PrivacySandboxBridgeJni.get().getFlocGroupString();
    }

    public static String getFlocUpdateString() {
        return PrivacySandboxBridgeJni.get().getFlocUpdateString();
    }

    public static String getFlocDescriptionString() {
        return PrivacySandboxBridgeJni.get().getFlocDescriptionString();
    }

    public static String getFlocResetExplanationString() {
        return PrivacySandboxBridgeJni.get().getFlocResetExplanationString();
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

    public static @DialogType int getRequiredDialogType() {
        return PrivacySandboxBridgeJni.get().getRequiredDialogType();
    }

    public static void dialogActionOccurred(@DialogAction int action) {
        PrivacySandboxBridgeJni.get().dialogActionOccurred(action);
    }

    @NativeMethods
    public interface Natives {
        boolean isPrivacySandboxEnabled();
        boolean isPrivacySandboxManaged();
        boolean isPrivacySandboxRestricted();
        void setPrivacySandboxEnabled(boolean enabled);
        boolean isFlocEnabled();
        void setFlocEnabled(boolean enabled);
        boolean isFlocIdResettable();
        void resetFlocId();
        String getFlocStatusString();
        String getFlocGroupString();
        String getFlocUpdateString();
        String getFlocDescriptionString();
        String getFlocResetExplanationString();
        Topic[] getCurrentTopTopics();
        Topic[] getBlockedTopics();
        void setTopicAllowed(int topicId, int taxonomyVersion, boolean allowed);
        int getRequiredDialogType();
        void dialogActionOccurred(int action);
    }
}
