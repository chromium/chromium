// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.payments.PaymentPreferencesUtil;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentRequestParams;

import java.util.Collections;
import java.util.Set;
import java.util.concurrent.TimeUnit;

/** Unit tests for {@link PaymentAppComparator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class PaymentAppComparatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    // Unstubbed, so getPaymentOptions() returns null and PaymentAppComparator skips the shipping
    // and contact rules.
    @Mock private PaymentRequestParams mParams;

    private static PaymentApp createPaymentApp(String id) {
        return new PaymentApp(id, /* label= */ id, /* sublabel= */ null, /* icon= */ null) {
            @Override
            public Set<String> getInstrumentMethodNames() {
                return Collections.emptySet();
            }

            @Override
            public void dismissInstrument() {}
        };
    }

    // Ranking score is -ln(whole_days_since_last_use + 2) / ln(use_count + 2); higher (closer to 0)
    // ranks first.
    @Test
    public void testCompare_SameUseCountDifferentLastUseDate() {
        long now = System.currentTimeMillis();
        PaymentApp recentApp = createPaymentApp("recent_app");
        PaymentApp olderApp = createPaymentApp("older_app");

        // Score: -ln(1 + 2) / ln(5 + 2) = -ln(3) / ln(7) ≈ -0.56
        PaymentPreferencesUtil.setPaymentAppUseCountForTest("recent_app", 5);
        PaymentPreferencesUtil.setPaymentAppLastUseDate(
                "recent_app", now - TimeUnit.DAYS.toMillis(1));

        // Score: -ln(10 + 2) / ln(5 + 2) = -ln(12) / ln(7) ≈ -1.28
        PaymentPreferencesUtil.setPaymentAppUseCountForTest("older_app", 5);
        PaymentPreferencesUtil.setPaymentAppLastUseDate(
                "older_app", now - TimeUnit.DAYS.toMillis(10));

        PaymentAppComparator comparator = new PaymentAppComparator(mParams);

        assertTrue(comparator.compare(recentApp, olderApp) < 0);
        assertTrue(comparator.compare(olderApp, recentApp) > 0);
    }

    @Test
    public void testCompare_MuchMoreRecentOutweighsSlightlyHigherUseCount() {
        long now = System.currentTimeMillis();
        PaymentApp appA = createPaymentApp("app_a");
        PaymentApp appB = createPaymentApp("app_b");

        // Score: -ln(1 + 2) / ln(2 + 2) = -ln(3) / ln(4) ≈ -0.79
        PaymentPreferencesUtil.setPaymentAppUseCountForTest("app_a", 2);
        PaymentPreferencesUtil.setPaymentAppLastUseDate("app_a", now - TimeUnit.DAYS.toMillis(1));

        // Score: -ln(30 + 2) / ln(3 + 2) = -ln(32) / ln(5) ≈ -2.15
        PaymentPreferencesUtil.setPaymentAppUseCountForTest("app_b", 3);
        PaymentPreferencesUtil.setPaymentAppLastUseDate("app_b", now - TimeUnit.DAYS.toMillis(30));

        PaymentAppComparator comparator = new PaymentAppComparator(mParams);

        assertTrue(comparator.compare(appA, appB) < 0);
        assertTrue(comparator.compare(appB, appA) > 0);
    }

    @Test
    public void testCompare_MuchHigherUseCountOutweighsSlightlyMoreRecent() {
        long now = System.currentTimeMillis();
        PaymentApp appA = createPaymentApp("app_a");
        PaymentApp appB = createPaymentApp("app_b");

        // Score: -ln(2 + 2) / ln(10 + 2) = -ln(4) / ln(12) ≈ -0.56
        PaymentPreferencesUtil.setPaymentAppUseCountForTest("app_a", 10);
        PaymentPreferencesUtil.setPaymentAppLastUseDate("app_a", now - TimeUnit.DAYS.toMillis(2));

        // Score: -ln(1 + 2) / ln(1 + 2) = -ln(3) / ln(3) = -1.00
        PaymentPreferencesUtil.setPaymentAppUseCountForTest("app_b", 1);
        PaymentPreferencesUtil.setPaymentAppLastUseDate("app_b", now - TimeUnit.DAYS.toMillis(1));

        PaymentAppComparator comparator = new PaymentAppComparator(mParams);

        assertTrue(comparator.compare(appA, appB) < 0);
        assertTrue(comparator.compare(appB, appA) > 0);
    }
}
