// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;

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

    public void setAllPrivacySandboxAllowedForTesting() {
        PrivacySandboxBridgeJni.get().setAllPrivacySandboxAllowedForTesting(mProfile); // IN-TEST
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

        void setAllPrivacySandboxAllowedForTesting(Profile profile); // IN-TEST

        boolean shouldUsePrivacyPolicyChinaDomain(Profile profile);

        String getEmbeddedPrivacyPolicyURL(int domainType, int colorScheme, String locale);
    }
}
