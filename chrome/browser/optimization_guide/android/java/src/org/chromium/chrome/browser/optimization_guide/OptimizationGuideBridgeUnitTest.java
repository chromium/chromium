// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.optimization_guide;

import static org.mockito.AdditionalMatchers.aryEq;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.anyObject;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.ArgumentMatchers.isNull;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import androidx.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.JniMocker;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.HintsProto.OptimizationType;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.url.GURL;

import java.util.Arrays;

/**
 * Unit tests for OptimizationGuideBridge.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class OptimizationGuideBridgeUnitTest {
    private static final String TEST_URL = "https://testurl.com/";
    @Rule
    public JniMocker mocker = new JniMocker();

    @Mock
    OptimizationGuideBridge.Natives mOptimizationGuideBridgeJniMock;

    @Mock
    OptimizationGuideBridge.OptimizationGuideCallback mCallbackMock;

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
        bridge.registerOptimizationTypes(Arrays.asList(new OptimizationType[] {
                OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT}));
        verify(mOptimizationGuideBridgeJniMock, times(1))
                .registerOptimizationTypes(eq(1L), aryEq(new int[] {6, 5}));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testRegisterOptimizationTypes_withoutNativeBridge() {
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(0);
        bridge.registerOptimizationTypes(Arrays.asList(new OptimizationType[] {
                OptimizationType.PERFORMANCE_HINTS, OptimizationType.DEFER_ALL_SCRIPT}));
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
                .canApplyOptimization(anyLong(), anyObject(), anyInt(),
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
                .canApplyOptimization(eq(1L), eq(gurl), eq(6), eq(mCallbackMock));
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testCanApplyOptimizationAsync_withoutNativeBridge() {
        GURL gurl = new GURL(TEST_URL);
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(0);
        NavigationHandle navHandle = NavigationHandle.createForTesting(new GURL(TEST_URL),
                false /* isRendererInitiated */, 0 /* pageTransition */,
                false /* hasUserGesture */);

        bridge.canApplyOptimizationAsync(
                navHandle, OptimizationType.PERFORMANCE_HINTS, mCallbackMock);

        verify(mOptimizationGuideBridgeJniMock, never())
                .canApplyOptimization(anyLong(), anyObject(), anyInt(),
                        any(OptimizationGuideBridge.OptimizationGuideCallback.class));
        verify(mCallbackMock)
                .onOptimizationGuideDecision(eq(OptimizationGuideDecision.UNKNOWN), isNull());
    }

    @Test
    @SmallTest
    @UiThreadTest
    @Feature({"OptimizationHints"})
    public void testCanApplyOptimizationAsync() {
        GURL gurl = new GURL(TEST_URL);
        OptimizationGuideBridge bridge = new OptimizationGuideBridge(1);
        NavigationHandle navHandle =
                NavigationHandle.createForTesting(gurl, false /* isRendererInitiated */,
                        0 /* pageTransition */, false /* hasUserGesture */);

        bridge.canApplyOptimizationAsync(
                navHandle, OptimizationType.PERFORMANCE_HINTS, mCallbackMock);

        verify(mOptimizationGuideBridgeJniMock, times(1))
                .canApplyOptimizationAsync(eq(1L), eq(gurl), eq(6), eq(mCallbackMock));
    }
}
