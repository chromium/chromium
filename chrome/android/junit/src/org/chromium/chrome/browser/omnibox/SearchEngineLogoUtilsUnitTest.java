// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.shadow.api.Shadow;

import org.chromium.base.Callback;
import org.chromium.base.metrics.test.DisableHistogramsRule;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.favicon.FaviconHelper;
import org.chromium.chrome.browser.omnibox.SearchEngineLogoUtils.Delegate;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.search_engines.TemplateUrlServiceFactory;
import org.chromium.chrome.browser.ui.widget.RoundedIconGenerator;
import org.chromium.components.search_engines.TemplateUrlService;

/**
 * Tests for SearchEngineLogoUtils.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class SearchEngineLogoUtilsUnitTest {
    private static final String LOGO_URL = "http://testlogo.com";
    private static final String EVENTS_HISTOGRAM = "AndroidSearchEngineLogo.Events";

    @Rule
    public DisableHistogramsRule mDisableHistogramsRule = new DisableHistogramsRule();

    @Captor
    ArgumentCaptor<FaviconHelper.FaviconImageCallback> mCallbackCaptor;
    @Mock
    Callback<Bitmap> mCallback;
    @Mock
    Delegate mDelegate;
    @Mock
    FaviconHelper mFaviconHelper;
    @Mock
    TemplateUrlService mTemplateUrlService;
    @Mock
    RoundedIconGenerator mRoundedIconGenerator;

    Bitmap mBitmap;

    @Before
    public void setUp() {
        ShadowRecordHistogram.reset();
        MockitoAnnotations.initMocks(this);

        mBitmap = Shadow.newInstanceOf(Bitmap.class);
        shadowOf(mBitmap).appendDescription("test");

        TemplateUrlServiceFactory.setInstanceForTesting(mTemplateUrlService);

        SearchEngineLogoUtils.resetCacheForTesting();
        SearchEngineLogoUtils.setDelegateForTesting(mDelegate);
        SearchEngineLogoUtils.setFaviconHelperForTesting(mFaviconHelper);
        SearchEngineLogoUtils.setRoundedIconGeneratorForTesting(mRoundedIconGenerator);

        when(mRoundedIconGenerator.generateIconForText(any())).thenReturn(mBitmap);
        when(mDelegate.isSearchEngineLogoEnabled()).thenReturn(true);
        when(mDelegate.shouldShowSearchEngineLogo(false)).thenReturn(true);
        when(mDelegate.shouldShowRoundedSearchEngineLogo(false)).thenReturn(true);
        when(mTemplateUrlService.getUrlForSearchQuery(any())).thenReturn(LOGO_URL);
        when(mFaviconHelper.getLocalFaviconImageForURL(
                     any(), any(), anyInt(), mCallbackCaptor.capture()))
                .thenReturn(true);
    }

    @Test
    public void recordEvent() {
        SearchEngineLogoUtils.recordEvent(
                SearchEngineLogoUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(EVENTS_HISTOGRAM,
                        SearchEngineLogoUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
    }

    @Test
    public void getSearchEngineLogoFavicon() {
        SearchEngineLogoUtils.getSearchEngineLogoFavicon(
                Mockito.mock(Profile.class), Mockito.mock(Resources.class), mCallback);
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        assertNotNull(faviconCallback);
        faviconCallback.onFaviconAvailable(mBitmap, LOGO_URL);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(EVENTS_HISTOGRAM,
                        SearchEngineLogoUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineLogoUtils.Events.FETCH_SUCCESS));
    }

    @Test
    public void getSearchEngineLogoFavicon_faviconCached() {
        SearchEngineLogoUtils.getSearchEngineLogoFavicon(
                Mockito.mock(Profile.class), Mockito.mock(Resources.class), mCallback);
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        assertNotNull(faviconCallback);
        faviconCallback.onFaviconAvailable(mBitmap, LOGO_URL);
        SearchEngineLogoUtils.getSearchEngineLogoFavicon(
                Mockito.mock(Profile.class), Mockito.mock(Resources.class), mCallback);
        assertEquals(2,
                ShadowRecordHistogram.getHistogramValueCountForTesting(EVENTS_HISTOGRAM,
                        SearchEngineLogoUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineLogoUtils.Events.FETCH_SUCCESS));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineLogoUtils.Events.FETCH_SUCCESS_CACHE_HIT));
    }

    @Test
    public void getSearchEngineLogoFavicon_nullUrl() {
        doReturn(null).when(mTemplateUrlService).getUrlForSearchQuery(any());
        SearchEngineLogoUtils.getSearchEngineLogoFavicon(
                Mockito.mock(Profile.class), Mockito.mock(Resources.class), mCallback);
        verify(mCallback).onResult(null);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(EVENTS_HISTOGRAM,
                        SearchEngineLogoUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(
                        EVENTS_HISTOGRAM, SearchEngineLogoUtils.Events.FETCH_FAILED_NULL_URL));
    }

    @Test
    public void getSearchEngineLogoFavicon_faviconHelperError() {
        when(mFaviconHelper.getLocalFaviconImageForURL(
                     any(), any(), anyInt(), mCallbackCaptor.capture()))
                .thenReturn(false);

        SearchEngineLogoUtils.getSearchEngineLogoFavicon(
                Mockito.mock(Profile.class), Mockito.mock(Resources.class), mCallback);
        verify(mCallback).onResult(null);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(EVENTS_HISTOGRAM,
                        SearchEngineLogoUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(EVENTS_HISTOGRAM,
                        SearchEngineLogoUtils.Events.FETCH_FAILED_FAVICON_HELPER_ERROR));
    }

    @Test
    public void getSearchEngineLogoFavicon_returnedBitmapNull() {
        SearchEngineLogoUtils.getSearchEngineLogoFavicon(
                Mockito.mock(Profile.class), Mockito.mock(Resources.class), mCallback);
        FaviconHelper.FaviconImageCallback faviconCallback = mCallbackCaptor.getValue();
        assertNotNull(faviconCallback);
        faviconCallback.onFaviconAvailable(null, LOGO_URL);
        verify(mCallback).onResult(null);
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(EVENTS_HISTOGRAM,
                        SearchEngineLogoUtils.Events.FETCH_NON_GOOGLE_LOGO_REQUEST));
        assertEquals(1,
                ShadowRecordHistogram.getHistogramValueCountForTesting(EVENTS_HISTOGRAM,
                        SearchEngineLogoUtils.Events.FETCH_FAILED_RETURNED_BITMAP_NULL));
    }

    @Test
    public void getMostCommonEdgeColor_allOneColor() {
        int color = Color.BLUE;
        Bitmap bitmap = createSolidImage(100, 100, color);
        assertEquals(color, SearchEngineLogoUtils.getMostCommonEdgeColor(bitmap));
    }

    @Test
    public void getMostCommonEdgeColor_outerInnerColor() {
        int color = Color.BLUE;
        Bitmap bitmap = createSolidImageWithDifferentInnerColor(100, 100, color, Color.RED);
        assertEquals(color, SearchEngineLogoUtils.getMostCommonEdgeColor(bitmap));
    }

    @Test
    public void getMostCommonEdgeColor_slightlyLargerColor() {
        int color = Color.BLUE;
        Bitmap bitmap = createSolidImageWithSlighlyLargerEdgeCoverage(100, 100, color, Color.RED);
        assertEquals(color, SearchEngineLogoUtils.getMostCommonEdgeColor(bitmap));
    }

    private static Bitmap createSolidImage(int width, int height, int color) {
        Bitmap bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        for (int x = 0; x < bitmap.getWidth(); x++) {
            for (int y = 0; y < bitmap.getHeight(); y++) {
                bitmap.setPixel(x, y, color);
            }
        }
        return bitmap;
    }

    private static Bitmap createSolidImageWithDifferentInnerColor(
            int width, int height, int outerColor, int innerColor) {
        Bitmap bitmap = createSolidImage(width, height, outerColor);
        for (int x = 1; x < bitmap.getWidth() - 1; x++) {
            for (int y = 1; y < bitmap.getHeight() - 1; y++) {
                bitmap.setPixel(x, y, innerColor);
            }
        }
        return bitmap;
    }

    private static Bitmap createSolidImageWithSlighlyLargerEdgeCoverage(
            int width, int height, int largerColor, int smallerColor) {
        Bitmap bitmap = createSolidImage(width, height, largerColor);
        for (int x = 0; x < bitmap.getWidth(); x++) {
            for (int y = bitmap.getHeight() + 1; y < bitmap.getHeight(); y++) {
                bitmap.setPixel(x, y, smallerColor);
            }
        }
        return bitmap;
    }
}
