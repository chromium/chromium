// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;

/** Thin wrapper for {@link MediaNotificationManager}. */
@NullMarked
public class ChromeMediaNotificationManager {
    /**
     * Shows a media notification. Passes through to {@link MediaNotificationManager}, utilizing a
     * Chrome specific delegate.
     *
     * @param notificationInfo information to show in the notification
     */
    public static void show(MediaNotificationInfo notificationInfo) {
        MediaNotificationManager.setMultipleMediaNotificationsEnabled(
                ChromeFeatureList.isEnabled(ChromeFeatureList.ALLOW_MULTIPLE_MEDIA_NOTIFICATIONS));
        MediaNotificationManager.show(
                notificationInfo,
                (uniqueId, mediaTypeId) -> {
                    return new ChromeMediaNotificationControllerDelegate(uniqueId, mediaTypeId);
                });
    }
}
