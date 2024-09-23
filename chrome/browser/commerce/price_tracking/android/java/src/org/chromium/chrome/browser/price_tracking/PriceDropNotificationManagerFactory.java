// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;

/** A factory class to create a {@link PriceDropNotificationManager}. */
public class PriceDropNotificationManagerFactory {
    private static PriceDropNotificationManager sTestingInstance;

    public static void setInstanceForTesting(PriceDropNotificationManager testInstance) {
        sTestingInstance = testInstance;
    }

    /**
     * Builds a {@link PriceDropNotificationManager} instance.
     *
     * @param profile The {@link Profile} associated with the price drops.
     */
    public static PriceDropNotificationManager create(Profile profile) {
        if (sTestingInstance != null) {
            return sTestingInstance;
        }
        return new PriceDropNotificationManagerImpl(
                ContextUtils.getApplicationContext(),
                profile,
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext()));
    }

    /**
     * Builds a {@link PriceDropNotificationManager} instance.
     *
     * @param context The application context.
     * @param profile The {@link Profile} associated with the price drops.
     * @param notificationManagerProxy The {@link NotificationManagerProxy} for sending
     *     notifications.
     * @return The instance of {@link PriceDropNotificationManager}.
     */
    public static PriceDropNotificationManager create(
            Context context, Profile profile, NotificationManagerProxy notificationManagerProxy) {
        return new PriceDropNotificationManagerImpl(context, profile, notificationManagerProxy);
    }
}
