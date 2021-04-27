// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.merchant_viewer;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeFactory;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto;
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
                    Arrays.asList(HintsProto.OptimizationType.MERCHANT_TRUST_SIGNALS));

    /** Fetches {@link MerchantTrustSignals} through {@link OptimizationGuideBridge}. */
    public void getDataForUrl(GURL url, Callback<MerchantTrustSignals> callback) {
        sOptimizationGuideBridgeFactory.create().canApplyOptimization(
                url, HintsProto.OptimizationType.MERCHANT_TRUST_SIGNALS, (decision, metadata) -> {
                    if (decision != OptimizationGuideDecision.TRUE || metadata == null) {
                        callback.onResult(null);
                        return;
                    }
                    try {
                        MerchantTrustSignals trustSignals =
                                MerchantTrustSignals.parseFrom(metadata.getValue());

                        callback.onResult(
                                isValidMerchantTrustSignals(trustSignals) ? trustSignals : null);
                    } catch (IOException e) {
                        // Catching Exception instead of InvalidProtocolBufferException in order to
                        // avoid increasing the apk size by taking a dependency on protobuf lib.
                        Log.i(TAG,
                                "There was a problem parsing MerchantTrustSignals."
                                        + e.getMessage());
                        callback.onResult(null);
                    }
                });
    }

    private boolean isValidMerchantTrustSignals(MerchantTrustSignals trustSignals) {
        return trustSignals.hasMerchantCountRating() && trustSignals.hasMerchantStarRating()
                && trustSignals.hasMerchantDetailsPageUrl();
    }
}