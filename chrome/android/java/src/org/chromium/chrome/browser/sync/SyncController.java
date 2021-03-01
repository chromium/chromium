// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.sync;

import android.annotation.SuppressLint;

import androidx.annotation.Nullable;

import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.sync.StopSource;

/**
 * SyncController handles the coordination of sync state between the invalidation controller,
 * the Android sync settings, and the native sync code.
 *
 * It also handles initialization of some pieces of sync state on startup.
 *
 * Sync state can be changed from four places:
 *
 * - The Chrome UI, which will call SyncController directly.
 * - Native sync, which can disable it via a dashboard stop and clear.
 * - Android's Chrome sync setting.
 * - Android's master sync setting.
 *
 * SyncController implements listeners for the last three cases. When master sync is disabled, we
 * are careful to not change the Android Chrome sync setting so we know whether to turn sync back
 * on when it is re-enabled.
 */
public class SyncController
        implements ProfileSyncService.SyncStateChangedListener, AndroidSyncSettings.Delegate {
    private static final String TAG = "SyncController";

    @SuppressLint("StaticFieldLeak")
    private static SyncController sInstance;
    private static boolean sInitialized;

    private final ProfileSyncService mProfileSyncService;

    private SyncController() {
        AndroidSyncSettings.get().setDelegate(this);
        mProfileSyncService = ProfileSyncService.get();
        mProfileSyncService.addSyncStateChangedListener(this);

        updateSyncStateFromAndroid();
    }

    /**
     * Retrieve the singleton instance of this class.
     *
     * @return the singleton instance.
     */
    @Nullable
    public static SyncController get() {
        ThreadUtils.assertOnUiThread();
        if (!sInitialized) {
            if (ProfileSyncService.get() != null) {
                sInstance = new SyncController();
            }
            sInitialized = true;
        }
        return sInstance;
    }

    /**
     * Updates sync to reflect the state of the Android sync settings.
     */
    private void updateSyncStateFromAndroid() {
        // Note: |isChromeSyncEnabled| maps to SyncRequested, and
        // |isMasterSyncEnabled| maps to *both* SyncRequested and
        // SyncAllowedByPlatform.
        // TODO(crbug.com/921025): Don't mix these two concepts.

        mProfileSyncService.setSyncAllowedByPlatform(
                AndroidSyncSettings.get().doesMasterSyncSettingAllowChromeSync());

        if (isSyncEnabledInAndroidSyncSettings() == mProfileSyncService.isSyncRequested()) return;
        if (isSyncEnabledInAndroidSyncSettings()) {
            mProfileSyncService.setSyncRequested(true);
            return;
        }

        if (Profile.getLastUsedRegularProfile().isChild()) {
            // For child accounts, Sync needs to stay enabled, so we reenable it in settings.
            // TODO(bauerb): Remove the dependency on child account code and instead go through
            // prefs (here and in the Sync customization UI).
            AndroidSyncSettings.get().enableChromeSync();
        } else {
            // On sign-out, Sync.StopSource is already recorded in the native code, so only
            // record it here if there's still a primary (syncing) account.
            // TODO(crbug.com/1105795): Revisit how these metrics are recorded.
            if (mProfileSyncService.isAuthenticatedAccountPrimary()) {
                int source = !AndroidSyncSettings.get().doesMasterSyncSettingAllowChromeSync()
                        ? StopSource.ANDROID_MASTER_SYNC
                        : StopSource.ANDROID_CHROME_SYNC;
                RecordHistogram.recordEnumeratedHistogram(
                        "Sync.StopSource", source, StopSource.STOP_SOURCE_LIMIT);
            }

            mProfileSyncService.setSyncRequested(false);
        }
    }

    /**
     * From {@link ProfileSyncService.SyncStateChangedListener}.
     *
     * Changes the invalidation controller and Android sync setting state to match
     * the new native sync state.
     */
    @Override
    public void syncStateChanged() {
        ThreadUtils.assertOnUiThread();
        if (mProfileSyncService.isSyncRequested()) {
            if (!isSyncEnabledInAndroidSyncSettings()) {
                AndroidSyncSettings.get().enableChromeSync();
            }
        } else {
            if (isSyncEnabledInAndroidSyncSettings()) {
                // Both Android's master and Chrome sync setting are enabled, so we want to disable
                // the Chrome sync setting to match isSyncRequested. We have to be careful not to
                // disable it when isSyncRequested becomes false due to master sync being disabled
                // so that sync will turn back on if master sync is re-enabled.
                // TODO(crbug.com/921025): Master sync shouldn't influence isSyncRequested.
                AndroidSyncSettings.get().disableChromeSync();
            }
        }
    }

    /**
     * From {@link AndroidSyncSettings.Delegate}.
     */
    @Override
    public void androidSyncSettingsChanged() {
        updateSyncStateFromAndroid();
    }

    /**
     * Checks both the master sync for the device, and Chrome sync setting for the given account.
     * If no user is currently signed in it returns false.
     */
    private boolean isSyncEnabledInAndroidSyncSettings() {
        return AndroidSyncSettings.get().doesMasterSyncSettingAllowChromeSync()
                && AndroidSyncSettings.get().isChromeSyncEnabled();
    }
}
