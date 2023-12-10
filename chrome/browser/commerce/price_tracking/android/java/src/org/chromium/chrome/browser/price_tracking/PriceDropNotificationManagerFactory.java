// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import android.content.Context;

import org.chromium.base.ContextUtils;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxy;
import org.chromium.components.browser_ui.notifications.NotificationManagerProxyImpl;

/** A factory class to create a {@link PriceDropNotificationManager}. */
public class PriceDropNotificationManagerFactory {
    /** Builds a {@link PriceDropNotificationManager} instance. */
    public static PriceDropNotificationManager create() {
        return new PriceDropNotificationManagerImpl(
                ContextUtils.getApplicationContext(),
                new NotificationManagerProxyImpl(ContextUtils.getApplicationContext()));
    }

    /**
     * Builds a {@link PriceDropNotificationManager} instance.
     * @param context The application context.
     * @param notificationManagerProxy The {@link NotificationManagerProxy} for sending
     *         notifications.
     * @return The instance of {@link PriceDropNotificationManager}.
     */
    public static PriceDropNotificationManager create(
            Context context, NotificationManagerProxy notificationManagerProxy) {
        return new PriceDropNotificationManagerImpl(context, notificationManagerProxy);
    }
}
