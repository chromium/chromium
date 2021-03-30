// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.List;

/**
 * Provides access to the optimization guide using the C++ OptimizationGuideKeyedService.
 *
 * An instance of this class must be created, used, and destroyed on the UI thread.
 */
@JNINamespace("optimization_guide::android")
public class OptimizationGuideBridge {
    private long mNativeOptimizationGuideBridge;

    /**
     * Interface to implement to receive decisions from the optimization guide.
     */
    public interface OptimizationGuideCallback {
        void onOptimizationGuideDecision(
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
     * Invokes {@link callback} with the decision for the URL associated with {@link
     * navigationHandle} and {@link optimizationType} when sufficient information has been
     * collected to make a decision. This should only be called for main frame navigations.
     */
    public void canApplyOptimization(NavigationHandle navigationHandle,
            OptimizationType optimizationType, OptimizationGuideCallback callback) {
        assert navigationHandle.isInMainFrame();

        canApplyOptimization(navigationHandle.getUrl(), optimizationType, callback);
    }

    /**
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

    @CalledByNative
    private static void onOptimizationGuideDecision(OptimizationGuideCallback callback,
            @OptimizationGuideDecision int optimizationGuideDecision,
            @Nullable byte[] serializedAnyMetadata) {
        callback.onOptimizationGuideDecision(
                optimizationGuideDecision, deserializeAnyMetadata(serializedAnyMetadata));
    }

    @Nullable
    private static Any deserializeAnyMetadata(@Nullable byte[] serializedAnyMetadata) {
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
    }
}
