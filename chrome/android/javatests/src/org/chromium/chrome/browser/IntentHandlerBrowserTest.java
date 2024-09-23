// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import static androidx.test.espresso.matcher.ViewMatchers.assertThat;

import static org.hamcrest.core.StringStartsWith.startsWith;

import android.content.Intent;
import android.speech.RecognizerResultsIntent;

import androidx.test.annotation.UiThreadTest;
import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.ClassRule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.url.JUnitTestGURLs;

import java.util.ArrayList;
import java.util.Collections;

/** Tests for IntentHandler that require Browser initialization. */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class IntentHandlerBrowserTest {
    @ClassRule public static final ChromeBrowserTestRule sRule = new ChromeBrowserTestRule();

    private static final String VOICE_SEARCH_QUERY = "VOICE_QUERY";
    private static final String VOICE_SEARCH_QUERY_URL =
            "https://www.google.com/search?q=VOICE_QUERY";

    private static final String VOICE_URL_QUERY = "www.google.com";
    private static final String VOICE_URL_QUERY_URL = "INVALID_URLZ";

    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testGetQueryFromVoiceSearchResultIntent_validVoiceQuery() {
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        intent.putStringArrayListExtra(
                RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS,
                new ArrayList<>(Collections.singletonList(VOICE_SEARCH_QUERY)));
        intent.putStringArrayListExtra(
                RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS,
                new ArrayList<>(Collections.singletonList(VOICE_SEARCH_QUERY_URL)));
        String query = IntentHandler.getUrlFromVoiceSearchResult(intent);
        assertThat(query, startsWith(VOICE_SEARCH_QUERY_URL));
    }

    @Test
    @MediumTest
    @UiThreadTest
    @Feature({"Android-AppBase"})
    public void testGetQueryFromVoiceSearchResultIntent_validUrlQuery() {
        Intent intent = new Intent(RecognizerResultsIntent.ACTION_VOICE_SEARCH_RESULTS);
        intent.putStringArrayListExtra(
                RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_STRINGS,
                new ArrayList<>(Collections.singletonList(VOICE_URL_QUERY)));
        intent.putStringArrayListExtra(
                RecognizerResultsIntent.EXTRA_VOICE_SEARCH_RESULT_URLS,
                new ArrayList<>(Collections.singletonList(VOICE_URL_QUERY_URL)));
        String query = IntentHandler.getUrlFromVoiceSearchResult(intent);
        Assert.assertTrue(
                String.format(
                        "Expected qualified URL: %s, to start " + "with http://www.google.com",
                        query),
                query.indexOf("http://www.google.com") == 0);
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testGetURLFromShareIntent_validURL() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.putExtra(Intent.EXTRA_TEXT, JUnitTestGURLs.EXAMPLE_URL.getSpec());
        intent.setType("text/plain");
        String url = IntentHandler.getUrlFromShareIntent(intent);
        Assert.assertEquals(url, JUnitTestGURLs.EXAMPLE_URL.getSpec());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testGetURLFromShareIntent_Url() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.putExtra(Intent.EXTRA_TEXT, "www.google.com");
        intent.setType("text/plain");
        String url = IntentHandler.getUrlFromShareIntent(intent);
        Assert.assertEquals(url, JUnitTestGURLs.GOOGLE_URL.getSpec());
    }

    @Test
    @MediumTest
    @UiThreadTest
    public void testGetURLFromShareIntent_plainText() {
        Intent intent = new Intent(Intent.ACTION_SEND);
        intent.putExtra(Intent.EXTRA_TEXT, "test");
        intent.setType("text/plain");
        String url = IntentHandler.getUrlFromShareIntent(intent);
        assertThat(url, startsWith(JUnitTestGURLs.SEARCH_URL.getSpec()));
    }
}
