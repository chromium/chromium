// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import org.chromium.chrome.browser.profiles.Profile;

/** Java implementation of PrivacySandboxBridge for testing. */
public class FakePrivacySandboxBridge implements PrivacySandboxBridge.Natives {
    private boolean mIsRwsManaged /* = false*/;

    @Override
    public boolean isRelatedWebsiteSetsDataAccessEnabled(Profile profile) {
        return true;
    }

    @Override
    public boolean isRelatedWebsiteSetsDataAccessManaged(Profile profile) {
        return false;
    }

    @Override
    public boolean isPartOfManagedRelatedWebsiteSet(Profile profile, String origin) {
        return mIsRwsManaged;
    }

    @Override
    public void setRelatedWebsiteSetsDataAccessEnabled(Profile profile, boolean enabled) {}

    @Override
    public String getRelatedWebsiteSetOwner(Profile profile, String memberOrigin) {
        return null;
    }

    public void setIsRwsManaged(boolean managed) {
        mIsRwsManaged = managed;
    }
}
