// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.omaha.notification;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.ChromeFeatureList;

/**
 * A factory that creates an {@link UpdateNotificationController} instance.
 */
public class UpdateNotificationControllerFactory {
    /**
     * @param activity A {@link ChromeActivity} instance the notification will be shown in.
     * @return a new {@link UpdateNotificationController} to use.
     */
    public static UpdateNotificationController create(ChromeActivity activity) {
        if (ChromeFeatureList.isEnabled(
                    ChromeFeatureList.UPDATE_NOTIFICATION_SCHEDULING_INTEGRATION)) {
            return new UpdateNotificationScheduleCoordinator(activity);
        }
        return new UpdateNotificationControllerImpl(activity);
    }
}
