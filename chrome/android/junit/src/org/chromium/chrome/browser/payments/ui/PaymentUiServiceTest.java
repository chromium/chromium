// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.payments.PaymentPreferencesUtil;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.payments.JourneyLogger;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentComplete;

import java.util.Collections;

/** Unit tests for {@link PaymentUiService}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentUiServiceTest {
    private static final String PAYMENT_APP_ID = "test-payment-app-id";
    private static final long INITIAL_LAST_USE_DATE = 1000L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private PaymentUiService.Delegate mDelegate;
    @Mock private PaymentRequestParams mParams;
    @Mock private WebContents mWebContents;
    @Mock private JourneyLogger mJourneyLogger;
    @Mock private Profile mProfile;
    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private PaymentApp mPaymentApp;
    @Mock private Runnable mOnUiCompleted;

    private PaymentUiService mPaymentUiService;

    @Before
    public void setUp() {
        Profile.setProfileFromWebContentsForTesting(mProfile);
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
        when(mWebContents.getTitle()).thenReturn("Merchant");
        when(mPaymentApp.getIdentifier()).thenReturn(PAYMENT_APP_ID);
        when(mPaymentApp.canPreselect()).thenReturn(true);

        PaymentPreferencesUtil.setPaymentAppUseCountForTest(PAYMENT_APP_ID, 2);
        PaymentPreferencesUtil.setPaymentAppLastUseDate(PAYMENT_APP_ID, INITIAL_LAST_USE_DATE);

        mPaymentUiService =
                new PaymentUiService(
                        mDelegate,
                        mParams,
                        mWebContents,
                        /* isOffTheRecord= */ false,
                        mJourneyLogger,
                        "https://example.test");
        mPaymentUiService.setPaymentApps(Collections.singletonList(mPaymentApp));
    }

    @After
    public void tearDown() {
        mPaymentUiService.close();
    }

    @Test
    public void testOnPaymentRequestComplete_SuccessUpdatesPaymentAppUsageStats() {
        long timeBefore = System.currentTimeMillis();

        mPaymentUiService.onPaymentRequestComplete(PaymentComplete.SUCCESS, mOnUiCompleted);

        assertEquals(3, PaymentPreferencesUtil.getPaymentAppUseCount(PAYMENT_APP_ID));
        assertTrue(PaymentPreferencesUtil.getPaymentAppLastUseDate(PAYMENT_APP_ID) >= timeBefore);
        verify(mOnUiCompleted).run();
    }

    @Test
    public void testOnPaymentRequestComplete_UnknownUpdatesPaymentAppUsageStats() {
        long timeBefore = System.currentTimeMillis();

        mPaymentUiService.onPaymentRequestComplete(PaymentComplete.UNKNOWN, mOnUiCompleted);

        assertEquals(3, PaymentPreferencesUtil.getPaymentAppUseCount(PAYMENT_APP_ID));
        assertTrue(PaymentPreferencesUtil.getPaymentAppLastUseDate(PAYMENT_APP_ID) >= timeBefore);
        verify(mOnUiCompleted).run();
    }

    @Test
    public void testOnPaymentRequestComplete_FailDoesNotUpdatePaymentAppUsageStats() {
        mPaymentUiService.onPaymentRequestComplete(PaymentComplete.FAIL, mOnUiCompleted);

        assertEquals(2, PaymentPreferencesUtil.getPaymentAppUseCount(PAYMENT_APP_ID));
        assertEquals(
                INITIAL_LAST_USE_DATE,
                PaymentPreferencesUtil.getPaymentAppLastUseDate(PAYMENT_APP_ID));
        verify(mOnUiCompleted).run();
    }
}
