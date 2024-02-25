// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.handler.toolbar;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.payments.handler.toolbar.PaymentHandlerToolbarMediator.PaymentHandlerToolbarMediatorDelegate;
import org.chromium.components.security_state.ConnectionSecurityLevel;
import org.chromium.content_public.browser.NavigationHandle;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/** A test for PaymentHandlerToolbarMediator. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentHandlerToolbarMediatorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Mock private WebContents mMockWebContents;
    @Mock private PaymentHandlerToolbarMediatorDelegate mMockDelegate;

    private PropertyModel mModel;
    private PaymentHandlerToolbarMediator mMediator;

    @Before
    public void setUp() {
        mModel = new PropertyModel.Builder(PaymentHandlerToolbarProperties.ALL_KEYS).build();
        mMediator = new PaymentHandlerToolbarMediator(mModel, mMockWebContents, mMockDelegate);
    }

    @Test
    @Feature({"Payments"})
    public void testDidChangeVisibleSecurityState() {
        Mockito.doReturn(ConnectionSecurityLevel.NONE).when(mMockDelegate).getSecurityLevel();
        Mockito.doReturn(123)
                .when(mMockDelegate)
                .getSecurityIconResource(ConnectionSecurityLevel.NONE);
        Mockito.doReturn("this is content description.")
                .when(mMockDelegate)
                .getSecurityIconContentDescription(ConnectionSecurityLevel.NONE);

        mMediator.didChangeVisibleSecurityState();

        Assert.assertEquals(123, mModel.get(PaymentHandlerToolbarProperties.SECURITY_ICON));
        Assert.assertEquals(
                "this is content description.",
                mModel.get(PaymentHandlerToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION));
    }

    @Test
    @Feature({"Payments"})
    public void testDidStartNavigation() {
        Mockito.doReturn(123).when(mMockDelegate).getSecurityIconResource(Mockito.anyInt());
        Mockito.doReturn("this is content description.")
                .when(mMockDelegate)
                .getSecurityIconContentDescription(Mockito.anyInt());

        NavigationHandle navigation = Mockito.mock(NavigationHandle.class);
        Mockito.when(navigation.isSameDocument()).thenReturn(false);

        mMediator.didStartNavigationInPrimaryMainFrame(navigation);

        Assert.assertEquals(123, mModel.get(PaymentHandlerToolbarProperties.SECURITY_ICON));
        Assert.assertEquals(
                "this is content description.",
                mModel.get(PaymentHandlerToolbarProperties.SECURITY_ICON_CONTENT_DESCRIPTION));
    }
}
