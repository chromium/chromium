// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.ui;

import org.chromium.components.browser_ui.media.MediaNotificationInfo;
import org.chromium.components.browser_ui.media.MediaNotificationManager;

/** Thin wrapper for {@link MediaNotificationManager}. */
public class ChromeMediaNotificationManager {
    /**
     * Shows a media notification. Passes through to {@link MediaNotificationManager}, utilizing a
     * Chrome specific delegate.
     *
     * @param notificationInfo information to show in the notification
     */
    public static void show(MediaNotificationInfo notificationInfo) {
        MediaNotificationManager.show(
                notificationInfo,
                () -> {
                    return new ChromeMediaNotificationControllerDelegate(notificationInfo.id);
                });
    }
}
