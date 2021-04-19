// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import android.os.Handler;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.components.messages.MessageScopeType;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link MerchantTrustMessageScheduler}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustMessageSchedulerTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
    }

    @Mock
    private MessageDispatcher mMockMessageDispatcher;

    @Mock
    private WebContents mMockWebContents;

    @Test
    public void testSchedule() {
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        scheduler.schedule(mockPropteryModel, mockMessagesContext, 0);
        Robolectric.flushForegroundThreadScheduler();

        verify(mMockMessageDispatcher, times(1))
                .enqueueMessage(eq(mockPropteryModel), eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION));
    }

    @Test
    public void testScheduleInvalidWebContents() {
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(false).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        scheduler.schedule(mockPropteryModel, mockMessagesContext, 0);
        Robolectric.flushForegroundThreadScheduler();

        verify(mMockMessageDispatcher, never())
                .enqueueMessage(eq(mockPropteryModel), eq(mMockWebContents),
                        eq(MessageScopeType.NAVIGATION));
    }

    @Test
    public void testScheduleWithDelay() {
        MerchantTrustMessageScheduler scheduler = getSchedulerUnderTest();
        PropertyModel mockPropteryModel = mock(PropertyModel.class);
        doReturn(false).when(mMockWebContents).isDestroyed();

        MerchantTrustMessageContext mockMessagesContext = mock(MerchantTrustMessageContext.class);
        doReturn(true).when(mockMessagesContext).isValid();
        doReturn(mMockWebContents).when(mockMessagesContext).getWebContents();

        Handler mockHandler = mock(Handler.class);
        scheduler.setHandlerForTesting(mockHandler);
        scheduler.schedule(mockPropteryModel, mockMessagesContext, 100);
        verify(mockHandler, times(1)).postDelayed(any(Runnable.class), eq(100L));
    }

    private MerchantTrustMessageScheduler getSchedulerUnderTest() {
        return new MerchantTrustMessageScheduler(mMockMessageDispatcher);
    }
}