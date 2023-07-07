// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.RequestContext;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.PushNotificationProto.HintNotificationPayload;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the optimization guide using the C++ OptimizationGuideKeyedService.
 *
 * An instance of this class must be created, used, and destroyed on the UI thread.
 */
@JNINamespace("optimization_guide::android")
public class OptimizationGuideBridge implements Destroyable {
    private long mNativeOptimizationGuideBridge;

    /**
     * Interface to implement to receive decisions from the optimization guide.
     */
    public interface OptimizationGuideCallback {
        void onOptimizationGuideDecision(
                @OptimizationGuideDecision int decision, @Nullable Any metadata);
    }

    /**
     * Interface to implement to receive on-demand decisions from the optimization guide.
     */
    public interface OnDemandOptimizationGuideCallback {
        void onOnDemandOptimizationGuideDecision(GURL url, OptimizationType optimizationType,
                @OptimizationGuideDecision int decision, @Nullable Any metadata);
    }

    /**
     * Initializes the C++ side of this class, using the Optimization Guide Decider for the last
     * used Profile.
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PROTECTED)
    public OptimizationGuideBridge() {
        ThreadUtils.assertOnUiThread();

        mNativeOptimizationGuideBridge = OptimizationGuideBridgeJni.get().init();
    }

    @VisibleForTesting
    protected OptimizationGuideBridge(long nativeOptimizationGuideBridge) {
        mNativeOptimizationGuideBridge = nativeOptimizationGuideBridge;
    }

    /**
     * Deletes the C++ side of this class. This must be called when this object is no longer needed.
     */
    @Override
    public void destroy() {
        ThreadUtils.assertOnUiThread();

        if (mNativeOptimizationGuideBridge != 0) {
            OptimizationGuideBridgeJni.get().destroy(mNativeOptimizationGuideBridge);
            mNativeOptimizationGuideBridge = 0;
        }
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

        OptimizationGuideBridgeJni.get().registerOptimizationTypes(
                mNativeOptimizationGuideBridge, intOptimizationTypes);
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

        OptimizationGuideBridgeJni.get().canApplyOptimization(
                mNativeOptimizationGuideBridge, url, optimizationType.getNumber(), callback);
    }

    /**
     * Invokes {@link OnDemandOptimizationGuideCallback} with the decision for all types contained
     * in {@link optimizationTypes} for each URL contained in {@link urls}, when sufficient
     * information has been collected to make decisions. {@link requestContext} must be included to
     * indicate when the request is being made to determine the appropriate permissions to make the
     * request for accounting purposes.
     *
     * It is expected for consumers to consult with the Optimization Guide team before using this
     * API. If approved, add your request context to the assertion list here.
     */
    public void canApplyOptimizationOnDemand(List<GURL> urls,
            List<OptimizationType> optimizationTypes, RequestContext requestContext,
            OnDemandOptimizationGuideCallback callback) {
        ThreadUtils.assertOnUiThread();

        // TODO(b/279643150): Reconfigure this assertion once we have an actual client here.
        //
        // Currently, this is just for testing purposes to allow new tab page.
        assert requestContext == RequestContext.CONTEXT_NEW_TAB_PAGE;

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
        OptimizationGuideBridgeJni.get().canApplyOptimizationOnDemand(
                mNativeOptimizationGuideBridge, gurlsArray, intOptimizationTypes,
                requestContext.getNumber(), callback);
    }

    public void onNewPushNotification(HintNotificationPayload notification) {
        ThreadUtils.assertOnUiThread();
        if (mNativeOptimizationGuideBridge == 0) {
            OptimizationGuidePushNotificationManager.onPushNotificationNotHandledByNative(
                    notification);
            return;
        }
        OptimizationGuideBridgeJni.get().onNewPushNotification(
                mNativeOptimizationGuideBridge, notification.toByteArray());
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

    @CalledByNative
    private static void onOptimizationGuideDecision(OptimizationGuideCallback callback,
            @OptimizationGuideDecision int optimizationGuideDecision,
            @Nullable byte[] serializedAnyMetadata) {
        callback.onOptimizationGuideDecision(
                optimizationGuideDecision, deserializeAnyMetadata(serializedAnyMetadata));
    }

    @CalledByNative
    private static void onOnDemandOptimizationGuideDecision(
            OnDemandOptimizationGuideCallback callback, GURL url, int optimizationTypeInt,
            @OptimizationGuideDecision int optimizationGuideDecision,
            @Nullable byte[] serializedAnyMetadata) {
        OptimizationType optimizationType = OptimizationType.forNumber(optimizationTypeInt);
        if (optimizationType == null) return;
        callback.onOnDemandOptimizationGuideDecision(url, optimizationType,
                optimizationGuideDecision, deserializeAnyMetadata(serializedAnyMetadata));
    }

    /**
     * Clears all cached push notifications for the given optimization type.
     */
    @CalledByNative
    private static void clearCachedPushNotifications(int optimizationTypeInt) {
        OptimizationType optimizationType = OptimizationType.forNumber(optimizationTypeInt);
        if (optimizationType == null) return;

        OptimizationGuidePushNotificationManager.clearCacheForOptimizationType(optimizationType);
    }

    /**
     * Returns an array of all the optimization types that have cached push notifications.
     */
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
        List<OptimizationType> overflows = OptimizationGuidePushNotificationManager
                                                   .getOptTypesThatOverflowedPushNotifications();
        int[] intOverflows = new int[overflows.size()];
        for (int i = 0; i < overflows.size(); i++) {
            intOverflows[i] = overflows.get(i).getNumber();
        }
        return intOverflows;
    }

    /**
     * Returns a 2D byte array of all cached push notifications for the given optimization type.
     */
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
        long init();
        void destroy(long nativeOptimizationGuideBridge);
        void registerOptimizationTypes(long nativeOptimizationGuideBridge, int[] optimizationTypes);
        void canApplyOptimization(long nativeOptimizationGuideBridge, GURL url,
                int optimizationType, OptimizationGuideCallback callback);
        void canApplyOptimizationOnDemand(long nativeOptimizationGuideBridge, GURL[] urls,
                int[] optimizationTypes, int requestContext,
                OnDemandOptimizationGuideCallback callback);
        void onNewPushNotification(long nativeOptimizationGuideBridge, byte[] encodedNotification);
        void onDeferredStartup(long nativeOptimizationGuideBridge);
    }
}
