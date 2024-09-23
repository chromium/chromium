// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import androidx.annotation.Nullable;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

import java.util.HashMap;
import java.util.Map;

/** Reports profile settings for users in a family group. */
@JNINamespace("chrome::android")
public class FamilyInfoFeedbackSource implements AsyncFeedbackSource {
    // LINT.IfChange
    private static final String FAMILY_MEMBER_ROLE = "Family_Member_Role";
    // LINT.ThenChange(//components/supervised_user/core/common/supervised_user_constants.h)
    private static final String PARENTAL_CONTROL_SITES_CHILD = "Parental_Control_Sites_Child";

    private final Profile mProfile;
    private Map<String, String> mFeedbackMap = new HashMap<>();
    private boolean mIsReady;
    private Runnable mCallback;

    public FamilyInfoFeedbackSource(Profile profile) {
        mProfile = profile;
    }

    // AsyncFeedbackSource implementation.
    @Override
    public void start(final Runnable callback) {
        mCallback = callback;
        FamilyInfoFeedbackSourceJni.get().start(this, mProfile);
    }

    private void processFamilyMemberRole(String familyRole) {
        // Adds a family role only if the user is enrolled in a Family group.
        if (!familyRole.isEmpty()) {
            mFeedbackMap.put(FAMILY_MEMBER_ROLE, familyRole);
        }
    }

    private void processParentalControlSitesChild(String webFilterType) {
        // Adds the parental control sites web filter for child users.
        assert mProfile.isChild();
        assert !webFilterType.isEmpty();
        mFeedbackMap.put(PARENTAL_CONTROL_SITES_CHILD, webFilterType);
    }

    @CalledByNative
    private void processPrimaryAccountFamilyInfo(
            String familyRole, @Nullable String webFilterType) {
        processFamilyMemberRole(familyRole);

        if (webFilterType != null) {
            processParentalControlSitesChild(webFilterType);
        }

        mIsReady = true;
        if (mCallback != null) {
            mCallback.run();
        }
    }

    @Override
    public boolean isReady() {
        return mIsReady;
    }

    @Override
    public Map<String, String> getFeedback() {
        return mFeedbackMap;
    }

    @NativeMethods
    interface Natives {
        void start(FamilyInfoFeedbackSource source, @JniType("Profile*") Profile profile);
    }
}
