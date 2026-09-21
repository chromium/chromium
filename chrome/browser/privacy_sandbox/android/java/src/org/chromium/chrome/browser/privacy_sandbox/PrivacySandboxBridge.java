// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.profiles.Profile;

/** Bridge, providing access to the native-side Privacy Sandbox configuration. */
@NullMarked
public class PrivacySandboxBridge {

    private final Profile mProfile;

    public PrivacySandboxBridge(Profile profile) {
        mProfile = profile;
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
    public @Nullable String getRelatedWebsiteSetOwner(String memberOrigin) {
        return PrivacySandboxBridgeJni.get().getRelatedWebsiteSetOwner(mProfile, memberOrigin);
    }

    @NativeMethods
    public interface Natives {
        boolean isRelatedWebsiteSetsDataAccessEnabled(@JniType("Profile*") Profile profile);

        boolean isRelatedWebsiteSetsDataAccessManaged(@JniType("Profile*") Profile profile);

        boolean isPartOfManagedRelatedWebsiteSet(
                @JniType("Profile*") Profile profile, @JniType("std::string") String origin);

        void setRelatedWebsiteSetsDataAccessEnabled(
                @JniType("Profile*") Profile profile, boolean enabled);

        @Nullable String getRelatedWebsiteSetOwner(
                @JniType("Profile*") Profile profile, @JniType("std::string") String memberOrigin);
    }
}
