// Copyright 2020 The Chromium Authors
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
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.browser.payments.test_support.ShadowProfile;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.payments.InvalidPaymentRequest;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.components.payments.test_support.DefaultPaymentFeatureConfig;
import org.chromium.components.payments.test_support.ShadowWebContentsStatics;
import org.chromium.content_public.browser.PermissionsPolicyFeature;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;

import java.util.concurrent.atomic.AtomicInteger;

/** A test for ChromePaymentRequestFactory. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowWebContentsStatics.class, ShadowProfile.class})
public class ChromePaymentRequestFactoryTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock RenderFrameHost mRenderFrameHost;
    @Mock WebContents mWebContents;
    @Mock Profile mProfile;

    @Before
    public void setUp() {
        DefaultPaymentFeatureConfig.setDefaultFlagConfigurationForTesting();

        setWebContentsDestroyed(false);
        ShadowWebContentsStatics.setWebContents(mWebContents);

        Mockito.doReturn(true).when(mProfile).isOffTheRecord();
        ShadowProfile.setProfile(mProfile);

        setPaymentPermissionsPolicy(true);
    }

    private void setPaymentPermissionsPolicy(boolean enabled) {
        Mockito.doReturn(enabled)
                .when(mRenderFrameHost)
                .isFeatureEnabled(PermissionsPolicyFeature.PAYMENT);
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
        Assert.assertTrue(
                createFactory(/* renderFrameHost= */ null).createImpl()
                        instanceof InvalidPaymentRequest);
    }

    @Test
    @Feature({"Payments"})
    public void testDisabledPolicyCausesBadMessage() {
        setPaymentPermissionsPolicy(false);
        AtomicInteger isKilledReason = new AtomicInteger(0);
        Mockito.doAnswer(
                        invocation -> {
                            isKilledReason.set((int) invocation.getArguments()[0]);
                            return null;
                        })
                .when(mRenderFrameHost)
                .terminateRendererDueToBadMessage(Mockito.anyInt());
        Assert.assertNull(createFactory(mRenderFrameHost).createImpl());
        // 241 == PAYMENTS_WITHOUT_PERMISSION.
        Assert.assertEquals(isKilledReason.get(), 241);
    }

    @Test
    @Feature({"Payments"})
    @DisableFeatures(PaymentFeatureList.WEB_PAYMENTS)
    public void testDisabledFeatureCausesInvalidPaymentRequest() {
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
