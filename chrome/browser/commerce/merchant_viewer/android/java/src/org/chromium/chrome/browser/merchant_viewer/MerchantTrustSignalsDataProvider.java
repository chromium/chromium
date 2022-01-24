// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import android.text.TextUtils;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignalsV2;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.io.IOException;
import java.util.Arrays;

/**
 * Merchant trust data provider using {@link OptimizationGuideBridge}.
 */
class MerchantTrustSignalsDataProvider {
    private static final String TAG = "MTDP";
    private static final OptimizationGuideBridgeFactory sOptimizationGuideBridgeFactory =
            new OptimizationGuideBridgeFactory(
                    Arrays.asList(HintsProto.OptimizationType.MERCHANT_TRUST_SIGNALS_V2));

    /**
     * Fetches {@link MerchantTrustSignalsV2} through {@link OptimizationGuideBridge} based on
     * {@link NavigationHandle}.
     */
    public void getDataForNavigationHandle(
            NavigationHandle navigationHandle, Callback<MerchantTrustSignalsV2> callback) {
        sOptimizationGuideBridgeFactory.create().canApplyOptimizationAsync(navigationHandle,
                HintsProto.OptimizationType.MERCHANT_TRUST_SIGNALS_V2, (decision, metadata) -> {
                    onOptimizationGuideDecision(decision, metadata, callback);
                });
    }

    /**
     * Fetches {@link MerchantTrustSignalsV2} through {@link OptimizationGuideBridge} based on
     * {@link GURL}.
     */
    public void getDataForUrl(GURL url, Callback<MerchantTrustSignalsV2> callback) {
        sOptimizationGuideBridgeFactory.create().canApplyOptimization(url,
                HintsProto.OptimizationType.MERCHANT_TRUST_SIGNALS_V2, (decision, metadata) -> {
                    onOptimizationGuideDecision(decision, metadata, callback);
                });
    }

    private void onOptimizationGuideDecision(@OptimizationGuideDecision int decision,
            @Nullable Any metadata, Callback<MerchantTrustSignalsV2> callback) {
        if (decision != OptimizationGuideDecision.TRUE || metadata == null) {
            callback.onResult(null);
            return;
        }
        try {
            MerchantTrustSignalsV2 trustSignals =
                    MerchantTrustSignalsV2.parseFrom(metadata.getValue());
            callback.onResult(isValidMerchantTrustSignals(trustSignals) ? trustSignals : null);
        } catch (IOException e) {
            // Catching Exception instead of InvalidProtocolBufferException in order to
            // avoid increasing the apk size by taking a dependency on protobuf lib.
            Log.i(TAG, "There was a problem parsing MerchantTrustSignals." + e.getMessage());
            callback.onResult(null);
        }
    }

    @VisibleForTesting
    boolean isValidMerchantTrustSignals(MerchantTrustSignalsV2 trustSignals) {
        return (!TextUtils.isEmpty(trustSignals.getMerchantDetailsPageUrl()))
                && (!trustSignals.getContainsSensitiveContent())
                && (trustSignals.getMerchantStarRating() > 0 || trustSignals.getHasReturnPolicy());
    }
}
