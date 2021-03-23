// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.util.Arrays;

/**
 * Unit tests for OptimizationGuideBridge that call into native.
 */
public class OptimizationGuideBridgeNativeUnitTest {
    private static final String TEST_URL = "https://example.com/";

    private static class OptimizationGuideCallback
            implements OptimizationGuideBridge.OptimizationGuideCallback {
        private boolean mWasCalled;
        private @OptimizationGuideDecision int mDecision;
        private Any mMetadata;

        @Override
        public void onOptimizationGuideDecision(
                @OptimizationGuideDecision int decision, Any metadata) {
            mWasCalled = true;
            mDecision = decision;
            mMetadata = metadata;
        }

        public boolean wasCalled() {
            return mWasCalled;
        }

        public @OptimizationGuideDecision int getDecision() {
            return mDecision;
        }

        public Any getMetadata() {
            return mMetadata;
        }
    }

    @CalledByNative
    private OptimizationGuideBridgeNativeUnitTest() {}

    @CalledByNative
    public void testRegisterOptimizationTypes() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge();
        bridge.registerOptimizationTypes(Arrays.asList(new OptimizationType[] {
                OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT}));
    }

    @CalledByNative
    public void testCanApplyOptimizationPreInit() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge();

        NavigationHandle navHandle =
                new NavigationHandle(0, new GURL(TEST_URL), true, false, false);
        OptimizationGuideCallback callback = new OptimizationGuideCallback();
        bridge.canApplyOptimization(navHandle, OptimizationType.PERFORMANCE_HINTS, callback);

        assertTrue(callback.wasCalled());
        assertEquals(OptimizationGuideDecision.UNKNOWN, callback.getDecision());
        assertNull(callback.getMetadata());
    }

    @CalledByNative
    public void testCanApplyOptimizationHasHint() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge();

        NavigationHandle navHandle =
                new NavigationHandle(0, new GURL(TEST_URL), true, false, false);
        OptimizationGuideCallback callback = new OptimizationGuideCallback();
        bridge.canApplyOptimization(navHandle, OptimizationType.PERFORMANCE_HINTS, callback);

        assertTrue(callback.wasCalled());
        assertEquals(OptimizationGuideDecision.TRUE, callback.getDecision());
        assertNotNull(callback.getMetadata());
        assertEquals("optimization_guide.proto.PerformanceHintsMetadata",
                callback.getMetadata().getTypeUrl());
    }
}
