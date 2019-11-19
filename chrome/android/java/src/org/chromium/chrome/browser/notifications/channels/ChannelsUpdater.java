// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;

import android.content.SharedPreferences;
import android.os.Build;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.notifications.NotificationManagerProxyImpl;

/**
 * Contains helper methods for checking if we should update channels and updating them if so.
 */
public class ChannelsUpdater {
    @VisibleForTesting
    static final String CHANNELS_VERSION_KEY = "channels_version_key";

    private static final Object sLock = new Object();

    private final ChannelsInitializer mChannelsInitializer;
    private final SharedPreferences mSharedPreferences;
    private final boolean mIsAtLeastO;
    private final int mChannelsVersion;

    public static ChannelsUpdater getInstance() {
        return LazyHolder.INSTANCE;
    }

    private static class LazyHolder {
        // If pre-O, initialize with nulls as a small optimization to avoid getting AppContext etc
        // when we won't need it. It's ok for these parameters to be null when mIsAtLeastO is false.
        public static final ChannelsUpdater INSTANCE = Build.VERSION.SDK_INT < Build.VERSION_CODES.O
                ? new ChannelsUpdater(false /* isAtLeastO */, null, null, -1)
                : new ChannelsUpdater(true /* isAtLeastO */, ContextUtils.getAppSharedPreferences(),
                          new ChannelsInitializer(new NotificationManagerProxyImpl(
                                                          ContextUtils.getApplicationContext()),
                                  ContextUtils.getApplicationContext().getResources()),
                          ChannelDefinitions.CHANNELS_VERSION);
    }

    @VisibleForTesting
    ChannelsUpdater(boolean isAtLeastO, SharedPreferences sharedPreferences,
            ChannelsInitializer channelsInitializer, int channelsVersion) {
        mIsAtLeastO = isAtLeastO;
        mSharedPreferences = sharedPreferences;
        mChannelsInitializer = channelsInitializer;
        mChannelsVersion = channelsVersion;
    }

    public boolean shouldUpdateChannels() {
        return mIsAtLeastO
                && mSharedPreferences.getInt(CHANNELS_VERSION_KEY, -1) != mChannelsVersion;
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
        mSharedPreferences.edit().putInt(CHANNELS_VERSION_KEY, mChannelsVersion).apply();
    }
}
