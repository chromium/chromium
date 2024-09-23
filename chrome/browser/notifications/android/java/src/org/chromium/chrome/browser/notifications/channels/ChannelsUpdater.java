// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.notifications.channels;


import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.shared_preferences.SharedPreferencesManager;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.components.browser_ui.notifications.BaseNotificationManagerProxyFactory;
import org.chromium.components.browser_ui.notifications.channels.ChannelsInitializer;

/** Contains helper methods for checking if we should update channels and updating them if so. */
public class ChannelsUpdater {
    private static final Object sLock = new Object();

    private final ChannelsInitializer mChannelsInitializer;
    private final SharedPreferencesManager mSharedPreferences;
    private final int mChannelsVersion;

    public static ChannelsUpdater getInstance() {
        return LazyHolder.INSTANCE;
    }

    private static class LazyHolder {
        public static final ChannelsUpdater INSTANCE;

        static {
            INSTANCE =
                    new ChannelsUpdater(
                            ChromeSharedPreferences.getInstance(),
                            new ChannelsInitializer(
                                    BaseNotificationManagerProxyFactory.create(
                                            ContextUtils.getApplicationContext()),
                                    ChromeChannelDefinitions.getInstance(),
                                    ContextUtils.getApplicationContext().getResources()),
                            ChromeChannelDefinitions.CHANNELS_VERSION);
        }
    }

    @VisibleForTesting
    ChannelsUpdater(
            SharedPreferencesManager sharedPreferences,
            ChannelsInitializer channelsInitializer,
            int channelsVersion) {
        mSharedPreferences = sharedPreferences;
        mChannelsInitializer = channelsInitializer;
        mChannelsVersion = channelsVersion;
    }

    public boolean shouldUpdateChannels() {
        return mSharedPreferences.readInt(ChromePreferenceKeys.NOTIFICATIONS_CHANNELS_VERSION, -1)
                != mChannelsVersion;
    }

    public void updateChannels() {
        synchronized (sLock) {
            assert mChannelsInitializer != null;
            mChannelsInitializer.deleteLegacyChannels();
            mChannelsInitializer.initializeStartupChannels();
            storeChannelVersionInPrefs();
        }
    }

    public void updateLocale() {
        synchronized (sLock) {
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
