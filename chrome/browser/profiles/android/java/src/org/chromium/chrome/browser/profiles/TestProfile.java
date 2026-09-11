// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.profiles;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.BrowserStartupController;

import java.util.HashMap;
import java.util.Map;

/**
 * An in-memory implementation of {@link Profile} for unit tests and non-browser on-device
 * integration tests.
 */
@NullMarked
public class TestProfile extends Profile {
    private final @Nullable TestProfile mOriginalProfile;
    private final Map<OtrProfileId, TestProfile> mOtrProfiles = new HashMap<>();

    private TestProfile(
            @Nullable TestProfile originalProfile, @Nullable OtrProfileId otrProfileId) {
        super(/* nativeProfile= */ 0L, otrProfileId);
        assertBrowserProcessNotStarted();
        mOriginalProfile = originalProfile;
    }

    /** Asserts that the full C++ browser process has not been instantiated. */
    public static void assertBrowserProcessNotStarted() {
        boolean isFullBrowserStarted =
                ThreadUtils.runOnUiThreadBlocking(
                        () -> BrowserStartupController.getInstance().isFullBrowserStarted());
        assert !isFullBrowserStarted
                : "TestProfile should only be used in tests where the browser process is not"
                        + " started.";
    }

    /** Creates a regular (non-OTR) in-memory profile. */
    public static TestProfile createRegular() {
        return new TestProfile(/* originalProfile= */ null, /* otrProfileId= */ null);
    }

    /** Creates a primary incognito in-memory profile. */
    public static TestProfile createIncognito() {
        TestProfile original = createRegular();
        return original.getOrCreatePrimaryOtrProfile();
    }

    /**
     * Creates a primary incognito in-memory profile associated with the given regular profile.
     *
     * @param originalProfile The original regular profile.
     */
    public static TestProfile createIncognito(TestProfile originalProfile) {
        assert originalProfile != null && !originalProfile.isOffTheRecord();
        return originalProfile.getOrCreatePrimaryOtrProfile();
    }

    /** Creates an incognito CCT in-memory profile. */
    public static TestProfile createIncognitoCct() {
        TestProfile original = createRegular();
        return original.getOrCreateOffTheRecordProfile(
                new OtrProfileId("CCT:Incognito::test_profile"));
    }

    /**
     * Creates a custom OTR in-memory profile with the given prefix.
     *
     * @param prefix The prefix for the OTR profile.
     */
    public static TestProfile createCustomOtr(String prefix) {
        TestProfile original = createRegular();
        return original.getOrCreateOffTheRecordProfile(new OtrProfileId(prefix));
    }

    /**
     * Creates a custom OTR in-memory profile with the given OtrProfileId.
     *
     * @param otrProfileId The OTR profile ID.
     */
    public static TestProfile createCustomOtr(OtrProfileId otrProfileId) {
        TestProfile original = createRegular();
        return original.getOrCreateOffTheRecordProfile(otrProfileId);
    }

    @Override
    public TestProfile getOriginalProfile() {
        return mOriginalProfile != null ? mOriginalProfile : this;
    }

    @Override
    public long getCreationTime() {
        return 0L;
    }

    @Override
    public boolean isInitialProfile() {
        return !isOffTheRecord();
    }

    @Override
    public @Nullable TestProfile getOffTheRecordProfile(
            OtrProfileId profileId, boolean createIfNeeded) {
        assert profileId != null;
        if (mOriginalProfile != null) {
            return mOriginalProfile.getOffTheRecordProfile(profileId, createIfNeeded);
        }
        TestProfile profile = mOtrProfiles.get(profileId);
        if (profile == null && createIfNeeded) {
            profile = new TestProfile(this, profileId);
            mOtrProfiles.put(profileId, profile);
        }
        return profile;
    }

    @Override
    public TestProfile getOrCreateOffTheRecordProfile(OtrProfileId profileId) {
        TestProfile profile = getOffTheRecordProfile(profileId, /* createIfNeeded= */ true);
        assert profile != null;
        return profile;
    }

    @Override
    public @Nullable TestProfile getPrimaryOtrProfile(boolean createIfNeeded) {
        return getOffTheRecordProfile(OtrProfileId.getPrimaryOtrProfileId(), createIfNeeded);
    }

    @Override
    public TestProfile getOrCreatePrimaryOtrProfile() {
        return getOrCreateOffTheRecordProfile(OtrProfileId.getPrimaryOtrProfileId());
    }

    @Override
    public boolean hasOffTheRecordProfile(OtrProfileId profileId) {
        assert profileId != null;
        if (mOriginalProfile != null) {
            return mOriginalProfile.hasOffTheRecordProfile(profileId);
        }
        return mOtrProfiles.containsKey(profileId);
    }

    @Override
    public boolean hasPrimaryOtrProfile() {
        return hasOffTheRecordProfile(OtrProfileId.getPrimaryOtrProfileId());
    }

    @Override
    public ProfileKey getProfileKey() {
        throw new UnsupportedOperationException("ProfileKey is not supported in TestProfile.");
    }

    @Deprecated
    @Override
    public boolean isChild() {
        return false;
    }

    @Override
    public void wipe() {
        throw new UnsupportedOperationException("wipe() is not supported in TestProfile.");
    }
}
