// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.FeaturePolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.services.service_manager.InterfaceProvider;

import java.util.concurrent.atomic.AtomicBoolean;

/** A test for ChromePaymentRequestFactory. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE,
        shadows = {ShadowPaymentFeatureList.class, ShadowWebContentsStatics.class,
                ShadowProfile.class})
public class ChromePaymentRequestFactoryTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);
    @Mock
    RenderFrameHost mRenderFrameHost;
    @Mock
    WebContents mWebContents;
    @Mock
    Profile mProfile;

    @Before
    public void setUp() {
        ShadowPaymentFeatureList.setFeatureEnabled(PaymentFeatureList.WEB_PAYMENTS, true);

        setWebContentsDestroyed(false);
        ShadowWebContentsStatics.setWebContents(mWebContents);

        Mockito.doReturn(true).when(mProfile).isOffTheRecord();
        ShadowProfile.setProfile(mProfile);

        setPaymentFeaturePolicy(true);
    }

    private void setPaymentFeaturePolicy(boolean enabled) {
        Mockito.doReturn(enabled)
                .when(mRenderFrameHost)
                .isFeatureEnabled(FeaturePolicyFeature.PAYMENT);
    }

    private void setWebContentsDestroyed(boolean isDestroyed) {
        Mockito.doReturn(isDestroyed).when(mWebContents).isDestroyed();
    }

    private ChromePaymentRequestFactory createFactory(RenderFrameHost renderFrameHost) {
        return new ChromePaymentRequestFactory(renderFrameHost);
    }

    @Test
    @Feature({"Payments"})
    public void testNullFrameCausesInvalidPaymentRequest() {
        Assert.assertTrue(createFactory(/*renderFrameHost=*/null).createImpl()
                                  instanceof InvalidPaymentRequest);
    }

    @Test
    @Feature({"Payments"})
    public void testDisabledPolicyCausesNullReturn() {
        setPaymentFeaturePolicy(false);
        InterfaceProvider provider = Mockito.mock(InterfaceProvider.class);
        Mockito.doReturn(provider).when(mRenderFrameHost).getRemoteInterfaces();
        AtomicBoolean isConnectionError = new AtomicBoolean(false);
        Mockito.doAnswer((args) -> {
                   isConnectionError.set(true);
                   return null;
               })
                .when(provider)
                .onConnectionError(Mockito.any());
        Assert.assertNull(createFactory(mRenderFrameHost).createImpl());
        Assert.assertTrue(isConnectionError.get());
    }

    @Test
    @Feature({"Payments"})
    public void testDisabledFeatureCausesInvalidPaymentRequest() {
        ShadowPaymentFeatureList.setFeatureEnabled(PaymentFeatureList.WEB_PAYMENTS, false);
        Assert.assertTrue(
                createFactory(mRenderFrameHost).createImpl() instanceof InvalidPaymentRequest);
    }

    @Test
    @Feature({"Payments"})
    public void testNullWebContentsCausesInvalidPaymentRequest() {
        ShadowWebContentsStatics.setWebContents(null);
        Assert.assertTrue(
                createFactory(mRenderFrameHost).createImpl() instanceof InvalidPaymentRequest);
    }

    @Test
    @Feature({"Payments"})
    public void testDestroyedWebContentsCausesInvalidPaymentRequest() {
        setWebContentsDestroyed(true);
        Assert.assertTrue(
                createFactory(mRenderFrameHost).createImpl() instanceof InvalidPaymentRequest);
    }

    @Test
    @Feature({"Payments"})
    public void testPaymentRequestIsReturned() {
        Assert.assertFalse(
                createFactory(mRenderFrameHost).createImpl() instanceof InvalidPaymentRequest);
    }
}
