// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.RequestContext;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.components.optimization_guide.proto.HintsProto.RequestContextMetadata;
import org.chromium.url.GURL;

import java.util.Arrays;

/** Unit tests for OptimizationGuideBridge. */
// TODO(kamalchoudhury): Include requestContextMetadata when Logic in production code is completed
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class OptimizationGuideBridgeUnitTest {
    private static final String TEST_URL = "https://testurl.com/";
    private static final String TEST_URL2 = "https://testurl2.com/";

    @Rule public JniMocker mocker = new JniMocker();

    @Mock OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Mock OptimizationGuideBridge.OptimizationGuideCallback mCallbackMock;

    @Mock OptimizationGuideBridge.OnDemandOptimizationGuideCallback mOnDemandCallbackMock;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mOptimizationGuideBridgeJniMock);
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testRegisterOptimizationTypes() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);
        bridge.registerOptimizationTypes(
                Arrays.asList(
                        new OptimizationType[] {
                            OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT
                        }));
        verify(mOptimizationGuideBridgeJniMock, times(1))
                .registerOptimizationTypes(
                        eq(1L),
                        aryEq(
                                new int[] {
                                    OptimizationType.PERFORMANCE_HINTS_VALUE,
                                    OptimizationType.DEFER_ALL_SCRIPT_VALUE
                                }));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testRegisterOptimizationTypes_withoutNativeBridge() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(0);
        bridge.registerOptimizationTypes(
                Arrays.asList(
                        new OptimizationType[] {
                            OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT
                        }));
        verify(mOptimizationGuideBridgeJniMock, never())
                .registerOptimizationTypes(anyLong(), any(int[].class));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testRegisterOptimizationTypes_noOptimizationTypes() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);
        bridge.registerOptimizationTypes(null);
        verify(mOptimizationGuideBridgeJniMock, times(1))
                .registerOptimizationTypes(eq(1L), aryEq(new int[0]));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testCanApplyOptimization_withoutNativeBridge() {
        GURL gurl = new GURL(TEST_URL);
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(0);

        bridge.canApplyOptimization(gurl, OptimizationType.PERFORMANCE_HINTS, mCallbackMock);

        verify(mOptimizationGuideBridgeJniMock, never())
                .canApplyOptimization(
                        anyLong(),
                        any(),
                        anyInt(),
                        any(OptimizationGuideBridge.OptimizationGuideCallback.class));
        verify(mCallbackMock)
                .onOptimizationGuideDecision(eq(OptimizationGuideDecision.UNKNOWN), isNull());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testCanApplyOptimization() {
        GURL gurl = new GURL(TEST_URL);
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);

        bridge.canApplyOptimization(gurl, OptimizationType.PERFORMANCE_HINTS, mCallbackMock);

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimization(
                        eq(1L),
                        eq(gurl),
                        eq(OptimizationType.PERFORMANCE_HINTS_VALUE),
                        eq(mCallbackMock));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testCanApplyOptimizationOnDemand_withoutNativeBridge() {
        GURL gurl = new GURL(TEST_URL);
        GURL gurl2 = new GURL(TEST_URL2);
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(0);

        bridge.canApplyOptimizationOnDemand(
                Arrays.asList(new GURL[] {gurl, gurl2}),
                Arrays.asList(
                        new OptimizationType[] {
                            OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT
                        }),
                RequestContext.CONTEXT_PAGE_INSIGHTS_HUB,
                mOnDemandCallbackMock,
                null);

        verify(mOptimizationGuideBridgeJniMock, never())
                .canApplyOptimizationOnDemand(
                        anyLong(),
                        any(),
                        any(),
                        anyInt(),
                        any(OptimizationGuideBridge.OnDemandOptimizationGuideCallback.class),
                        any());
        verify(mOnDemandCallbackMock)
                .onOnDemandOptimizationGuideDecision(
                        eq(gurl),
                        eq(OptimizationType.DEFER_ALL_SCRIPT),
                        eq(OptimizationGuideDecision.UNKNOWN),
                        isNull());
        verify(mOnDemandCallbackMock)
                .onOnDemandOptimizationGuideDecision(
                        eq(gurl),
                        eq(OptimizationType.PERFORMANCE_HINTS),
                        eq(OptimizationGuideDecision.UNKNOWN),
                        isNull());
        verify(mOnDemandCallbackMock)
                .onOnDemandOptimizationGuideDecision(
                        eq(gurl2),
                        eq(OptimizationType.DEFER_ALL_SCRIPT),
                        eq(OptimizationGuideDecision.UNKNOWN),
                        isNull());
        verify(mOnDemandCallbackMock)
                .onOnDemandOptimizationGuideDecision(
                        eq(gurl2),
                        eq(OptimizationType.PERFORMANCE_HINTS),
                        eq(OptimizationGuideDecision.UNKNOWN),
                        isNull());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testCanApplyOptimizationOnDemand() {
        GURL gurl = new GURL(TEST_URL);
        GURL gurl2 = new GURL(TEST_URL2);
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);
        RequestContextMetadata requestContextMetadata = RequestContextMetadata.newBuilder().build();

        bridge.canApplyOptimizationOnDemand(
                Arrays.asList(new GURL[] {gurl, gurl2}),
                Arrays.asList(
                        new OptimizationType[] {
                            OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT
                        }),
                RequestContext.CONTEXT_PAGE_INSIGHTS_HUB,
                mOnDemandCallbackMock,
                requestContextMetadata);

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationOnDemand(
                        eq(1L),
                        aryEq(new GURL[] {gurl, gurl2}),
                        aryEq(
                                new int[] {
                                    OptimizationType.PERFORMANCE_HINTS_VALUE,
                                    OptimizationType.DEFER_ALL_SCRIPT_VALUE
                                }),
                        eq(RequestContext.CONTEXT_PAGE_INSIGHTS_HUB_VALUE),
                        eq(mOnDemandCallbackMock),
                        eq(requestContextMetadata.toByteArray()));
    }
}
