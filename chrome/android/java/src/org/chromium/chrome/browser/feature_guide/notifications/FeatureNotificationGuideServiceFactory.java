// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_guide.notifications;

import org.jni_zero.NativeMethods;

import org.chromium.chrome.browser.profiles.Profile;

/**
 * Basic factory that creates and returns an {@link FeatureNotificationGuideService} that is
 * attached natively to the given {@link Profile}.
 */
public final class FeatureNotificationGuideServiceFactory {
    private static FeatureNotificationGuideService sFeatureNotificationGuideServiceForTesting;

    private FeatureNotificationGuideServiceFactory() {}

    public static FeatureNotificationGuideService getForProfile(Profile profile) {
        if (sFeatureNotificationGuideServiceForTesting != null) {
            return sFeatureNotificationGuideServiceForTesting;
        }
        return FeatureNotificationGuideServiceFactoryJni.get().getForProfile(profile);
    }

    @NativeMethods
    interface Natives {
        FeatureNotificationGuideService getForProfile(Profile profile);
    }
}
