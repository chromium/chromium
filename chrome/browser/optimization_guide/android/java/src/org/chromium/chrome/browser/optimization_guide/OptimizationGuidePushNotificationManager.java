// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import android.util.Base64;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.IntCachedFieldTrialParameter;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.SharedPreferencesManager;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.PushNotificationProto.HintNotificationPayload;

import java.util.Iterator;
import java.util.Set;

/**
 * Manages incoming push notifications to the Optimization Guide. Each notification needs to be
 * forwarded to the native code, but they may arrive at any time. Notifications received while
 * native is down are persisted to prefs, up to an experimentally controlled limit. Such overflows
 * are detected and also forwarded to native.
 */
public class OptimizationGuidePushNotificationManager {
    private static Boolean sNativeIsInitialized;
    private static OptimizationGuideBridgeFactory sBridgeFactory;

    // All logic here is static, so no instances of this class are needed.
    private OptimizationGuidePushNotificationManager() {}

    /** A sentinel Set that is set when the pref for a specific OptimizationType overflows. */
    private static final Set<String> OVERFLOW_SENTINEL_SET = Set.of("__overflow");

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static final IntCachedFieldTrialParameter MAX_CACHE_SIZE =
            new IntCachedFieldTrialParameter(
                    ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, "max_cache_size", 100);

    /**
     * Called when a new push notification is received.
     * @param payload the incoming payload.
     */
    public static void onPushNotification(HintNotificationPayload payload) {
        if (!CachedFeatureFlags.isEnabled(
                    ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS)) {
            // In case the feature has become disabled after once being enabled, clear everything.
            clearCacheForAllTypes();
            return;
        }

        if (nativeIsInitialized()) {
            if (sBridgeFactory == null) {
                sBridgeFactory = new OptimizationGuideBridgeFactory();
            }
            sBridgeFactory.create().onNewPushNotification(payload);
            return;
        }

        // Persist the notification until the next time native is awake.
        persistNotificationPayload(payload);
    }

    /**
     * Called when the native code isn't able to handle a push notification that was previously
     * sent. The given notification will be cached instead, until the next time that the cached
     * notifications are requested.
     */
    public static void onPushNotificationNotHandledByNative(HintNotificationPayload payload) {
        persistNotificationPayload(payload);
    }

    /**
     * Clears the cache for the given optimization type.
     * @param optimizationType the optimization type to clear
     */
    public static void clearCacheForOptimizationType(OptimizationType optimizationType) {
        SharedPreferencesManager.getInstance().removeKey(cacheKey(optimizationType));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void clearCacheForAllTypes() {
        for (OptimizationType type : OptimizationType.values()) {
            clearCacheForOptimizationType(type);
        }
    }

    /**
     * Returns all cached notifications for the given Optimization Type. A null return value is
     * returned for overflowed caches. An empty array means that no notifications were cached. Care
     * should be taken when iterating through elements of the returned array since null elements may
     * be present when a persisted value was not parsed successfully.
     * @param optimizationType the optimization type to get cached notifications for
     * @return a possibly null array of persisted notifications
     */
    @Nullable
    public static HintNotificationPayload[] getNotificationCacheForOptimizationType(
            OptimizationType optimizationType) {
        Set<String> cache = getStringCacheForOptimizationType(optimizationType);
        if (checkForOverflow(cache)) return null;

        Iterator<String> cache_iter = cache.iterator();

        HintNotificationPayload[] notifications = new HintNotificationPayload[cache.size()];
        for (int i = 0; i < notifications.length; i++) {
            HintNotificationPayload payload = null;
            try {
                payload = HintNotificationPayload.parseFrom(
                        Base64.decode(cache_iter.next(), Base64.DEFAULT));
            } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            } catch (IllegalArgumentException e) {
                // TODO(crbug/1199123): Add metrics for both exceptions to track if they actually
                // occur in the wild.
            }
            notifications[i] = payload;
        }

        return notifications;
    }

    private static Set<String> getStringCacheForOptimizationType(
            OptimizationType optimizationType) {
        return SharedPreferencesManager.getInstance().readStringSet(cacheKey(optimizationType));
    }

    /**
     * Signals whether the cached notifications for the given optimization type overflowed.
     * @param optimizationType the optimization to check for cache overflow
     * @return true if the cache overflowed.
     */
    public static boolean didNotificationCacheOverflowForOptimizationType(
            OptimizationType optimizationType) {
        return checkForOverflow(getStringCacheForOptimizationType(optimizationType));
    }

    private static String cacheKey(OptimizationType optimizationType) {
        return ChromePreferenceKeys.OPTIMIZATION_GUIDE_PUSH_NOTIFICATION_CACHE.createKey(
                optimizationType.toString());
    }

    @VisibleForTesting
    public static void setNativeIsInitializedForTesting(Boolean nativeIsInitialized) {
        sNativeIsInitialized = nativeIsInitialized;
    }

    private static boolean nativeIsInitialized() {
        if (sNativeIsInitialized != null) return sNativeIsInitialized;
        return LibraryLoader.getInstance().isInitialized();
    }

    private static boolean checkForOverflow(Set<String> cache) {
        return cache != null && cache.equals(OVERFLOW_SENTINEL_SET);
    }

    private static void persistNotificationPayload(HintNotificationPayload payload) {
        if (!payload.hasOptimizationType()) return;
        if (!payload.hasKeyRepresentation()) return;
        if (!payload.hasHintKey()) return;

        Set<String> cache = getStringCacheForOptimizationType(payload.getOptimizationType());

        // Check if the cache is already overflowed, in which case we should do nothing.
        if (checkForOverflow(cache)) return;

        // Check if we would overflow the cache by writing the new element.
        if (cache.size() >= MAX_CACHE_SIZE.getValue() - 1) {
            SharedPreferencesManager.getInstance().writeStringSet(
                    cacheKey(payload.getOptimizationType()), OVERFLOW_SENTINEL_SET);
            return;
        }

        // The notification's payload isn't used so it can be stripped to preserve memory space.
        HintNotificationPayload slim_payload =
                HintNotificationPayload.newBuilder(payload).clearPayload().build();
        SharedPreferencesManager.getInstance().addToStringSet(
                cacheKey(slim_payload.getOptimizationType()),
                Base64.encodeToString(slim_payload.toByteArray(), Base64.DEFAULT));
    }
}
