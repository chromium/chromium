// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import static org.junit.Assert.assertEquals;

import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.net.test.EmbeddedTestServerRule;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for metrics collected by {@link CustomTabActivity} launched in incognito
 * mode.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class CustomTabActivityIncognitoMetricTest {
    private static final String UMA_KEY = "CustomTabs.IncognitoCCTCallerId";
    private static final String FIRST_PARTY_UMA_KEY = "CustomTabs.ClientAppId.Incognito";
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    private String mTestPage;

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

    @Rule public TestRule mProcessor = new Features.InstrumentationProcessor();

    @Rule public EmbeddedTestServerRule mEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Before
    public void setUp() throws TimeoutException {
        mTestPage = mEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        IncognitoDataTestUtils.fireAndWaitForCctWarmup();
    }

    private Intent createMinimalIncognitoCustomTabIntent() {
        return CustomTabsIntentTestUtils.createMinimalIncognitoCustomTabIntent(
                ApplicationProvider.getApplicationContext(), mTestPage);
    }

    @Test
    @MediumTest
    public void recordsHistogram_1P() {
        assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(UMA_KEY));
        Intent intent = createMinimalIncognitoCustomTabIntent();
        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);

        assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(UMA_KEY));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        UMA_KEY, IntentHandler.IncognitoCCTCallerId.GOOGLE_APPS));
    }

    @Test
    @MediumTest
    public void recordsHistogram_ReaderMode_WithExtra() {
        assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(UMA_KEY));
        Intent intent = createMinimalIncognitoCustomTabIntent();
        CustomTabIntentDataProvider.addReaderModeUIExtras(intent);
        IncognitoCustomTabIntentDataProvider.addIncognitoExtrasForChromeFeatures(
                intent, IntentHandler.IncognitoCCTCallerId.READER_MODE);

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(UMA_KEY));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        UMA_KEY, IntentHandler.IncognitoCCTCallerId.READER_MODE));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY)
    public void recordsHistogram_Other() {
        assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(UMA_KEY));
        Intent intent = createMinimalIncognitoCustomTabIntent();
        // Remove the first party override to emulate third party
        mCustomTabActivityTestRule.setRemoveFirstPartyOverride();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        assertEquals(1, RecordHistogram.getHistogramTotalCountForTesting(UMA_KEY));
        assertEquals(
                1,
                RecordHistogram.getHistogramValueCountForTesting(
                        UMA_KEY, IntentHandler.IncognitoCCTCallerId.OTHER_APPS));
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY)
    public void doesNotRecordThirdPartySpecificHistogram() {
        assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(FIRST_PARTY_UMA_KEY));
        Intent intent = createMinimalIncognitoCustomTabIntent();

        // Remove the first party override to emulate third party
        mCustomTabActivityTestRule.setRemoveFirstPartyOverride();

        mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        assertEquals(0, RecordHistogram.getHistogramTotalCountForTesting(FIRST_PARTY_UMA_KEY));
    }
}
