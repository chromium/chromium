// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import org.jni_zero.CalledByNative;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.PushNotificationProto.HintNotificationPayload;

/** Unit test helper for OptimizationGuidePushNotificationManager. */
public class OptimizationGuidePushNotificationTestHelper {
    @Mock private Profile mProfile;

    @CalledByNative
    private OptimizationGuidePushNotificationTestHelper() {}

    @CalledByNative
    public void setUpMocks() {
        MockitoAnnotations.initMocks(this);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
    }

    @CalledByNative
    public static boolean cacheNotification(byte[] encodedNotification) {
        HintNotificationPayload notification;
        try {
            notification = HintNotificationPayload.parseFrom(encodedNotification);
            OptimizationGuidePushNotificationManager.onPushNotificationNotHandledByNative(
                    notification);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            return false;
        }

        return true;
    }

    @CalledByNative
    public static int countCachedNotifications(int optType) {
        HintNotificationPayload[] payloads =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        OptimizationType.forNumber(optType));
        return payloads == null ? 0 : payloads.length;
    }

    @CalledByNative
    public static boolean didOverflow(int optType) {
        for (OptimizationType type :
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications()) {
            if (type.getNumber() == optType) {
                return true;
            }
        }
        return false;
    }

    @CalledByNative
    public static void setOverflowSizeForTesting(int size) {
        OptimizationGuidePushNotificationManager.MAX_CACHE_SIZE.setForTesting(size);
    }

    @CalledByNative
    public static void clearAllCaches() {
        OptimizationGuidePushNotificationManager.clearCacheForAllTypes();
    }

    @CalledByNative
    public static void setFeatureEnabled() {
        ChromeFeatureList.sOptimizationGuidePushNotifications.setForTesting(true);
    }

    @CalledByNative
    public static boolean pushNotification(byte[] encodedNotification) {
        HintNotificationPayload notification;
        try {
            notification = HintNotificationPayload.parseFrom(encodedNotification);
            OptimizationGuidePushNotificationManager.onPushNotification(notification);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            return false;
        }

        return true;
    }
}
