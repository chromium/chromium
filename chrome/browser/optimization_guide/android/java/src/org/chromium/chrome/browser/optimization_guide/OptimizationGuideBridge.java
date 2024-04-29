// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ThreadUtils;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.RequestContext;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.HintsProto.RequestContextMetadata;
import org.chromium.components.optimization_guide.proto.PushNotificationProto.HintNotificationPayload;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the optimization guide using the C++ OptimizationGuideKeyedService.
 *
 * <p>An instance of this class must be created, used, and destroyed on the UI thread.
 */
@JNINamespace("optimization_guide::android")
public class OptimizationGuideBridge {
    private long mNativeOptimizationGuideBridge;

    /** Interface to implement to receive decisions from the optimization guide. */
    public interface OptimizationGuideCallback {
        void onOptimizationGuideDecision(
                @OptimizationGuideDecision int decision, @Nullable Any metadata);
    }

    /** Interface to implement to receive on-demand decisions from the optimization guide. */
    public interface OnDemandOptimizationGuideCallback {
        void onOnDemandOptimizationGuideDecision(
                GURL url,
                OptimizationType optimizationType,
                @OptimizationGuideDecision int decision,
                @Nullable Any metadata);
    }

    @VisibleForTesting
    @CalledByNative
    protected OptimizationGuideBridge(long nativeOptimizationGuideBridge) {
        ThreadUtils.assertOnUiThread();

        mNativeOptimizationGuideBridge = nativeOptimizationGuideBridge;
    }

    /**
     * Registers the optimization types that intend to be queried during the session. It is expected
     * for this to be called after the browser has been initialized.
     */
    public void registerOptimizationTypes(@Nullable List<OptimizationType> optimizationTypes) {
        ThreadUtils.assertOnUiThread();
        if (mNativeOptimizationGuideBridge == 0) return;

        if (optimizationTypes == null) {
            optimizationTypes = new ArrayList<>();
        }
        int[] intOptimizationTypes = new int[optimizationTypes.size()];
        for (int i = 0; i < optimizationTypes.size(); i++) {
            intOptimizationTypes[i] = optimizationTypes.get(i).getNumber();
        }

        OptimizationGuideBridgeJni.get()
                .registerOptimizationTypes(mNativeOptimizationGuideBridge, intOptimizationTypes);
    }

    /**
     * Returns whether {@link optimizationType} can be applied for {@link url}. This should
     * only be called for main frame navigations or future main frame navigations.
     *
     * @param url main frame navigation URL an optimization decision is being made for.
     * @param optimizationType {@link OptimizationType} decision is being made for
     * @param callback {@link OptimizationGuideCallback} optimization decision is passed in
     */
    public void canApplyOptimization(
            GURL url, OptimizationType optimizationType, OptimizationGuideCallback callback) {
        ThreadUtils.assertOnUiThread();

        if (mNativeOptimizationGuideBridge == 0) {
            callback.onOptimizationGuideDecision(OptimizationGuideDecision.UNKNOWN, null);
            return;
        }

        OptimizationGuideBridgeJni.get()
                .canApplyOptimization(
                        mNativeOptimizationGuideBridge,
                        url,
                        optimizationType.getNumber(),
                        callback);
    }

    /**
     * Invokes {@link OnDemandOptimizationGuideCallback} with the decision for all types contained
     * in {@link optimizationTypes} for each URL contained in {@link urls}, when sufficient
     * information has been collected to make decisions. {@link requestContext} must be included to
     * indicate when the request is being made to determine the appropriate permissions to make the
     * request for accounting purposes.
     *
     * <p>It is expected for consumers to consult with the Optimization Guide team before using this
     * API. If approved, add your request context to the assertion list here.
     */
    public void canApplyOptimizationOnDemand(
            List<GURL> urls,
            List<OptimizationType> optimizationTypes,
            RequestContext requestContext,
            OnDemandOptimizationGuideCallback callback,
            RequestContextMetadata requestContextMetadata) {
        ThreadUtils.assertOnUiThread();

        assert isRequestContextAllowedForOnDemandOptimizations(requestContext);

        if (mNativeOptimizationGuideBridge == 0) {
            for (GURL url : urls) {
                for (OptimizationType optimizationType : optimizationTypes) {
                    callback.onOnDemandOptimizationGuideDecision(
                            url, optimizationType, OptimizationGuideDecision.UNKNOWN, null);
                }
            }
            return;
        }

        GURL[] gurlsArray = new GURL[urls.size()];
        urls.toArray(gurlsArray);
        int[] intOptimizationTypes = new int[optimizationTypes.size()];
        for (int i = 0; i < optimizationTypes.size(); i++) {
            intOptimizationTypes[i] = optimizationTypes.get(i).getNumber();
        }

        byte[] requestContextMetadataSerialized = requestContextMetadata.toByteArray();

        OptimizationGuideBridgeJni.get()
                .canApplyOptimizationOnDemand(
                        mNativeOptimizationGuideBridge,
                        gurlsArray,
                        intOptimizationTypes,
                        requestContext.getNumber(),
                        callback,
                        requestContextMetadataSerialized);
    }

    public void onNewPushNotification(HintNotificationPayload notification) {
        ThreadUtils.assertOnUiThread();
        if (mNativeOptimizationGuideBridge == 0) {
            OptimizationGuidePushNotificationManager.onPushNotificationNotHandledByNative(
                    notification);
            return;
        }
        OptimizationGuideBridgeJni.get()
                .onNewPushNotification(mNativeOptimizationGuideBridge, notification.toByteArray());
    }

    /**
     * Signal native OptimizationGuide that deferred startup has occurred. This enables
     * OptimizationGuide to fetch hints in the background while minimizing the risk of
     * regressing key performance metrics such as jank. This method should only be
     * called by ProcessInitializationHandler.
     */
    public void onDeferredStartup() {
        if (mNativeOptimizationGuideBridge == 0) {
            return;
        }
        OptimizationGuideBridgeJni.get().onDeferredStartup(mNativeOptimizationGuideBridge);
    }

    private boolean isRequestContextAllowedForOnDemandOptimizations(RequestContext requestContext) {
        switch (requestContext) {
            case CONTEXT_PAGE_INSIGHTS_HUB:
            case CONTEXT_NON_PERSONALIZED_PAGE_INSIGHTS_HUB:
                return true;
            default:
                return false;
        }
    }

    @CalledByNative
    private static void onOptimizationGuideDecision(
            OptimizationGuideCallback callback,
            @OptimizationGuideDecision int optimizationGuideDecision,
            @Nullable byte[] serializedAnyMetadata) {
        callback.onOptimizationGuideDecision(
                optimizationGuideDecision, deserializeAnyMetadata(serializedAnyMetadata));
    }

    @CalledByNative
    private static void onOnDemandOptimizationGuideDecision(
            OnDemandOptimizationGuideCallback callback,
            GURL url,
            int optimizationTypeInt,
            @OptimizationGuideDecision int optimizationGuideDecision,
            @Nullable byte[] serializedAnyMetadata) {
        OptimizationType optimizationType = OptimizationType.forNumber(optimizationTypeInt);
        if (optimizationType == null) return;
        callback.onOnDemandOptimizationGuideDecision(
                url,
                optimizationType,
                optimizationGuideDecision,
                deserializeAnyMetadata(serializedAnyMetadata));
    }

    /** Clears all cached push notifications for the given optimization type. */
    @CalledByNative
    private static void clearCachedPushNotifications(int optimizationTypeInt) {
        OptimizationType optimizationType = OptimizationType.forNumber(optimizationTypeInt);
        if (optimizationType == null) return;

        OptimizationGuidePushNotificationManager.clearCacheForOptimizationType(optimizationType);
    }

    /** Returns an array of all the optimization types that have cached push notifications. */
    @CalledByNative
    private static int[] getOptTypesWithPushNotifications() {
        List<OptimizationType> cachedTypes =
                OptimizationGuidePushNotificationManager.getOptTypesWithPushNotifications();
        int[] intCachedTypes = new int[cachedTypes.size()];
        for (int i = 0; i < cachedTypes.size(); i++) {
            intCachedTypes[i] = cachedTypes.get(i).getNumber();
        }
        return intCachedTypes;
    }

    /**
     * Returns an array of all the optimization types that overflowed their cache for push
     * notifications.
     */
    @CalledByNative
    private static int[] getOptTypesThatOverflowedPushNotifications() {
        List<OptimizationType> overflows =
                OptimizationGuidePushNotificationManager
                        .getOptTypesThatOverflowedPushNotifications();
        int[] intOverflows = new int[overflows.size()];
        for (int i = 0; i < overflows.size(); i++) {
            intOverflows[i] = overflows.get(i).getNumber();
        }
        return intOverflows;
    }

    /** Returns a 2D byte array of all cached push notifications for the given optimization type. */
    @CalledByNative
    private static byte[][] getEncodedPushNotifications(int optimizationTypeInt) {
        OptimizationType optimizationType = OptimizationType.forNumber(optimizationTypeInt);
        if (optimizationType == null) return null;

        HintNotificationPayload[] notifications =
                OptimizationGuidePushNotificationManager.getNotificationCacheForOptimizationType(
                        optimizationType);
        if (notifications == null) return null;

        byte[][] encoded_notifications = new byte[notifications.length][];
        for (int i = 0; i < notifications.length; i++) {
            encoded_notifications[i] = notifications[i].toByteArray();
        }

        return encoded_notifications;
    }

    /**
     * Called when a push notification that was passed to native immediately (without having been
     * cached) is unable to be stored right now, so it should be cached.
     */
    @CalledByNative
    private static void onPushNotificationNotHandledByNative(byte[] encodedNotification) {
        HintNotificationPayload notification;
        try {
            notification = HintNotificationPayload.parseFrom(encodedNotification);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            return;
        }
        OptimizationGuidePushNotificationManager.onPushNotificationNotHandledByNative(notification);
    }

    private static @Nullable Any deserializeAnyMetadata(@Nullable byte[] serializedAnyMetadata) {
        if (serializedAnyMetadata == null) {
            return null;
        }

        Any anyMetadata;
        try {
            anyMetadata = Any.parseFrom(serializedAnyMetadata);
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            return null;
        }
        return anyMetadata;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PACKAGE_PRIVATE)
    @NativeMethods
    public interface Natives {
        void registerOptimizationTypes(long nativeOptimizationGuideBridge, int[] optimizationTypes);

        void canApplyOptimization(
                long nativeOptimizationGuideBridge,
                @JniType("GURL") GURL url,
                int optimizationType,
                OptimizationGuideCallback callback);

        void canApplyOptimizationOnDemand(
                long nativeOptimizationGuideBridge,
                @JniType("std::vector<GURL>") GURL[] urls,
                int[] optimizationTypes,
                int requestContext,
                OnDemandOptimizationGuideCallback callback,
                @JniType("jni_zero::ByteArrayView") byte[] requestContextMetadata);

        void onNewPushNotification(long nativeOptimizationGuideBridge, byte[] encodedNotification);

        void onDeferredStartup(long nativeOptimizationGuideBridge);
    }
}
