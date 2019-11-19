// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;
import static org.mockito.Mockito.doReturn;

import android.support.test.filters.SmallTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.test.DisableHistogramsRule;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.search_engines.TemplateUrl;
import org.chromium.components.search_engines.TemplateUrlService;

/**
 * Unit tests for {@link SearchEngineChoiceMetrics}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public final class SearchEngineChoiceMetricsTest {
    private static final String TEST_INITIAL_ENGINE = "google.com";
    private static final String TEST_ALTERNATIVE_ENGINE = "duckduckgo.com";

    private static final String HISTOGRAM_AFTER_CHOICE =
            "Android.SearchEngineChoice.ChosenSearchEngine";

    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();
    @Mock
    private TemplateUrlService mTemplateUrlService;
    @Mock
    private TemplateUrl mInitialSearchEngine;
    @Mock
    private TemplateUrl mAlternativeSearchEngine;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        ShadowRecordHistogram.reset();

        // Sets up appropriate responses from Template URL service.
        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);
        doReturn(TEST_ALTERNATIVE_ENGINE).when(mAlternativeSearchEngine).getKeyword();
        doReturn(SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(TEST_ALTERNATIVE_ENGINE);
        doReturn(TEST_INITIAL_ENGINE).when(mInitialSearchEngine).getKeyword();
        doReturn(SearchEngineType.SEARCH_ENGINE_GOOGLE)
                .when(mTemplateUrlService)
                .getSearchEngineTypeFromTemplateUrl(TEST_INITIAL_ENGINE);
        doReturn(mInitialSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
    }

    @Test
    @SmallTest
    public void recordSearchEngineTypeBeforeChoice() {
        doReturn(mInitialSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        SearchEngineChoiceMetrics.recordSearchEngineTypeBeforeChoice();
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.SearchEngineBeforeChoicePrompt",
                        SearchEngineType.SEARCH_ENGINE_GOOGLE));

        doReturn(mAlternativeSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        SearchEngineChoiceMetrics.recordSearchEngineTypeBeforeChoice();
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.SearchEngineBeforeChoicePrompt",
                        SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO));
    }

    @Test
    @SmallTest
    public void recordSearchEngineTypeAfterChoice() {
        doReturn(mInitialSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        SearchEngineChoiceMetrics.setPreviousSearchEngineType(
                SearchEngineChoiceMetrics.getDefaultSearchEngineType());
        doReturn(mAlternativeSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();

        SearchEngineChoiceMetrics.recordSearchEngineTypeAfterChoice();
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_AFTER_CHOICE, SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO));
    }

    @Test
    @SmallTest
    public void recordSearchEngineTypeAfterChoice_noChoice() {
        doReturn(mInitialSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        SearchEngineChoiceMetrics.setPreviousSearchEngineType(
                SearchEngineChoiceMetrics.getDefaultSearchEngineType());

        SearchEngineChoiceMetrics.recordSearchEngineTypeAfterChoice();
        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        HISTOGRAM_AFTER_CHOICE, SearchEngineType.SEARCH_ENGINE_GOOGLE));
        assertEquals(0,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.EventsV2",
                        SearchEngineChoiceMetrics.Events.SEARCH_ENGINE_CHANGED));
    }

    @Test
    @SmallTest
    public void getDefaultSearchEngineType() {
        doReturn(mInitialSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        assertEquals(SearchEngineType.SEARCH_ENGINE_GOOGLE,
                SearchEngineChoiceMetrics.getDefaultSearchEngineType());

        doReturn(mAlternativeSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        assertEquals(SearchEngineType.SEARCH_ENGINE_DUCKDUCKGO,
                SearchEngineChoiceMetrics.getDefaultSearchEngineType());
    }

    @Test
    @SmallTest
    public void isSearchEnginePossiblyDifferent() {
        doReturn(mInitialSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        SearchEngineChoiceMetrics.setPreviousSearchEngineType(
                SearchEngineChoiceMetrics.getDefaultSearchEngineType());
        assertTrue(SearchEngineChoiceMetrics.isSearchEnginePossiblyDifferent());
    }

    @Test
    @SmallTest
    public void isSearchEnginePossiblyDifferent_notDifferent() {
        doReturn(mInitialSearchEngine)
                .when(mTemplateUrlService)
                .getDefaultSearchEngineTemplateUrl();
        assertFalse(SearchEngineChoiceMetrics.isSearchEnginePossiblyDifferent());
    }

    @Test
    @SmallTest
    public void recordEventV2_sanityCheck() {
        SearchEngineChoiceMetrics.recordEventV2(
                SearchEngineChoiceMetrics.EventsV2.CHOICE_REQUEST_VALID);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        "Android.SearchEngineChoice.EventsV2",
                        SearchEngineChoiceMetrics.EventsV2.CHOICE_REQUEST_VALID));
    }
}
