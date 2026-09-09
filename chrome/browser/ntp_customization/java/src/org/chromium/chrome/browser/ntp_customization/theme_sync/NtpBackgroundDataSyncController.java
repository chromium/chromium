// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.ntp_customization.theme_sync;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.TriState;
import org.chromium.base.TriStateUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.ntp_customization.theme_sync.data.NtpBackgroundDataManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.sync.SyncServiceFactory;
import org.chromium.components.sync.SyncService;
import org.chromium.components.sync.SyncService.SyncStateChangedListener;
import org.chromium.components.sync.UserSelectableType;

/**
 * Coordinates and controls NTP background data in response to theme sync lifecycle events (e.g.
 * clearing remote platform data when theme sync is disabled).
 */
@NullMarked
public class NtpBackgroundDataSyncController implements SyncStateChangedListener {

    private @TriState int mIsThemeSyncEnabled;
    private @Nullable Profile mProfile;
    private @Nullable SyncService mSyncService;
    private @Nullable NtpBackgroundDataManager mBackgroundDataManager;

    private static @Nullable NtpBackgroundDataSyncController sInstanceForTesting;

    /** Static class that implements the initialization-on-demand holder idiom. */
    private static class LazyHolder {
        static final NtpBackgroundDataSyncController INSTANCE =
                new NtpBackgroundDataSyncController();
    }

    /** Returns the singleton instance of {@link NtpBackgroundDataSyncController}. */
    public static NtpBackgroundDataSyncController getInstance() {
        if (sInstanceForTesting != null) {
            return sInstanceForTesting;
        }
        return LazyHolder.INSTANCE;
    }

    @VisibleForTesting
    NtpBackgroundDataSyncController() {}

    /**
     * Initializes the controller with the user's profile and starts observing sync state.
     *
     * @param profile The current user profile.
     */
    public void onFinishNativeInitialization(Profile profile) {
        if (mProfile == profile) return;

        destroy();
        mProfile = profile;

        mSyncService = SyncServiceFactory.getForProfile(mProfile);
        if (mSyncService != null) {
            mSyncService.addSyncStateChangedListener(this);
        }

        mIsThemeSyncEnabled = TriStateUtils.from(isThemeSyncEnabled());
    }

    /** Cleans up observers and profile references. */
    public void destroy() {
        if (mSyncService != null) {
            mSyncService.removeSyncStateChangedListener(this);
            mSyncService = null;
        }
        mProfile = null;
        mIsThemeSyncEnabled = TriState.NOT_SET;
    }

    // SyncStateChangedListener implementation.

    @Override
    public void syncStateChanged() {
        @TriState int currentlyEnabled = TriStateUtils.from(isThemeSyncEnabled());
        if (mIsThemeSyncEnabled == currentlyEnabled) return;

        mIsThemeSyncEnabled = currentlyEnabled;
        if (currentlyEnabled == TriState.FALSE) {
            handleThemeSyncDisabled();
        }
    }

    /** Returns whether theme sync is currently enabled and active. */
    public boolean isThemeSyncEnabled() {
        return mSyncService != null
                && mSyncService.isEngineInitialized()
                && mSyncService.getSelectedTypes().contains(UserSelectableType.THEMES);
    }

    // Event handling methods.

    /** Handles actions when theme sync is disabled. */
    public void handleThemeSyncDisabled() {
        getBackgroundDataManager().removeAllNonAndroidPlatformData();
    }

    private NtpBackgroundDataManager getBackgroundDataManager() {
        if (mBackgroundDataManager == null) {
            // It is fine to use the application context here since we don't use
            // mBackgroundDataManager to load data, but only to remove theme sync data. Otherwise,
            // use the activity context instead.
            mBackgroundDataManager =
                    new NtpBackgroundDataManager(ContextUtils.getApplicationContext());
        }
        return mBackgroundDataManager;
    }

    public static void setInstanceForTesting(@Nullable NtpBackgroundDataSyncController instance) {
        sInstanceForTesting = instance;
        ResettersForTesting.register(() -> sInstanceForTesting = null);
    }

    public @TriState int getIsThemeSyncEnabledForTesting() {
        return mIsThemeSyncEnabled;
    }

    public void setBackgroundDataManagerForTesting(NtpBackgroundDataManager backgroundDataManager) {
        NtpBackgroundDataManager oldInstance = mBackgroundDataManager;
        mBackgroundDataManager = backgroundDataManager;
        ResettersForTesting.register(() -> mBackgroundDataManager = oldInstance);
    }
}
