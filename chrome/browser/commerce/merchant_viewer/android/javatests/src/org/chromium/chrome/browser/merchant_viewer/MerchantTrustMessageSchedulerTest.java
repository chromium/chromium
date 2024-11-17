// Copyright 2021 The Chromium Authors
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
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.FeatureList;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.merchant_viewer.MerchantTrustMetrics.MessageClearReason;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.messages.DismissReason;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.concurrent.TimeoutException;

/** Tests for {@link MerchantTrustMessageScheduler}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustMessageSchedulerTest {

    @Mock private MessageDispatcher mMockMessageDispatcher;

    @Mock private WebContents mMockWebContents;

    @Mock private MerchantTrustMetrics mMockMetrics;

    @Mock private Handler mMockHandler;

    @Mock private ObservableSupplier<Tab> mMockTabProvider;

    @Mock private Tab mMockTab;

    @Mock private WebContents mMockWebContents2;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        doAnswer(
                        invocation -> {
                            Runnable runnable = (Runnable) invocation.getArguments()[0];
                            runnable.run();
                            return null;
                        })
                .when(mMockHandler)
                .postDelayed(any(Runnable.class), anyLong());
        doReturn(mMockTab).when(mMockTabProvider).get();
        doReturn(true).when(mMockTabProvider).hasValue();
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
        doReturn(mMockWebContents).when(mMockTab).getWebContents();
        doReturn("fake_host").when(mockMessagesContext).getHostName();

        scheduler.setHandlerForTesting(mMockHandler);

        int callCount = callbackHelper.getCallCount();
        scheduler.schedule(
                mockPropteryModel, 4.7, mockMessagesContext, 2000, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);

        Assert.assertNotNull(callbackHelper.getResult());
        verify(mMockMetrics, times(1)).recordMetricsForMessagePrepared();
        verify(mMockHandler, times(1)).postDelayed(any(Runnable.class), eq(2000L));
        verify(mMockMessageDispatcher, times(1))
                .enqueueMessage(
                        eq(mockPropteryModel),
                        eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        eq(false));
        verify(mMockMetrics, times(1)).startRecordingMessageImpact(eq("fake_host"), eq(4.7));
        verify(mMockMetrics, times(1)).recordMetricsForMessageShown();
        Assert.assertNull(scheduler.getScheduledMessageContext());
    }

    @Test
    public void testSchedule_DisableMessageForImpactStudy() throws TimeoutException {
        FeatureList.TestValues testValues = new FeatureList.TestValues();
        testValues.addFieldTrialParamOverride(
                ChromeFeatureList.COMMERCE_MERCHANT_VIEWER,
                MerchantViewerConfig.TRUST_SIGNALS_MESSAGE_DISABLED_FOR_IMPACT_STUDY_PARAM,
                "true");
        FeatureList.setTestValues(testValues);

        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();
        doReturn(mMockWebContents).when(mMockTab).getWebContents();
        doReturn("fake_host").when(mockMessagesContext).getHostName();

        scheduler.setHandlerForTesting(mMockHandler);

        int callCount = callbackHelper.getCallCount();
        scheduler.schedule(
                mockPropteryModel, 4.7, mockMessagesContext, 2000, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);

        Assert.assertNotNull(callbackHelper.getResult());
        verify(mMockMetrics, times(1)).recordMetricsForMessagePrepared();
        verify(mMockHandler, times(1)).postDelayed(any(Runnable.class), eq(2000L));
        verify(mMockMessageDispatcher, times(0))
                .enqueueMessage(
                        eq(mockPropteryModel),
                        eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        eq(false));
        verify(mMockMetrics, times(1)).startRecordingMessageImpact(eq("fake_host"), eq(4.7));
        verify(mMockMetrics, times(0)).recordMetricsForMessageShown();
        verify(mMockMetrics, times(1))
                .recordMetricsForMessageCleared(eq(MessageClearReason.UNKNOWN));
        Assert.assertNull(scheduler.getScheduledMessageContext());
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
        doReturn(mMockWebContents).when(mMockTab).getWebContents();

        scheduler.setHandlerForTesting(mMockHandler);

        int callCount = callbackHelper.getCallCount();
        scheduler.schedule(
                mockPropteryModel, mockMessagesContext, 2000, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);

        Assert.assertNull(callbackHelper.getResult());
        verify(mMockMetrics, times(1)).recordMetricsForMessagePrepared();
        verify(mMockMetrics, times(1))
                .recordMetricsForMessageCleared(
                        eq(MessageClearReason.MESSAGE_CONTEXT_NO_LONGER_VALID));
        verify(mMockMessageDispatcher, never())
                .enqueueMessage(
                        eq(mockPropteryModel),
                        eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        eq(false));
        Assert.assertNull(scheduler.getScheduledMessageContext());
    }

    @Test
    public void testScheduleDifferentWebContents() throws TimeoutException {
        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();
        doReturn(mMockWebContents2).when(mMockTab).getWebContents();

        scheduler.setHandlerForTesting(mMockHandler);

        int callCount = callbackHelper.getCallCount();
        scheduler.schedule(
                mockPropteryModel, mockMessagesContext, 2000, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);

        Assert.assertNull(callbackHelper.getResult());
        verify(mMockMetrics, times(1)).recordMetricsForMessagePrepared();
        verify(mMockMetrics, times(1))
                .recordMetricsForMessageCleared(
                        eq(MessageClearReason.SWITCH_TO_DIFFERENT_WEBCONTENTS));
        verify(mMockMessageDispatcher, never())
                .enqueueMessage(
                        eq(mockPropteryModel),
                        eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        eq(false));
        Assert.assertNull(scheduler.getScheduledMessageContext());
    }

    @Test
    public void testScheduleUnknownClearReason() throws TimeoutException {
        MerchantTrustSignalsCallbackHelper callbackHelper =
                new MerchantTrustSignalsCallbackHelper();
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();
        doReturn(mMockWebContents).when(mMockTab).getWebContents();
        doReturn(false).when(mMockTabProvider).hasValue();

        scheduler.setHandlerForTesting(mMockHandler);

        int callCount = callbackHelper.getCallCount();
        scheduler.schedule(
                mockPropteryModel, mockMessagesContext, 2000, callbackHelper::notifyCalled);
        callbackHelper.waitForCallback(callCount);

        Assert.assertNull(callbackHelper.getResult());
        verify(mMockMetrics, times(1)).recordMetricsForMessagePrepared();
        verify(mMockMetrics, times(1))
                .recordMetricsForMessageCleared(eq(MessageClearReason.UNKNOWN));
        verify(mMockMessageDispatcher, never())
                .enqueueMessage(
                        eq(mockPropteryModel),
                        eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION),
                        eq(false));
        Assert.assertNull(scheduler.getScheduledMessageContext());
    }

    @Test
    public void testClear() throws TimeoutException {
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        scheduler.setScheduledMessage(
                new Pair<MerchantTrustMessageContext, PropertyModel>(
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
    public void testClearNoScheduledMessage() {
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        scheduler.clear(MessageClearReason.UNKNOWN);
        verify(mMockMetrics, times(0))
                .recordMetricsForMessageCleared(eq(MessageClearReason.UNKNOWN));
    }

    private MerchantTrustMessageScheduler getSchedulerUnderTest() {
        return new MerchantTrustMessageScheduler(
                mMockMessageDispatcher, mMockMetrics, mMockTabProvider);
    }
}
