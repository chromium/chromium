// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.IntentHandler.IncognitoCCTCallerId;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.incognito.IncognitoDataTestUtils;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
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
    private static final String IS_TRUSTED_UMA_KEY = "CustomTabs.IncognitoCCTCallerIsTrusted";
    private static final String FIRST_PARTY_UMA_KEY = "CustomTabs.ClientAppId.Incognito";
    private static final String TEST_PAGE = "/chrome/test/data/android/google.html";

    private String mTestPage;

    @Rule
    public IncognitoCustomTabActivityTestRule mCustomTabActivityTestRule =
            new IncognitoCustomTabActivityTestRule();

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
        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(UMA_KEY, IntentHandler.IncognitoCCTCallerId.GOOGLE_APPS)
                        .expectBooleanRecord(IS_TRUSTED_UMA_KEY, true)
                        .build()) {
            Intent intent = createMinimalIncognitoCustomTabIntent();
            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        }
    }

    @Test
    @MediumTest
    public void recordsHistogram_ReaderMode_WithExtra() {
        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(UMA_KEY, IncognitoCCTCallerId.READER_MODE)
                        .expectBooleanRecord(IS_TRUSTED_UMA_KEY, true)
                        .build()) {
            Intent intent = createMinimalIncognitoCustomTabIntent();
            CustomTabIntentDataProvider.addReaderModeUIExtras(intent);
            IncognitoCustomTabIntentDataProvider.addIncognitoExtrasForChromeFeatures(
                    intent, IntentHandler.IncognitoCCTCallerId.READER_MODE);

            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        }
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY)
    public void recordsHistogram_Other() {
        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectIntRecord(UMA_KEY, IncognitoCCTCallerId.OTHER_APPS)
                        .expectNoRecords(IS_TRUSTED_UMA_KEY)
                        .build()) {
            Intent intent = createMinimalIncognitoCustomTabIntent();
            // Remove the first party override to emulate third party.
            mCustomTabActivityTestRule.setRemoveFirstPartyOverride();

            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        }
    }

    @Test
    @MediumTest
    public void recordsHistogram_OtherUntrusted() {
        try (var ignored =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(UMA_KEY)
                        .expectBooleanRecord(IS_TRUSTED_UMA_KEY, false)
                        .build()) {
            Intent intent = createMinimalIncognitoCustomTabIntent();
            // Remove the first party override to emulate third party.
            mCustomTabActivityTestRule.setRemoveFirstPartyOverride();

            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        }
    }

    @Test
    @MediumTest
    @EnableFeatures(ChromeFeatureList.CCT_INCOGNITO_AVAILABLE_TO_THIRD_PARTY)
    public void doesNotRecordThirdPartySpecificHistogram() {
        try (var ignored =
                HistogramWatcher.newBuilder().expectNoRecords(FIRST_PARTY_UMA_KEY).build()) {
            Intent intent = createMinimalIncognitoCustomTabIntent();

            // Remove the first party override to emulate third party
            mCustomTabActivityTestRule.setRemoveFirstPartyOverride();

            mCustomTabActivityTestRule.startCustomTabActivityWithIntent(intent);
        }
    }
}
