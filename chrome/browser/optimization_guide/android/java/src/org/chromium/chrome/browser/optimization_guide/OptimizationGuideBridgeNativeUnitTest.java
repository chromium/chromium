// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.jni_zero.CalledByNative;

import org.chromium.base.test.util.Batch;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.RequestContext;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.HintsProto.RequestContextMetadata;
import org.chromium.url.GURL;

import java.util.Arrays;
import java.util.HashMap;
import java.util.Map;

/** Unit tests for OptimizationGuideBridge that call into native. */
@Batch(Batch.UNIT_TESTS)
public class OptimizationGuideBridgeNativeUnitTest {
    private static final String TEST_URL = "https://example.com/";
    private static final String TEST_URL2 = "https://example2.com/";

    private static class OptimizationGuideDecisionWithMetadata {
        private @OptimizationGuideDecision int mDecision;
        private Any mMetadata;

        public OptimizationGuideDecisionWithMetadata(
                @OptimizationGuideDecision int decision, Any metadata) {
            mDecision = decision;
            mMetadata = metadata;
        }

        public @OptimizationGuideDecision int getDecision() {
            return mDecision;
        }

        public Any getMetadata() {
            return mMetadata;
        }
    }

    private static class OptimizationGuideCallback
            implements OptimizationGuideBridge.OptimizationGuideCallback {
        private boolean mWasCalled;
        private OptimizationGuideDecisionWithMetadata mDecisionMetadata;

        @Override
        public void onOptimizationGuideDecision(
                @OptimizationGuideDecision int decision, Any metadata) {
            mWasCalled = true;
            mDecisionMetadata = new OptimizationGuideDecisionWithMetadata(decision, metadata);
        }

        public boolean wasCalled() {
            return mWasCalled;
        }

        public OptimizationGuideDecisionWithMetadata getDecisionMetadata() {
            return mDecisionMetadata;
        }
    }

    private static class OnDemandOptimizationGuideCallback
            implements OptimizationGuideBridge.OnDemandOptimizationGuideCallback {
        private Map<GURL, Map<OptimizationType, OptimizationGuideDecisionWithMetadata>> mDecisions =
                new HashMap<>();

        @Override
        public void onOnDemandOptimizationGuideDecision(
                GURL url,
                OptimizationType optimizationType,
                @OptimizationGuideDecision int decision,
                Any metadata) {
            mDecisions.putIfAbsent(url, new HashMap<>());
            mDecisions
                    .get(url)
                    .put(
                            optimizationType,
                            new OptimizationGuideDecisionWithMetadata(decision, metadata));
        }

        public Map<OptimizationType, OptimizationGuideDecisionWithMetadata>
                getDecisionMetadataForURL(GURL url) {
            return mDecisions.get(url);
        }
    }

    private final OptimizationGuideBridge mOptimizationGuideBridge;

    @CalledByNative
    private OptimizationGuideBridgeNativeUnitTest(OptimizationGuideBridge optimizationGuideBridge) {
        mOptimizationGuideBridge = optimizationGuideBridge;
    }

    @CalledByNative
    public void testRegisterOptimizationTypes() {
        mOptimizationGuideBridge.registerOptimizationTypes(
                Arrays.asList(
                        new OptimizationType[] {
                            OptimizationType.LOADING_PREDICTOR, OptimizationType.DEFER_ALL_SCRIPT
                        }));
    }

    @CalledByNative
    public void testCanApplyOptimizationHasHint() {
        OptimizationGuideCallback callback = new OptimizationGuideCallback();
        mOptimizationGuideBridge.canApplyOptimization(
                new GURL(TEST_URL), OptimizationType.LOADING_PREDICTOR, callback);

        assertTrue(callback.wasCalled());
        OptimizationGuideDecisionWithMetadata decisionMetadata = callback.getDecisionMetadata();
        assertNotNull(decisionMetadata);
        assertEquals(OptimizationGuideDecision.TRUE, decisionMetadata.getDecision());
        assertNotNull(decisionMetadata.getMetadata());
        assertEquals(
                "optimization_guide.proto.LoadingPredictorMetadata",
                decisionMetadata.getMetadata().getTypeUrl());
    }

    @CalledByNative
    public void testCanApplyOptimizationOnDemand() {
        RequestContextMetadata requestContextMetadata = RequestContextMetadata.newBuilder().build();

        OnDemandOptimizationGuideCallback callback = new OnDemandOptimizationGuideCallback();
        mOptimizationGuideBridge.canApplyOptimizationOnDemand(
                Arrays.asList(new GURL[] {new GURL(TEST_URL), new GURL(TEST_URL2)}),
                Arrays.asList(
                        new OptimizationType[] {
                            OptimizationType.LOADING_PREDICTOR, OptimizationType.DEFER_ALL_SCRIPT
                        }),
                RequestContext.CONTEXT_PAGE_INSIGHTS_HUB,
                callback,
                requestContextMetadata);

        Map<OptimizationType, OptimizationGuideDecisionWithMetadata> test_url_metadata =
                callback.getDecisionMetadataForURL(new GURL(TEST_URL));
        assertNotNull(test_url_metadata);
        OptimizationGuideDecisionWithMetadata test_url_lp_metadata =
                test_url_metadata.get(OptimizationType.LOADING_PREDICTOR);
        assertNotNull(test_url_lp_metadata);
        assertEquals(OptimizationGuideDecision.TRUE, test_url_lp_metadata.getDecision());
        assertNotNull(test_url_lp_metadata.getMetadata());
        assertEquals(
                "optimization_guide.proto.LoadingPredictorMetadata",
                test_url_lp_metadata.getMetadata().getTypeUrl());
        OptimizationGuideDecisionWithMetadata test_url_ds_metadata =
                test_url_metadata.get(OptimizationType.DEFER_ALL_SCRIPT);
        assertNotNull(test_url_ds_metadata);
        assertEquals(OptimizationGuideDecision.FALSE, test_url_ds_metadata.getDecision());
        assertNull(test_url_ds_metadata.getMetadata());

        Map<OptimizationType, OptimizationGuideDecisionWithMetadata> test_url2_metadata =
                callback.getDecisionMetadataForURL(new GURL(TEST_URL2));
        assertNotNull(test_url2_metadata);
        OptimizationGuideDecisionWithMetadata test_url2_lp_metadata =
                test_url2_metadata.get(OptimizationType.LOADING_PREDICTOR);
        assertNotNull(test_url2_lp_metadata);
        assertEquals(OptimizationGuideDecision.FALSE, test_url2_lp_metadata.getDecision());
        assertNull(test_url2_lp_metadata.getMetadata());
        OptimizationGuideDecisionWithMetadata test_url2_ds_metadata =
                test_url2_metadata.get(OptimizationType.DEFER_ALL_SCRIPT);
        assertNotNull(test_url2_ds_metadata);
        assertEquals(OptimizationGuideDecision.TRUE, test_url2_ds_metadata.getDecision());
        assertNotNull(test_url2_ds_metadata.getMetadata());
        assertEquals(
                "optimization_guide.proto.StringValue",
                test_url2_ds_metadata.getMetadata().getTypeUrl());
    }
}
