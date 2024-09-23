// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.DestroyObserver;
import org.chromium.chrome.browser.profiles.OTRProfileID;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileProvider;

/** Handles initializing an Activity appropriate ProfileProvider. */
public class ActivityProfileProvider extends OneshotSupplierImpl<ProfileProvider>
        implements ProfileManager.Observer, DestroyObserver {
    private OTRProfileID mOTRProfileID;
    private boolean mHasCreatedOTRProfileID;

    /**
     * Handles initialization of a ProfileProvider for a given Activity.
     *
     * @param lifecycleDispatcher Provides lifecycle events for the host activity.
     */
    public ActivityProfileProvider(@NonNull ActivityLifecycleDispatcher lifecycleDispatcher) {
        lifecycleDispatcher.register(this);

        if (ProfileManager.isInitialized()) {
            onProfileManagerReady();
        } else {
            ProfileManager.addObserver(this);
        }
    }

    @Override
    public void onProfileAdded(Profile profile) {
        assert ProfileManager.isInitialized();
        onProfileManagerReady();
        ProfileManager.removeObserver(this);
    }

    @Override
    public void onProfileDestroyed(Profile profile) {}

    private void onProfileManagerReady() {
        set(
                new ProfileProvider() {
                    @NonNull
                    @Override
                    public Profile getOriginalProfile() {
                        return ProfileManager.getLastUsedRegularProfile();
                    }

                    @Nullable
                    @Override
                    public Profile getOffTheRecordProfile(boolean createIfNeeded) {
                        Profile originalProfile = getOriginalProfile();
                        OTRProfileID otrProfileID = getOrCreateOTRProfileID();
                        return otrProfileID == null
                                ? originalProfile.getPrimaryOTRProfile(createIfNeeded)
                                : originalProfile.getOffTheRecordProfile(
                                        otrProfileID, createIfNeeded);
                    }

                    @Override
                    public boolean hasOffTheRecordProfile() {
                        Profile originalProfile = getOriginalProfile();
                        OTRProfileID otrProfileID = getOrCreateOTRProfileID();
                        return otrProfileID == null
                                ? originalProfile.hasPrimaryOTRProfile()
                                : originalProfile.hasOffTheRecordProfile(otrProfileID);
                    }
                });
    }

    @Nullable
    private OTRProfileID getOrCreateOTRProfileID() {
        if (!mHasCreatedOTRProfileID) {
            mOTRProfileID = createOffTheRecordProfileID();
            mHasCreatedOTRProfileID = true;
        }
        return mOTRProfileID;
    }

    /**
     * Create the OTRProfileID that should be used for the incognito profile of this provider. If
     * null, the default OffTheRecord profile will be used.
     */
    @Nullable
    protected OTRProfileID createOffTheRecordProfileID() {
        return null;
    }

    @Override
    public void onDestroy() {
        ProfileManager.removeObserver(this);
    }
}
