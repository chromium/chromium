// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.merchant_viewer;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tabmodel.TabModelFilterProvider;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Tests for {@link MerchantTrustSignalsCoordinator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class MerchantTrustSignalsCoordinatorTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();
    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        doReturn(mTabModelFilterProvider).when(mMockTabModelSelector).getTabModelFilterProvider();
    }

    @Mock
    private TabModelSelector mMockTabModelSelector;

    @Mock
    private TabModelFilterProvider mTabModelFilterProvider;

    @Mock
    private MerchantTrustMessageScheduler mMockMerchantMessageScheduler;

    @Mock
    private WebContents mMockWebContents;

    @Test
    public void testMaybeDisplayMessage() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(null).when(mMockMerchantMessageScheduler).getScheduledMessageContext();

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext("fake_host", mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1)).clear();

        // TODO: validate PropertyModel once it's populated.
        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq(MerchantTrustSignalsCoordinator.MESSAGE_ENQUEUE_DELAY_MILLIS));
    }

    @Test
    public void testMaybeDisplayMessageWithScheduledMessage() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(new MerchantTrustMessageContext("fake_host", mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext("fake_host", mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1)).clear();

        // TODO: validate PropertyModel once it's populated.
        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq(MerchantTrustMessageScheduler.MESSAGE_ENQUEUE_NO_DELAY));
    }

    @Test
    public void testMaybeDisplayMessageWithScheduledMessageDifferentHost() {
        // Verify previous scheduled message is canceled.
        MerchantTrustSignalsCoordinator coordinator = getCoordinatorUnderTest();
        doReturn(new MerchantTrustMessageContext("different_host", mMockWebContents))
                .when(mMockMerchantMessageScheduler)
                .getScheduledMessageContext();

        coordinator.maybeDisplayMessage(
                new MerchantTrustMessageContext("fake_host", mMockWebContents));

        verify(mMockMerchantMessageScheduler, times(1)).clear();

        // TODO: validate PropertyModel once it's populated.
        verify(mMockMerchantMessageScheduler, times(1))
                .schedule(any(PropertyModel.class), any(MerchantTrustMessageContext.class),
                        eq(MerchantTrustSignalsCoordinator.MESSAGE_ENQUEUE_DELAY_MILLIS));
    }

    private MerchantTrustSignalsCoordinator getCoordinatorUnderTest() {
        return new MerchantTrustSignalsCoordinator(
                mMockTabModelSelector, mMockMerchantMessageScheduler);
    }
}