// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.price_tracking;

import org.chromium.chrome.browser.profiles.Profile;

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
        return new PriceDropNotificationManagerImpl(profile);
    }
}
