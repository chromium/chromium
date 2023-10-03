// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/**
 * Contains helper methods for checking if we should update channels and updating them if so.
 */
public class ChannelsUpdater {
    private static final Object sLock = new Object();

    private final ChannelsInitializer mChannelsInitializer;
    private final SharedPreferencesManager mSharedPreferences;
    private final boolean mIsAtLeastO;
    private final int mChannelsVersion;

    public static ChannelsUpdater getInstance() {
        return LazyHolder.INSTANCE;
    }

    private static class LazyHolder {
        public static final ChannelsUpdater INSTANCE;

        static {
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) {
                INSTANCE = new ChannelsUpdater(false /* isAtLeastO */, null, null, -1);
            } else {
                // If pre-O, initialize with nulls as a small optimization to avoid getting
                // AppContext etc when we won't need it. It's ok for these parameters to be null
                // when mIsAtLeastO is false.
                INSTANCE = new ChannelsUpdater(true /* isAtLeastO */,
                        ChromeSharedPreferences.getInstance(),
                        new ChannelsInitializer(new NotificationManagerProxyImpl(
                                                        ContextUtils.getApplicationContext()),
                                ChromeChannelDefinitions.getInstance(),
                                ContextUtils.getApplicationContext().getResources()),
                        ChromeChannelDefinitions.CHANNELS_VERSION);
            }
        }
    }

    @VisibleForTesting
    ChannelsUpdater(boolean isAtLeastO, SharedPreferencesManager sharedPreferences,
            ChannelsInitializer channelsInitializer, int channelsVersion) {
        mIsAtLeastO = isAtLeastO;
        mSharedPreferences = sharedPreferences;
        mChannelsInitializer = channelsInitializer;
        mChannelsVersion = channelsVersion;
    }

    public boolean shouldUpdateChannels() {
        return mIsAtLeastO
                && mSharedPreferences.readInt(
                           ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, -1)
                != mChannelsVersion;
    }

    public void updateChannels() {
        synchronized (sLock) {
            if (!mIsAtLeastO) return;
            assert mChannelsInitializer != null;
            mChannelsInitializer.deleteLegacyChannels();
            mChannelsInitializer.initializeStartupChannels();
            storeChannelVersionInPrefs();
        }
    }

    public void updateLocale() {
        synchronized (sLock) {
            if (!mIsAtLeastO) return;
            assert mChannelsInitializer != null;
            mChannelsInitializer.updateLocale(ContextUtils.getApplicationContext().getResources());
        }
    }

    private void storeChannelVersionInPrefs() {
        assert mSharedPreferences != null;
        mSharedPreferences.writeInt(
                ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, mChannelsVersion);
    }
}
