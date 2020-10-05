// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.translate;

import android.app.assist.AssistContent;
import android.os.Build;

import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.json.JSONException;
import org.json.JSONObject;
import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.batch.BlankCTATabInitialStateRule;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Tests for the translate info included in onProvideAssistContent.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class TranslateAssistContentTest {
    @ClassRule
    public static final ChromeTabbedActivityTestRule sActivityTestRule =
            new ChromeTabbedActivityTestRule();
    @Rule
    public final BlankCTATabInitialStateRule mInitialStateRule =
            new BlankCTATabInitialStateRule(sActivityTestRule, false);

    private static final String TRANSLATE_PAGE = "/chrome/test/data/translate/fr_test.html";
    private static final String NON_TRANSLATE_PAGE = "/chrome/test/data/android/test.html";

    /**
     * Wait until the activity tab is translatable.
     */
    private void waitUntilTranslatable() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Tab tab = sActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());
            boolean canManuallyTranslate = TestThreadUtils.runOnUiThreadBlockingNoException(
                    () -> TranslateBridge.canManuallyTranslate(tab));
            Criteria.checkThat(canManuallyTranslate, Matchers.is(true));
        });
    }

    /**
     * Wait until the activity tab is translated.
     */
    private void waitUntilTranslated() {
        CriteriaHelper.pollInstrumentationThread(() -> {
            Tab tab = sActivityTestRule.getActivity().getActivityTab();
            Criteria.checkThat(tab, Matchers.notNullValue());

            String original = TestThreadUtils.runOnUiThreadBlockingNoException(
                    () -> TranslateBridge.getOriginalLanguage(tab));
            Criteria.checkThat(original, Matchers.notNullValue());

            String current = TestThreadUtils.runOnUiThreadBlockingNoException(
                    () -> TranslateBridge.getCurrentLanguage(tab));
            Criteria.checkThat(current, Matchers.notNullValue());

            Criteria.checkThat(current, Matchers.is(Matchers.not(original)));
        });
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1134812")
    // onProvideAssistContent was first added in the M SDK.
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Features.DisableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentDisabled() throws TimeoutException {
        // Load a page that triggers the translate recommendation.
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);
        waitUntilTranslatable();

        AssistContent assistContent = new AssistContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().onProvideAssistContent(assistContent));
        Assert.assertNull(assistContent.getStructuredData());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1134812")
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentTranslatablePage() throws TimeoutException, JSONException {
        // Load a page that triggers the translate recommendation.
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);
        waitUntilTranslatable();

        AssistContent assistContent = new AssistContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().onProvideAssistContent(assistContent));

        JSONObject structuredData = new JSONObject(assistContent.getStructuredData());
        Assert.assertEquals(structuredData.getString(TranslateAssistContent.TYPE_KEY),
                TranslateAssistContent.TYPE_VALUE);
        Assert.assertEquals(structuredData.getString(TranslateAssistContent.URL_KEY), url);
        Assert.assertEquals(structuredData.getString(TranslateAssistContent.IN_LANGUAGE_KEY), "fr");
        Assert.assertEquals(
                structuredData.getJSONObject(TranslateAssistContent.WORK_TRANSLATION_KEY)
                        .getString(TranslateAssistContent.IN_LANGUAGE_KEY),
                "en");
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1134812")
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentTranslatedPage() throws TimeoutException, JSONException {
        // Load a page that triggers the translate recommendation.
        final String url = sActivityTestRule.getTestServer().getURL(TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);
        waitUntilTranslatable();
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> TranslateBridge.translateTabWhenReady(
                                sActivityTestRule.getActivity().getActivityTab()));
        waitUntilTranslated();

        AssistContent assistContent = new AssistContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().onProvideAssistContent(assistContent));

        JSONObject structuredData = new JSONObject(assistContent.getStructuredData());
        Assert.assertEquals(structuredData.getString(TranslateAssistContent.TYPE_KEY),
                TranslateAssistContent.TYPE_VALUE);
        Assert.assertEquals(structuredData.getString(TranslateAssistContent.URL_KEY), url);
        Assert.assertEquals(structuredData.getString(TranslateAssistContent.IN_LANGUAGE_KEY), "en");
        Assert.assertEquals(
                structuredData.getJSONObject(TranslateAssistContent.TRANSLATION_OF_WORK_KEY)
                        .getString(TranslateAssistContent.IN_LANGUAGE_KEY),
                "fr");
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1134812")
    @MinAndroidSdkLevel(Build.VERSION_CODES.M)
    @Features.EnableFeatures({ChromeFeatureList.TRANSLATE_ASSIST_CONTENT})
    public void testAssistContentNonTranslatePage() throws TimeoutException, JSONException {
        // Load a page that can't be translated.
        final String url = sActivityTestRule.getTestServer().getURL(NON_TRANSLATE_PAGE);
        sActivityTestRule.loadUrl(url);

        AssistContent assistContent = new AssistContent();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> sActivityTestRule.getActivity().onProvideAssistContent(assistContent));
        JSONObject structuredData = new JSONObject(assistContent.getStructuredData());
        Assert.assertEquals(structuredData.getString(TranslateAssistContent.TYPE_KEY),
                TranslateAssistContent.TYPE_VALUE);
        Assert.assertEquals(structuredData.getString(TranslateAssistContent.URL_KEY), url);
        Assert.assertFalse(structuredData.has(TranslateAssistContent.IN_LANGUAGE_KEY));
        Assert.assertFalse(structuredData.has(TranslateAssistContent.TRANSLATION_OF_WORK_KEY));
        Assert.assertFalse(structuredData.has(TranslateAssistContent.WORK_TRANSLATION_KEY));
    }
}
