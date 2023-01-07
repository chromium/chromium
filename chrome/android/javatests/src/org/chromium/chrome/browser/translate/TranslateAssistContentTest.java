// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.TranslateUtil;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.TimeoutException;

/**
 * Tests for the translate info included in onProvideAssistContent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(TranslateAssistContentTest.TRANSLATE_BATCH_NAME)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class TranslateAssistContentTest {
    public static final String TRANSLATE_BATCH_NAME = "translate_batch_name";

    @ClassRule
    public static ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule
    public BlankCTATabInitialStateRule mBlankCTATabInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";
    private static final String NON_TRANSLATE_PAGE = "/chrome/test/data/android/test.html";

    /**
     * Returns true if a test that requires internet access should be skipped due to an
     * out-of-process NetworkService. When the NetworkService is run out-of-process, a fake DNS
     * resolver is used that will fail to resolve any non-local names. crbug.com/1134812 is tracking
     * the changes to make the translate service mockable and remove the internet requirement.
     */
    private boolean shouldSkipDueToNetworkService() {
        return !ChromeFeatureList.isEnabled("NetworkServiceInProcess2");
    }

    @Before
    public void setUp() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> TranslateBridge.setIgnoreMissingKeyForTesting(true));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentDisabled() throws TimeoutException, ExecutionException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that triggers the translate recommendation.
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitUntilTranslatable(sActivityTestRule.getActivity().getActivityTab());

        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                sActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/false));
        Assert.assertNull(structuredData);
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_TFLITE})
    public void testAssistContentTranslatablePage()
            throws TimeoutException, ExecutionException, JSONException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that triggers the translate recommendation.
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitUntilTranslatable(sActivityTestRule.getActivity().getActivityTab());

        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                sActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/false));

        JSONObject parsed = new JSONObject(structuredData);
        Assert.assertEquals(TranslateAssistContent.TYPE_VALUE,
                parsed.getString(TranslateAssistContent.TYPE_KEY));
        Assert.assertEquals(url, parsed.getString(TranslateAssistContent.URL_KEY));
        Assert.assertEquals("fr", parsed.getString(TranslateAssistContent.IN_LANGUAGE_KEY));
        Assert.assertEquals("en",
                parsed.getJSONObject(TranslateAssistContent.WORK_TRANSLATION_KEY)
                        .getString(TranslateAssistContent.IN_LANGUAGE_KEY));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_TFLITE})
    public void testAssistContentTranslatedPage()
            throws TimeoutException, ExecutionException, JSONException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that triggers the translate recommendation.
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitUntilTranslatable(sActivityTestRule.getActivity().getActivityTab());
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateBridge.translateTabWhenReady(
                                sActivityTestRule.getActivity().getActivityTab()));

        // Can't wait on the Translate infobar state here because the target language tab is
        // selected before the translation is complete. Wait for the language to change instead.
        CriteriaHelper.pollUiThread(() -> {
            Criteria.checkThat(TranslateBridge.getCurrentLanguage(
                                       sActivityTestRule.getActivity().getActivityTab()),
                    Matchers.is("en"));
        });

        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                sActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/false));

        JSONObject parsed = new JSONObject(structuredData);
        Assert.assertEquals(TranslateAssistContent.TYPE_VALUE,
                parsed.getString(TranslateAssistContent.TYPE_KEY));
        Assert.assertEquals(url, parsed.getString(TranslateAssistContent.URL_KEY));
        Assert.assertEquals("en", parsed.getString(TranslateAssistContent.IN_LANGUAGE_KEY));
        Assert.assertEquals("fr",
                parsed.getJSONObject(TranslateAssistContent.TRANSLATION_OF_WORK_KEY)
                        .getString(TranslateAssistContent.IN_LANGUAGE_KEY));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    @DisabledTest(message = "https://crbug.com/1189385")
    public void testAssistContentNonTranslatePage()
            throws TimeoutException, ExecutionException, JSONException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that can't be translated.
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);

        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                sActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/false));

        JSONObject parsed = new JSONObject(structuredData);
        Assert.assertEquals(TranslateAssistContent.TYPE_VALUE,
                parsed.getString(TranslateAssistContent.TYPE_KEY));
        Assert.assertEquals(url, parsed.getString(TranslateAssistContent.URL_KEY));
        Assert.assertFalse(parsed.has(TranslateAssistContent.IN_LANGUAGE_KEY));
        Assert.assertFalse(parsed.has(TranslateAssistContent.TRANSLATION_OF_WORK_KEY));
        Assert.assertFalse(parsed.has(TranslateAssistContent.WORK_TRANSLATION_KEY));
    }

    @Test
    @MediumTest
    @Restriction(Restriction.RESTRICTION_TYPE_INTERNET)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentOverviewMode() throws TimeoutException, ExecutionException {
        if (shouldSkipDueToNetworkService()) return;
        // Load a page that triggers the translate recommendation.
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);
        TranslateUtil.waitUntilTranslatable(sActivityTestRule.getActivity().getActivityTab());

        // Pretend we're in overview mode.
        String structuredData = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateAssistContent.getTranslateDataForTab(
                                sActivityTestRule.getActivity().getActivityTab(),
                                /*isInOverviewMode=*/true));
        Assert.assertNull(structuredData);
    }
}
