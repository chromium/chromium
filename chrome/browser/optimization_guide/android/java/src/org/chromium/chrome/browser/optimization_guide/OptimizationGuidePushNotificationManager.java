// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import android.util.Base64;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.ResettersForTesting;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.ChromePreferenceKeys;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.cached_flags.IntCachedFieldTrialParameter;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.PushNotificationProto.HintNotificationPayload;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Set;

/**
 * Manages incoming push notifications to the Optimization Guide. Each notification needs to be
 * forwarded to the native code, but they may arrive at any time. Notifications received while
 * native is down are persisted to prefs, up to an experimentally controlled limit. Such overflows
 * are detected and also forwarded to native.
 */
public class OptimizationGuidePushNotificationManager {
    private static Boolean sNativeIsInitialized;

    private static final String TAG = "OGPNotificationMngr";

    private static final String READ_CACHE_RESULT_HISTOGRAM =
            "OptimizationGuide.PushNotifications.ReadCacheResult";

    // Should be in sync with the enum "OptimizationGuideReadCacheResult" in
    // tools/metrics/histograms/enums.xml.
    @SuppressWarnings("unused")
    @IntDef({
        ReadCacheResult.UNKNOWN,
        ReadCacheResult.SUCCESS,
        ReadCacheResult.INVALID_PROTO_ERROR,
        ReadCacheResult.BASE64_ERROR
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface ReadCacheResult {
        int UNKNOWN = 0;
        int SUCCESS = 1;
        int INVALID_PROTO_ERROR = 2;
        int BASE64_ERROR = 3;

        int NUM_ENTRIES = 4;
    }

    // All logic here is static, so no instances of this class are needed.
    private OptimizationGuidePushNotificationManager() {}

    /** A sentinel Set that is set when the pref for a specific OptimizationType overflows. */
    private static final Set<String> OVERFLOW_SENTINEL_SET = Set.of("__overflow");

    /** The default cache size in Java for push notification. */
    public static final IntCachedFieldTrialParameter MAX_CACHE_SIZE =
            ChromeFeatureList.newIntCachedFieldTrialParameter(
                    ChromeFeatureList.OPTIMIZATION_GUIDE_PUSH_NOTIFICATIONS, "max_cache_size", 100);

    /**
     * Called when a new push notification is received.
     * @param payload the incoming payload.
     */
    public static void onPushNotification(HintNotificationPayload payload) {
        if (!ChromeFeatureList.sOptimizationGuidePushNotifications.isEnabled()) {
            // In case the feature has become disabled after once being enabled, clear everything.
            clearCacheForAllTypes();
            return;
        }

        if (nativeIsInitialized()) {
            OptimizationGuideBridgeFactory.getForProfile(ProfileManager.getLastUsedRegularProfile())
                    .onNewPushNotification(payload);
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
        ChromeSharedPreferences.getInstance().removeKey(cacheKey(optimizationType));
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    public static void clearCacheForAllTypes() {
        for (OptimizationType type : OptimizationType.values()) {
            clearCacheForOptimizationType(type);
        }
    }

    /**
     * Returns all cached notifications for the given Optimization Type. A null return value is
     * returned for overflowed caches. An empty array means that no notifications were cached.
     * @param optimizationType the optimization type to get cached notifications for
     * @return a possibly null array of persisted notifications
     */
    @Nullable
    public static HintNotificationPayload[] getNotificationCacheForOptimizationType(
            OptimizationType optimizationType) {
        Set<String> cache = getStringCacheForOptimizationType(optimizationType);
        if (checkForOverflow(cache)) return null;

        Iterator<String> cache_iter = cache.iterator();

        List<HintNotificationPayload> notifications = new ArrayList<HintNotificationPayload>();
        for (int i = 0; i < cache.size(); i++) {
            try {
                HintNotificationPayload payload =
                        HintNotificationPayload.parseFrom(
                                Base64.decode(cache_iter.next(), Base64.DEFAULT));
                notifications.add(payload);
                RecordHistogram.recordEnumeratedHistogram(
                        READ_CACHE_RESULT_HISTOGRAM,
                        ReadCacheResult.SUCCESS,
                        ReadCacheResult.NUM_ENTRIES);
            } catch (com.google.protobuf.InvalidProtocolBufferException e) {
                RecordHistogram.recordEnumeratedHistogram(
                        READ_CACHE_RESULT_HISTOGRAM,
                        ReadCacheResult.INVALID_PROTO_ERROR,
                        ReadCacheResult.NUM_ENTRIES);
                Log.e(TAG, Log.getStackTraceString(e));
            } catch (IllegalArgumentException e) {
                RecordHistogram.recordEnumeratedHistogram(
                        READ_CACHE_RESULT_HISTOGRAM,
                        ReadCacheResult.BASE64_ERROR,
                        ReadCacheResult.NUM_ENTRIES);
                Log.e(TAG, Log.getStackTraceString(e));
            }
        }

        HintNotificationPayload[] notificationsArray =
                new HintNotificationPayload[notifications.size()];
        notifications.toArray(notificationsArray);
        return notificationsArray;
    }

    private static Set<String> getStringCacheForOptimizationType(
            OptimizationType optimizationType) {
        return ChromeSharedPreferences.getInstance().readStringSet(cacheKey(optimizationType));
    }

    /**
     * Returns a list of all the optimization types that have push notification cached. Optimization
     * types with overflowed caches are not included.
     */
    public static List<OptimizationType> getOptTypesWithPushNotifications() {
        List<OptimizationType> types = new ArrayList<OptimizationType>();
        for (OptimizationType type : OptimizationType.values()) {
            Set<String> cache = ChromeSharedPreferences.getInstance().readStringSet(cacheKey(type));
            if (cache != null && cache.size() > 0 && !checkForOverflow(cache)) {
                types.add(type);
            }
        }
        return types;
    }

    /**
     * Returns a list of all the optimization types that overflowed their push notification caches.
     */
    public static List<OptimizationType> getOptTypesThatOverflowedPushNotifications() {
        List<OptimizationType> overflows = new ArrayList<OptimizationType>();
        for (OptimizationType type : OptimizationType.values()) {
            if (checkForOverflow(getStringCacheForOptimizationType(type))) {
                overflows.add(type);
            }
        }
        return overflows;
    }

    @VisibleForTesting
    public static String cacheKey(OptimizationType optimizationType) {
        return ChromePreferenceKeys.OPTIMIZATION_GUIDE_PUSH_NOTIFICATION_CACHE.createKey(
                optimizationType.name());
    }

    public static void setNativeIsInitializedForTesting(Boolean nativeIsInitialized) {
        var oldValue = sNativeIsInitialized;
        sNativeIsInitialized = nativeIsInitialized;
        ResettersForTesting.register(() -> sNativeIsInitialized = oldValue);
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
            ChromeSharedPreferences.getInstance()
                    .writeStringSet(cacheKey(payload.getOptimizationType()), OVERFLOW_SENTINEL_SET);
            return;
        }

        // The notification's payload isn't used so it can be stripped to preserve memory space.
        HintNotificationPayload slim_payload =
                HintNotificationPayload.newBuilder(payload).clearPayload().build();
        ChromeSharedPreferences.getInstance()
                .addToStringSet(
                        cacheKey(slim_payload.getOptimizationType()),
                        Base64.encodeToString(slim_payload.toByteArray(), Base64.DEFAULT));
    }
}
