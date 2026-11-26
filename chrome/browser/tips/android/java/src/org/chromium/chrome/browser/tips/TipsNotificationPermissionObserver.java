// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tips;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.notifications.channels.ChromeChannelDefinitions.ChannelId;
import org.chromium.chrome.browser.notifications.permissions.NotificationPermissionChangeNotifier;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;

/**
 * Process-wide observer that keeps the cached Magic Stack Tips opt-in eligibility pref fresh when
 * app-level notification permission or the Tips channel state changes. Kept separate from {@link
 * TipsUtils} so registering it during {@code Application.onCreate()} does not load {@code
 * TipsUtils} on every browser process start.
 */
@NullMarked
public class TipsNotificationPermissionObserver
        implements NotificationPermissionChangeNotifier.Observer {
    private static class LazyHolder {
        static final TipsNotificationPermissionObserver INSTANCE =
                new TipsNotificationPermissionObserver();
    }

    private TipsNotificationPermissionObserver() {}

    /**
     * Registers the singleton observer. Safe to call multiple times and before native is
     * initialized.
     */
    public static void register() {
        NotificationPermissionChangeNotifier.addObserver(LazyHolder.INSTANCE);
    }

    static void unregisterForTesting() {
        NotificationPermissionChangeNotifier.removeObserver(LazyHolder.INSTANCE);
    }

    @Override
    public void onAppNotificationPermissionChanged(boolean enabled) {
        if (!enabled) {
            TipsUtils.clearTipsNotificationsOptInEligibilityPref(
                    ChromeSharedPreferences.getInstance());
            return;
        }
        // Safe to call before native is initialized; deferred startup refreshes again as a
        // backstop if a cold broadcast process exits before the async channel lookup finishes.
        TipsUtils.updateTipsNotificationsOptInEligibilityPref(
                ChromeSharedPreferences.getInstance());
    }

    @Override
    public void onNotificationChannelStateChanged(String channelId, boolean enabled) {
        if (!ChannelId.TIPS_V2.equals(channelId)) return;

        if (enabled) {
            TipsUtils.clearTipsNotificationsOptInEligibilityPref(
                    ChromeSharedPreferences.getInstance());
            return;
        }
        TipsUtils.updateTipsNotificationsOptInEligibilityPref(
                ChromeSharedPreferences.getInstance());
    }
}
