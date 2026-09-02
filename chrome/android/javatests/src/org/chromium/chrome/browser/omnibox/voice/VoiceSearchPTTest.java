// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.voice;

import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.IntentMatchers.hasAction;

import static org.chromium.base.test.transit.TransitAsserts.assertFinalDestination;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.os.Bundle;
import android.speech.RecognizerIntent;

import androidx.test.espresso.intent.Intents;
import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.AfterClass;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarButtonVariant;
import org.chromium.chrome.browser.toolbar.adaptive.AdaptiveToolbarStatePredictor;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.transit.AutoResetCtaTransitTestRule;
import org.chromium.chrome.test.transit.ChromeTransitTestRules;
import org.chromium.chrome.test.transit.ntp.RegularNewTabPageStation;
import org.chromium.chrome.test.transit.omnibox.OmniboxEnteredTextFacility;
import org.chromium.chrome.test.transit.omnibox.OmniboxFacility;
import org.chromium.chrome.test.transit.page.WebPageStation;
import org.chromium.chrome.test.transit.testhtmls.NavigatePageStations;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.ArrayList;
import java.util.List;

/** Public Transit tests for Voice Search. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public final class VoiceSearchPTTest {
    private static final String VOICE_QUERY = "query";

    @Rule
    public final AutoResetCtaTransitTestRule mCtaTestRule =
            ChromeTransitTestRules.fastAutoResetCtaActivityRule();

    @BeforeClass
    public static void setUpBeforeActivityLaunched() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(AdaptiveToolbarButtonVariant.VOICE);
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(true);
    }

    @AfterClass
    public static void tearDownAfterActivityDestroyed() {
        AdaptiveToolbarStatePredictor.setToolbarStateForTesting(null);
        VoiceRecognitionUtil.setIsVoiceSearchEnabledForTesting(null);
    }

    @Before
    public void setUp() {
        Intents.init();
    }

    @After
    public void tearDown() {
        Intents.release();
    }

    @LargeTest
    @Test
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testNtpVoiceSearch_highConfidence_initiatesSearch() {
        RegularNewTabPageStation ntp = mCtaTestRule.startOnNtp();

        stubSpeechRecognitionResult(VOICE_QUERY, /* isHighConfidence= */ true);
        WebPageStation searchResultsPage = ntp.clickNtpMicToSearchPage(VOICE_QUERY);

        assertFinalDestination(searchResultsPage);
    }

    @LargeTest
    @Test
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testToolbarVoiceSearch_highConfidence_initiatesSearch() {
        String testPageUrl = mCtaTestRule.getTestServer().getURL(NavigatePageStations.PATH_SIMPLE);
        WebPageStation initialPage = mCtaTestRule.startOnWebPage(testPageUrl);

        stubSpeechRecognitionResult(VOICE_QUERY, /* isHighConfidence= */ true);
        WebPageStation searchResultsPage = initialPage.clickToolbarMicToSearchPage(VOICE_QUERY);

        assertFinalDestination(searchResultsPage);
    }

    @LargeTest
    @Test
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testOmniboxVoiceSearch_highConfidence_initiatesSearch() {
        RegularNewTabPageStation ntp = mCtaTestRule.startOnNtp();
        OmniboxFacility omnibox = ntp.openOmnibox();

        stubSpeechRecognitionResult(VOICE_QUERY, /* isHighConfidence= */ true);
        WebPageStation searchResultsPage = clickOmniboxMicToSearchPage(omnibox, ntp, VOICE_QUERY);

        assertFinalDestination(searchResultsPage);
    }

    @LargeTest
    @Test
    @Restriction(DeviceFormFactor.PHONE_OR_TABLET)
    public void testOmniboxVoiceSearch_lowConfidence_entersText() {
        RegularNewTabPageStation ntp = mCtaTestRule.startOnNtp();
        OmniboxFacility omnibox = ntp.openOmnibox();
        stubSpeechRecognitionResult(VOICE_QUERY, /* isHighConfidence= */ false);

        OmniboxEnteredTextFacility enteredText = clickOmniboxMicToEnteredText(omnibox, VOICE_QUERY);

        // Exiting the facility is necessary for batching tests together, as leaving the omnibox
        // focused causes subsequent tests in the batch to fail.
        enteredText.pressBackToExit();

        assertFinalDestination(ntp);
    }

    private static WebPageStation clickOmniboxMicToSearchPage(
            OmniboxFacility omnibox, RegularNewTabPageStation ntp, String query) {
        return omnibox.micButtonElement.clickTo().arriveAt(ntp.createSearchPageStation(query));
    }

    private static OmniboxEnteredTextFacility clickOmniboxMicToEnteredText(
            OmniboxFacility omnibox, String text) {
        return omnibox.micButtonElement
                .clickTo()
                .enterFacility(new OmniboxEnteredTextFacility(omnibox, text));
    }

    private static void stubSpeechRecognitionResult(String query, boolean isHighConfidence) {
        Bundle bundle = new Bundle();
        bundle.putStringArrayList(RecognizerIntent.EXTRA_RESULTS, new ArrayList<>(List.of(query)));
        float confidence =
                isHighConfidence
                        ? 1.0f
                        : VoiceRecognitionHandler.VOICE_SEARCH_CONFIDENCE_NAVIGATE_THRESHOLD
                                - 0.01f;
        bundle.putFloatArray(RecognizerIntent.EXTRA_CONFIDENCE_SCORES, new float[] {confidence});

        Intent resultIntent = new Intent();
        resultIntent.putExtras(bundle);
        ActivityResult result = new ActivityResult(Activity.RESULT_OK, resultIntent);

        intending(hasAction(RecognizerIntent.ACTION_RECOGNIZE_SPEECH)).respondWith(result);
    }
}
