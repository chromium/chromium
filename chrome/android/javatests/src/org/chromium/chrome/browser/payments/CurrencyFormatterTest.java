// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.support.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.components.payments.CurrencyFormatter;

import java.util.Arrays;
import java.util.List;

/**
 * A lightweight integration test for CurrencyFormatter to run on an Android device.
 */
@RunWith(BaseJUnit4ClassRunner.class)
public class CurrencyFormatterTest {
    @Rule
    public ChromeBrowserTestRule mActivityTestRule = new ChromeBrowserTestRule();

    /**
     * Unicode non-breaking space.
     */
    private static final String NBSP = "\u00A0";
    private static final String NarrowNBSP = "\u202F";

    private static String longStringOfLength(int len) {
        StringBuilder currency = new StringBuilder();
        for (int i = 0; i < len; i++) {
            currency.append("A");
        }
        return currency.toString();
    }

    @Test
    @MediumTest
    public void testMultipleConversions() {
        // Note, all spaces are expected to be unicode non-breaking spaces. Here they are shown as
        // normal spaces.
        List<Object[]> testCases = Arrays.asList(new Object[][] {
                {"55.00", "USD", "en-US", "USD", "$55.00"},
                {"55.00", "USD", "en-CA", "USD", "$55.00"},
                {"55.00", "USD", "fr-CA", "USD", "55,00 $"},
                {"55.00", "USD", "fr-FR", "USD", "55,00 $"},
                {"1234", "USD", "fr-FR", "USD", "1 234,00 $"},

                {"55.5", "USD", "en-US", "USD", "$55.50"}, {"55", "USD", "en-US", "USD", "$55.00"},
                {"-123", "USD", "en-US", "USD", "-$123.00"},
                {"-1234", "USD", "en-US", "USD", "-$1,234.00"},
                {"0.1234", "USD", "en-US", "USD", "$0.1234"},

                {"55.00", "EUR", "en-US", "EUR", "€55.00"},
                {"55.00", "EUR", "en-CA", "EUR", "€55.00"},
                {"55.00", "EUR", "fr-CA", "EUR", "55,00 €"},
                {"55.00", "EUR", "fr-FR", "EUR", "55,00 €"},

                {"55.00", "CAD", "en-US", "CAD", "$55.00"},
                {"55.00", "CAD", "en-CA", "CAD", "$55.00"},
                {"55.00", "CAD", "fr-CA", "CAD", "55,00 $"},
                {"55.00", "CAD", "fr-FR", "CAD", "55,00 $"},

                {"55", "JPY", "ja-JP", "JPY", "￥55"}, {"55.0", "JPY", "ja-JP", "JPY", "￥55"},
                {"55.00", "JPY", "ja-JP", "JPY", "￥55"},
                {"55.12", "JPY", "ja-JP", "JPY", "￥55.12"},
                {"55.49", "JPY", "ja-JP", "JPY", "￥55.49"},
                {"55.50", "JPY", "ja-JP", "JPY", "￥55.5"},
                {"55.9999", "JPY", "ja-JP", "JPY", "￥55.9999"},

                // Unofficial ISO 4217 currency code.
                {"55.00", "BTX", "en-US", "BTX", "55.00"},
                {"-0.00000001", "BTX", "en-US", "BTX", "-0.00000001"},
                {"-55.00", "BTX", "fr-FR", "BTX", "-55,00"},

                {"123456789012345678901234567890.123456789012345678901234567890", "USD", "fr-FR",
                        "USD", "123 456 789 012 345 678 901 234 567 890,123456789 $"},
                {"123456789012345678901234567890.123456789012345678901234567890", "USD", "en-US",
                        "USD", "$123,456,789,012,345,678,901,234,567,890.123456789"},

                // Any string of at most 2048 characters can be valid amount currency codes.
                {"55.00", "", "en-US", "", "55.00"},
                {"55.00", "ABCDEF", "en-US", "ABCDE\u2026", "55.00"},
                // Currency code more than 6 character is formatted to first 5 characters and
                // ellipsis. "\u2026" is unicode for ellipsis.
                {"55.00", longStringOfLength(2048), "en-US", "AAAAA\u2026", "55.00"},
        });

        for (int i = 0; i < testCases.size(); i++) {
            Object[] testCase = testCases.get(i);

            String amount = (String) testCase[0];
            String currency = (String) testCase[1];
            String locale = (String) testCase[2];
            String expectedCurrencyFormatting = (String) testCase[3];
            String expectedAmountFormatting = (String) testCase[4];

            CurrencyFormatter formatter =
                    new CurrencyFormatter(currency, LocaleUtils.forLanguageTag(locale));
            // To make tests robust against the CLDR data change in terms of space (ASCII
            // space, NBSP and Narrow NBSP), fold NBSP and NarrowNBSP into U+0020.
            String formattedAmount = formatter.format(amount).replace(NBSP, " ");
            Assert.assertEquals("\"" + currency + "\" \"" + amount + "\" (\"" + locale
                            + "\" locale) should be formatted into \"" + expectedAmountFormatting
                            + "\"",
                    expectedAmountFormatting, formattedAmount.replace(NarrowNBSP, " "));
            Assert.assertEquals("\"" + currency + "\""
                            + " should be formatted into \"" + expectedCurrencyFormatting + "\"",
                    expectedCurrencyFormatting, formatter.getFormattedCurrencyCode());
        }
    }
}
