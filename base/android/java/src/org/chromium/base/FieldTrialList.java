// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

/** Helper to get field trial information. */
public class FieldTrialList {

    private FieldTrialList() {}

    /**
     * @param trialName The name of the trial to get the group for.
     * @return The group name chosen for the named trial, or the empty string if the trial does
     *         not exist.
     */
    public static String findFullName(String trialName) {
        return FieldTrialListJni.get().findFullName(trialName);
    }

    /**
     * @param trialName The name of the trial to get the group for.
     * @return Whether the trial exists or not.
     */
    public static boolean trialExists(String trialName) {
        return FieldTrialListJni.get().trialExists(trialName);
    }

    /**
     * @param trialName    The name of the trial with the parameter.
     * @param parameterKey The key of the parameter.
     * @return The value of the parameter or an empty string if not found.
     */
    public static String getVariationParameter(String trialName, String parameterKey) {
        return FieldTrialListJni.get().getVariationParameter(trialName, parameterKey);
    }

    /**
     * Print active trials and their group assignments to logcat, for debugging purposes. Continue
     * printing new trials as they become active. This should be called at most once.
     */
    public static void logActiveTrials() {
        FieldTrialListJni.get().logActiveTrials();
    }

    /**
     * @param trialName The name of the trial to create.
     * @param groupName The name of the group to set.
     * @return True on success, false if there was already a field trial of the same name but with a
     *         different finalized {@code groupName}.
     */
    public static boolean createFieldTrial(String trialName, String groupName) {
        return FieldTrialListJni.get().createFieldTrial(trialName, groupName);
    }

    @NativeMethods
    interface Natives {
        @JniType("std::string")
        String findFullName(@JniType("std::string") String trialName);

        boolean trialExists(@JniType("std::string") String trialName);

        @JniType("std::string")
        String getVariationParameter(
                @JniType("std::string") String trialName,
                @JniType("std::string") String parameterKey);

        void logActiveTrials();

        boolean createFieldTrial(
                @JniType("std::string") String trialName, @JniType("std::string") String groupName);
    }
}
