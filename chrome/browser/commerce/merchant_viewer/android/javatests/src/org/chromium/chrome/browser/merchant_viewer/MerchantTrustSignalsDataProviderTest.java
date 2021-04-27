// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;

import com.google.protobuf.ByteString;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.mockito.invocation.InvocationOnMock;
import org.mockito.stubbing.Answer;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.merchant_viewer.proto.MerchantTrustSignalsOuterClass.MerchantTrustSignals;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridge.OptimizationGuideCallback;
import org.chromium.chrome.browser.optimization_guide.OptimizationGuideBridgeJni;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.optimization_guide.OptimizationGuideDecision;
import org.chromium.components.optimization_guide.proto.CommonTypesProto.Any;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;
/**
 * Tests for {@link MerchantTrustSignalsDataProvider}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustSignalsDataProviderTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private GURL mMockDestinationGurl;

    @Rule
    public JniMocker mMocker = new JniMocker();

    @Mock
    private Profile mMockProfile;

    @Mock
    private OptimizationGuideBridge.Natives mMockOptimizationGuideBridgeJni;

    static final MerchantTrustSignals FAKE_MERCHANT_TRUST_SIGNALS =
            MerchantTrustSignals.newBuilder()
                    .setMerchantStarRating(4.5f)
                    .setMerchantCountRating(100)
                    .setMerchantDetailsPageUrl("http://dummy/url")
                    .build();

    static final Any ANY_MERHCANT_TRUST_SIGNALS =
            Any.newBuilder()
                    .setValue(ByteString.copyFrom(FAKE_MERCHANT_TRUST_SIGNALS.toByteArray()))
                    .build();

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mMocker.mock(OptimizationGuideBridgeJni.TEST_HOOKS, mMockOptimizationGuideBridgeJni);
        doReturn(1L).when(mMockOptimizationGuideBridgeJni).init();
        doReturn(false).when(mMockProfile).isOffTheRecord();
        Profile.setLastUsedProfileForTesting(mMockProfile);
    }

    @Test
    public void testGetDataForUrlNoMetadata() throws TimeoutException {
        MerchantTrustSignalsDataProvider instance = getDataProvider();

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        int callCount = callbackHelper.getCallCount();
        mockOptimizationGuideResponse(mMockOptimizationGuideBridgeJni,
                OptimizationGuideDecision.FALSE, ANY_MERHCANT_TRUST_SIGNALS);
        instance.getDataForUrl(mMockDestinationGurl, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);
        Assert.assertNull(callbackHelper.getMerchantTrustSignalsResult());
    }

    @Test
    public void testGetDataForUrlNullMetadata() throws TimeoutException {
        MerchantTrustSignalsDataProvider instance = getDataProvider();

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        int callCount = callbackHelper.getCallCount();
        mockOptimizationGuideResponse(
                mMockOptimizationGuideBridgeJni, OptimizationGuideDecision.TRUE, null);
        instance.getDataForUrl(mMockDestinationGurl, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);
        Assert.assertNull(callbackHelper.getMerchantTrustSignalsResult());
    }

    @Test
    public void testGetDataForUrlInvalidMetadata() throws TimeoutException {
        MerchantTrustSignalsDataProvider instance = getDataProvider();

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        int callCount = callbackHelper.getCallCount();
        mockOptimizationGuideResponse(mMockOptimizationGuideBridgeJni,
                OptimizationGuideDecision.TRUE, Any.getDefaultInstance());
        instance.getDataForUrl(mMockDestinationGurl, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);
        Assert.assertNull(callbackHelper.getMerchantTrustSignalsResult());
    }

    @Test
    public void testGetDataForUrlValid() throws TimeoutException {
        MerchantTrustSignalsDataProvider instance = getDataProvider();

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        int callCount = callbackHelper.getCallCount();
        mockOptimizationGuideResponse(mMockOptimizationGuideBridgeJni,
                OptimizationGuideDecision.TRUE, ANY_MERHCANT_TRUST_SIGNALS);
        instance.getDataForUrl(mMockDestinationGurl, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);

        MerchantTrustSignals result = callbackHelper.getMerchantTrustSignalsResult();
        Assert.assertNotNull(result);
        Assert.assertEquals(4.5f, result.getMerchantStarRating(), 0.0f);
        Assert.assertEquals(100, result.getMerchantCountRating());
        Assert.assertEquals("http://dummy/url", result.getMerchantDetailsPageUrl());
    }

    static void mockOptimizationGuideResponse(OptimizationGuideBridge.Natives optimizationGuideJni,
            @OptimizationGuideDecision int decision, Any metadata) {
        doAnswer(new Answer<Void>() {
            @Override
            public Void answer(InvocationOnMock invocation) {
                OptimizationGuideCallback callback =
                        (OptimizationGuideCallback) invocation.getArguments()[3];
                callback.onOptimizationGuideDecision(decision, metadata);
                return null;
            }
        })
                .when(optimizationGuideJni)
                .canApplyOptimization(
                        anyLong(), any(GURL.class), anyInt(), any(OptimizationGuideCallback.class));
    }

    private MerchantTrustSignalsDataProvider getDataProvider() {
        return new MerchantTrustSignalsDataProvider();
    }
}