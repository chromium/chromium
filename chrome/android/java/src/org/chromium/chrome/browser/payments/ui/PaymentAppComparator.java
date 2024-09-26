// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.ui;

import org.chromium.chrome.browser.payments.PaymentPreferencesUtil;
import org.chromium.components.autofill.Completable;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentRequestParams;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.Comparator;

/**
 * A comparator that is used to rank the payment apps to be listed on the PaymentRequest
 * UI.
 */
/* package */ class PaymentAppComparator implements Comparator<PaymentApp> {
    private final PaymentRequestParams mParams;

    /**
     * Create an instance of PaymentAppComparator.
     * @param params The parameters of PaymentRequest specified by the merchant.
     */
    /* package */ PaymentAppComparator(PaymentRequestParams params) {
        mParams = params;
    }

    /**
     * Compares two payment apps by ranking score.
     * Return negative value if a has strictly lower ranking score than b.
     * Return zero if a and b have the same ranking score.
     * Return positive value if a has strictly higher ranking score than b.
     */
    private static int compareAppsByRankingScore(PaymentApp a, PaymentApp b) {
        int aCount = PaymentPreferencesUtil.getPaymentAppUseCount(a.getIdentifier());
        int bCount = PaymentPreferencesUtil.getPaymentAppUseCount(b.getIdentifier());
        long aDate = PaymentPreferencesUtil.getPaymentAppLastUseDate(a.getIdentifier());
        long bDate = PaymentPreferencesUtil.getPaymentAppLastUseDate(a.getIdentifier());

        return Double.compare(getRankingScore(aCount, aDate), getRankingScore(bCount, bDate));
    }

    /**
     * Compares two Completable by completeness score.
     * Return negative value if a has strictly lower completeness score than b.
     * Return zero if a and b have the same completeness score.
     * Return positive value if a has strictly higher completeness score than b.
     */
    /* package */ static int compareCompletablesByCompleteness(Completable a, Completable b) {
        return Integer.compare(a.getCompletenessScore(), b.getCompletenessScore());
    }

    /**
     * The ranking score is calculated according to use count and last use date. The formula is
     * the same as the one used in GetRankingScore in autofill_data_model.cc.
     */
    private static double getRankingScore(int count, long date) {
        long currentTime = System.currentTimeMillis();
        return -Math.log((double) ((currentTime - date) / (24 * 60 * 60 * 1000) + 2))
                / Math.log(count + 2);
    }

    /**
     * Sorts the payment apps by several rules:
     * Rule 1: Complete apps before incomplete apps.
     * Rule 2: When shipping address is requested, apps which will handle shipping address before
     * others.
     * Rule 3: When payer's contact information is requested, apps which will handle more required
     * contact fields (name, email, phone) come before others.
     * Rule 4: Preselectable apps before non-preselectable apps.
     * Rule 5: Frequently and recently used apps before rarely and non-recently used apps.
     */
    @Override
    public int compare(PaymentApp a, PaymentApp b) {
        // Complete cards before cards with missing information.
        int completeness = compareCompletablesByCompleteness(b, a);
        if (completeness != 0) return completeness;

        PaymentOptions options = mParams.getPaymentOptions();
        if (options != null) {
            // Payment apps which handle shipping address before others.
            if (options.requestShipping) {
                int canHandleShipping =
                        (b.handlesShippingAddress() ? 1 : 0) - (a.handlesShippingAddress() ? 1 : 0);
                if (canHandleShipping != 0) return canHandleShipping;
            }

            // Payment apps which handle more contact information fields come first.
            int aSupportedContactDelegationsNum = 0;
            int bSupportedContactDelegationsNum = 0;
            if (options.requestPayerName) {
                if (a.handlesPayerName()) aSupportedContactDelegationsNum++;
                if (b.handlesPayerName()) bSupportedContactDelegationsNum++;
            }
            if (options.requestPayerEmail) {
                if (a.handlesPayerEmail()) aSupportedContactDelegationsNum++;
                if (b.handlesPayerEmail()) bSupportedContactDelegationsNum++;
            }
            if (options.requestPayerPhone) {
                if (a.handlesPayerPhone()) aSupportedContactDelegationsNum++;
                if (b.handlesPayerPhone()) bSupportedContactDelegationsNum++;
            }
            if (bSupportedContactDelegationsNum != aSupportedContactDelegationsNum) {
                return bSupportedContactDelegationsNum - aSupportedContactDelegationsNum > 0
                        ? 1
                        : -1;
            }
        }

        // Preselectable apps before non-preselectable apps.
        // Note that this only affects service worker payment apps' apps for now
        // since autofill cards have already been sorted by preselect after sorting by completeness.
        // And the other payment apps can always be preselected.
        int canPreselect = (b.canPreselect() ? 1 : 0) - (a.canPreselect() ? 1 : 0);
        if (canPreselect != 0) return canPreselect;

        // More frequently and recently used apps first.
        return compareAppsByRankingScore(b, a);
    }
}
