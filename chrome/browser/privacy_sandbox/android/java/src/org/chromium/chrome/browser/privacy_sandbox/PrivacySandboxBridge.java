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

    @NativeMethods
    public interface Natives {
        boolean isRelatedWebsiteSetsDataAccessEnabled(Profile profile);

        boolean isRelatedWebsiteSetsDataAccessManaged(Profile profile);

        boolean isPartOfManagedRelatedWebsiteSet(Profile profile, String origin);

        void setRelatedWebsiteSetsDataAccessEnabled(Profile profile, boolean enabled);

        String getRelatedWebsiteSetOwner(Profile profile, String memberOrigin);
    }
}
