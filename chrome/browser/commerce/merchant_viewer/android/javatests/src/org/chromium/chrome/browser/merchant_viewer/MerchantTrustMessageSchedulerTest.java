// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Handler;
import android.util.Pair;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/**
 * Tests for {@link MerchantTrustMessageScheduler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustMessageSchedulerTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    private MessageDispatcher mMockMessageDispatcher;

    @Mock
    private WebContents mMockWebContents;

    @Mock
    private MerchantTrustMetrics mMockMetrics;

    @Mock
    private Handler mMockHandler;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doAnswer(invocation -> {
            Runnable runnable = (Runnable) (invocation.getArguments()[0]);
            runnable.run();
            return null;
        })
                .when(mMockHandler)
                .postDelayed(any(Runnable.class), anyLong());
    }

    @Test
    public void testSchedule() throws TimeoutException {
        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        scheduler.setHandlerForTesting(mMockHandler);

        int callCount = callbackHelper.getCallCount();
        scheduler.schedule(
                mockPropteryModel, mockMessagesContext, 2000, callbackHelper::notifyCalled);

        callbackHelper.waitForCallback(callCount);
        Assert.assertNotNull(callbackHelper.getResult());
        verify(mMockHandler, times(1)).postDelayed(any(Runnable.class), eq(2000L));
        verify(mMockMessageDispatcher, times(1))
                .enqueueMessage(eq(mockPropteryModel), eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION));
        verify(mMockMetrics, times(1)).recordMetricsForMessagePrepared();
        verify(mMockMetrics, times(1)).recordMetricsForMessageShown();
    }

    @Test
    public void testScheduleInvalidMessageContext() throws TimeoutException {
        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(false).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        scheduler.setHandlerForTesting(mMockHandler);

        int callCount = callbackHelper.getCallCount();
        scheduler.schedule(
                mockPropteryModel, mockMessagesContext, 2000, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);

        Assert.assertNull(callbackHelper.getResult());
        Assert.assertNull(scheduler.getScheduledMessageContext());

        verify(mMockMessageDispatcher, never())
                .enqueueMessage(eq(mockPropteryModel), eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION));
    }

    @Test
    public void testClear() throws TimeoutException {
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        scheduler.setScheduledMessage(new Pair<MerchantTrustMessageContext, PropertyModel>(
                mockMessagesContext, mockPropteryModel));
        Assert.assertNotNull(scheduler.getScheduledMessageContext());
        scheduler.clear(MessageClearReason.UNKNOWN);
        Assert.assertNull(scheduler.getScheduledMessageContext());
        verify(mMockMessageDispatcher, times(1))
                .dismissMessage(eq(mockPropteryModel), eq(DismissReason.SCOPE_DESTROYED));
        verify(mMockMetrics, times(1))
                .recordMetricsForMessageCleared(eq(MessageClearReason.UNKNOWN));
    }

    @Test
    public void testExpedite() throws TimeoutException {
        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();

        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        scheduler.schedule(
                mockPropteryModel, mockMessagesContext, 50000, callbackHelper::notifyCalled);
        Assert.assertNotNull(scheduler.getScheduledMessageContext());

        MerchantTrustSignalsCallbackHelper expediteCallbackHelper =
                new MerchantTrustSignalsCallbackHelper();
        int callCount = expediteCallbackHelper.getCallCount();
        scheduler.setHandlerForTesting(mMockHandler);
        scheduler.expedite(expediteCallbackHelper::notifyCalled);
        expediteCallbackHelper.waitForCallback(callCount);
        Assert.assertNotNull(expediteCallbackHelper.getResult());
        Assert.assertNull(scheduler.getScheduledMessageContext());
        verify(mMockMessageDispatcher, times(1))
                .enqueueMessage(eq(mockPropteryModel), eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION));
    }

    @Test
    public void testExpediteNoScheduledMessage() throws TimeoutException {
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        Assert.assertNull(scheduler.getScheduledMessageContext());

        MerchantTrustSignalsCallbackHelper expediteCallbackHelper =
                new MerchantTrustSignalsCallbackHelper();
        int callCount = expediteCallbackHelper.getCallCount();
        scheduler.expedite(expediteCallbackHelper::notifyCalled);

        expediteCallbackHelper.waitForCallback(callCount);
        Assert.assertNull(scheduler.getScheduledMessageContext());
        verify(mMockMessageDispatcher, never())
                .enqueueMessage(eq(mockPropteryModel), eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION));
    }

    @Test
    public void testClearNoScheduledMessage() {
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        scheduler.clear(MessageClearReason.UNKNOWN);
        verify(mMockMetrics, times(0))
                .recordMetricsForMessageCleared(eq(MessageClearReason.UNKNOWN));
    }

    private MerchantTrustMessageScheduler getSchedulerUnderTest() {
        return new MerchantTrustMessageScheduler(mMockMessageDispatcher, mMockMetrics);
    }
}