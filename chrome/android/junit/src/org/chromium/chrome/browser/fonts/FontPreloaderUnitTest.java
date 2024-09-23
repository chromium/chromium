// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fonts;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.containsInAnyOrder;
import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Typeface;
import android.os.Handler;
import android.os.SystemClock;

import androidx.core.content.res.ResourcesCompat;
import androidx.core.content.res.ResourcesCompat.FontCallback;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.Implementation;
import org.robolectric.annotation.Implements;
import org.robolectric.annotation.LooperMode;
import org.robolectric.annotation.LooperMode.Mode;
import org.robolectric.annotation.Resetter;
import org.robolectric.shadows.ShadowSystemClock;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.fonts.FontPreloaderUnitTest.ShadowResourcesCompat;

import java.util.ArrayList;
import java.util.List;

/** Unit tests for {@link FontPreloader}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(
        manifest = Config.NONE,
        shadows = {ShadowSystemClock.class, ShadowResourcesCompat.class})
@LooperMode(Mode.PAUSED)
public class FontPreloaderUnitTest {
    private static final Integer[] FONTS = {
        org.chromium.chrome.R.font.chrome_google_sans,
        org.chromium.chrome.R.font.chrome_google_sans_medium,
        org.chromium.chrome.R.font.chrome_google_sans_bold
    };
    private static final String AFTER_ON_CREATE =
            "Android.Fonts.TimeToRetrieveDownloadableFontsAfterOnCreate";
    private static final String AFTER_INFLATION =
            "Android.Fonts.TimeDownloadableFontsRetrievedAfterPostInflationStartup";
    private static final String BEFORE_INFLATION =
            "Android.Fonts.TimeDownloadableFontsRetrievedBeforePostInflationStartup";
    private static final String BEFORE_FIRST_DRAW =
            "Android.Fonts.TimeDownloadableFontsRetrievedBeforeFirstDraw";
    private static final String AFTER_FIRST_DRAW =
            "Android.Fonts.TimeDownloadableFontsRetrievedAfterFirstDraw";
    private static final String FRE = ".FirstRunActivity";
    private static final String TABBED = ".ChromeTabbedActivity";
    private static final String CUSTOM_TAB = ".CustomTabActivity";
    private static final int INITIAL_TIME = 1000;

    @Mock private Context mContext;

    private FontPreloader mFontPreloader;

    @Implements(ResourcesCompat.class)
    static class ShadowResourcesCompat {
        private static final List<Integer> sFontsRequested = new ArrayList<>();
        private static FontCallback sFontCallback;

        @Resetter
        public static void reset() {
            sFontsRequested.clear();
            sFontCallback = null;
        }

        @Implementation
        public static void getFont(
                Context context, int id, FontCallback fontCallback, Handler handler) {
            sFontCallback = fontCallback;
            sFontsRequested.add(id);
        }

        public static void loadFont() {
            sFontCallback.onFontRetrieved(Typeface.DEFAULT);
        }
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        SystemClock.setCurrentTimeMillis(INITIAL_TIME);
        ShadowResourcesCompat.reset();
        when(mContext.getApplicationContext()).thenReturn(mContext);
        mFontPreloader = new FontPreloader(FONTS);
        mFontPreloader.load(mContext);
    }

    @Test
    public void testGetFontCalledForAllFontsInArray() {
        assertThat(ShadowResourcesCompat.sFontsRequested, containsInAnyOrder(FONTS));
    }

    @Test
    public void testAllFontsRetrievedAfterOnPostInflationStartup_FRE() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        mFontPreloader.onPostInflationStartupFre();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 228);
        fakeLoadAllFonts();

        assertHistogramRecorded(AFTER_ON_CREATE, 228);
        assertHistogramRecorded(AFTER_INFLATION + FRE, 128);
        assertHistogramNotRecorded(BEFORE_INFLATION + FRE);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
        assertHistogramNotRecorded(AFTER_INFLATION + TABBED);
        assertHistogramNotRecorded(BEFORE_INFLATION + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_INFLATION + CUSTOM_TAB);
    }

    @Test
    public void testAllFontsRetrievedAfterOnPostInflationStartup_Tabbed() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        mFontPreloader.onPostInflationStartupTabbedActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 200);
        fakeLoadAllFonts();

        assertHistogramRecorded(AFTER_ON_CREATE, 200);
        assertHistogramRecorded(AFTER_INFLATION + TABBED, 100);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
        assertHistogramNotRecorded(BEFORE_INFLATION + FRE);
        assertHistogramNotRecorded(AFTER_INFLATION + FRE);
        assertHistogramNotRecorded(BEFORE_INFLATION + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_INFLATION + CUSTOM_TAB);
    }

    @Test
    public void testAllFontsRetrievedAfterOnPostInflationStartup_CustomTab() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        mFontPreloader.onPostInflationStartupCustomTabActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 150);
        fakeLoadAllFonts();

        assertHistogramRecorded(AFTER_ON_CREATE, 150);
        assertHistogramRecorded(AFTER_INFLATION + CUSTOM_TAB, 50);
        assertHistogramNotRecorded(BEFORE_INFLATION + CUSTOM_TAB);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
        assertHistogramNotRecorded(AFTER_INFLATION + TABBED);
        assertHistogramNotRecorded(BEFORE_INFLATION + FRE);
        assertHistogramNotRecorded(AFTER_INFLATION + FRE);
    }

    @Test
    public void testAllFontsRetrievedBeforeOnPostInflationStartup_FRE() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 150);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 200);
        mFontPreloader.onPostInflationStartupFre();

        assertHistogramRecorded(AFTER_ON_CREATE, 150);
        assertHistogramRecorded(BEFORE_INFLATION + FRE, 50);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
        assertHistogramNotRecorded(AFTER_INFLATION + TABBED);
        assertHistogramNotRecorded(AFTER_INFLATION + FRE);
        assertHistogramNotRecorded(BEFORE_INFLATION + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_INFLATION + CUSTOM_TAB);
    }

    @Test
    public void testAllFontsRetrievedBeforeOnPostInflationStartup_Tabbed() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 5);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 555);
        mFontPreloader.onPostInflationStartupTabbedActivity();

        assertHistogramRecorded(AFTER_ON_CREATE, 5);
        assertHistogramRecorded(BEFORE_INFLATION + TABBED, 550);
        assertHistogramNotRecorded(AFTER_INFLATION + TABBED);
        assertHistogramNotRecorded(BEFORE_INFLATION + FRE);
        assertHistogramNotRecorded(AFTER_INFLATION + FRE);
        assertHistogramNotRecorded(BEFORE_INFLATION + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_INFLATION + CUSTOM_TAB);
    }

    @Test
    public void testAllFontsRetrievedBeforeOnPostInflationStartup_CustomTab() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 123);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 234);
        mFontPreloader.onPostInflationStartupCustomTabActivity();

        assertHistogramRecorded(AFTER_ON_CREATE, 123);
        assertHistogramRecorded(BEFORE_INFLATION + CUSTOM_TAB, 111);
        assertHistogramNotRecorded(AFTER_INFLATION + CUSTOM_TAB);
        assertHistogramNotRecorded(BEFORE_INFLATION + FRE);
        assertHistogramNotRecorded(AFTER_INFLATION + FRE);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
        assertHistogramNotRecorded(AFTER_INFLATION + TABBED);
    }

    @Test
    public void testHistogramsNotRecordedBeforeAllFontsLoaded() {
        ShadowResourcesCompat.loadFont();
        ShadowResourcesCompat.loadFont();

        assertHistogramNotRecorded(AFTER_ON_CREATE);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
        assertHistogramNotRecorded(AFTER_INFLATION + TABBED);
        assertHistogramNotRecorded(BEFORE_INFLATION + FRE);
        assertHistogramNotRecorded(AFTER_INFLATION + FRE);
        assertHistogramNotRecorded(BEFORE_INFLATION + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_INFLATION + CUSTOM_TAB);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + CUSTOM_TAB);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW);

        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 500);
        ShadowResourcesCompat.loadFont();

        assertHistogramRecorded(AFTER_ON_CREATE, 500);
    }

    @Test
    public void testHistogramRecordedForOnlyFirstActivity_BeforeFREInflation() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 10);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 20);
        mFontPreloader.onPostInflationStartupFre();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 30);
        mFontPreloader.onPostInflationStartupTabbedActivity();

        assertHistogramRecorded(BEFORE_INFLATION + FRE, 10);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
    }

    @Test
    public void testHistogramRecordedForOnlyFirstActivity_AfterFREInflation() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 64);
        mFontPreloader.onPostInflationStartupFre();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 96);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 128);
        mFontPreloader.onPostInflationStartupTabbedActivity();

        assertHistogramRecorded(AFTER_INFLATION + FRE, 32);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
    }

    @Test
    public void testHistogramRecordedForOnlyFirstActivity_BeforeCCTInflation() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 1);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 11);
        mFontPreloader.onPostInflationStartupCustomTabActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 111);
        mFontPreloader.onPostInflationStartupTabbedActivity();

        assertHistogramRecorded(BEFORE_INFLATION + CUSTOM_TAB, 10);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
    }

    @Test
    public void testHistogramRecordedForOnlyFirstActivity_AfterCCTInflation() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 32);
        mFontPreloader.onPostInflationStartupCustomTabActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 64);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 128);
        mFontPreloader.onPostInflationStartupTabbedActivity();

        assertHistogramRecorded(AFTER_INFLATION + CUSTOM_TAB, 32);
        assertHistogramNotRecorded(BEFORE_INFLATION + TABBED);
    }

    @Test
    public void testHistogramNotOverEmittedForExtraFontLoads() {
        mFontPreloader.onPostInflationStartupTabbedActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 10);
        fakeLoadAllFonts();

        assertHistogramRecorded(AFTER_ON_CREATE, 10);
        assertHistogramRecorded(AFTER_INFLATION + TABBED, 10);

        // Load an extra font
        ShadowResourcesCompat.loadFont();
        // Still should have only 1 record.
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 10);
        assertHistogramRecorded(AFTER_INFLATION + TABBED, 10);
    }

    @Test
    public void testAllFontsRetrievedAfterFirstDraw_FRE() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        mFontPreloader.onFirstDrawFre();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 228);
        fakeLoadAllFonts();

        assertHistogramRecorded(AFTER_FIRST_DRAW, 128);
        assertHistogramRecorded(AFTER_FIRST_DRAW + FRE, 128);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + CUSTOM_TAB);
    }

    @Test
    public void testAllFontsRetrievedAfterFirstDraw_Tabbed() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        mFontPreloader.onFirstDrawTabbedActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 113);
        fakeLoadAllFonts();

        assertHistogramRecorded(AFTER_FIRST_DRAW, 13);
        assertHistogramRecorded(AFTER_FIRST_DRAW + TABBED, 13);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + CUSTOM_TAB);
    }

    @Test
    public void testAllFontsRetrievedAfterFirstDraw_CustomTab() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 50);
        mFontPreloader.onFirstDrawCustomTabActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        fakeLoadAllFonts();

        assertHistogramRecorded(AFTER_FIRST_DRAW, 50);
        assertHistogramRecorded(AFTER_FIRST_DRAW + CUSTOM_TAB, 50);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + CUSTOM_TAB);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + TABBED);
    }

    @Test
    public void testAllFontsRetrievedBeforeFirstDraw_FRE() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 90);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        mFontPreloader.onFirstDrawFre();

        assertHistogramRecorded(BEFORE_FIRST_DRAW, 10);
        assertHistogramRecorded(BEFORE_FIRST_DRAW + FRE, 10);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + CUSTOM_TAB);
    }

    @Test
    public void testAllFontsRetrievedBeforeFirstDraw_Tabbed() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 123);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 246);
        mFontPreloader.onFirstDrawTabbedActivity();

        assertHistogramRecorded(BEFORE_FIRST_DRAW, 123);
        assertHistogramRecorded(BEFORE_FIRST_DRAW + TABBED, 123);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + CUSTOM_TAB);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + CUSTOM_TAB);
    }

    @Test
    public void testAllFontsRetrievedBeforeFirstDraw_CustomTab() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 119);
        mFontPreloader.onFirstDrawCustomTabActivity();

        assertHistogramRecorded(BEFORE_FIRST_DRAW, 19);
        assertHistogramRecorded(BEFORE_FIRST_DRAW + CUSTOM_TAB, 19);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + CUSTOM_TAB);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + FRE);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + TABBED);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + TABBED);
    }

    @Test
    public void testHistogramRecordedForOnlyFirstActivity_BeforeFREDraw() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 10);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 20);
        mFontPreloader.onFirstDrawFre();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 30);
        mFontPreloader.onFirstDrawTabbedActivity();

        assertHistogramRecorded(BEFORE_FIRST_DRAW + FRE, 10);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + TABBED);
    }

    @Test
    public void testHistogramRecordedForOnlyFirstActivity_AfterFREDraw() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 10);
        mFontPreloader.onFirstDrawFre();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 20);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 30);
        mFontPreloader.onFirstDrawTabbedActivity();

        assertHistogramRecorded(AFTER_FIRST_DRAW + FRE, 10);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + TABBED);
    }

    @Test
    public void testHistogramRecordedForOnlyFirstActivity_BeforeCCTDraw() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 100);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 200);
        mFontPreloader.onFirstDrawCustomTabActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 300);
        mFontPreloader.onFirstDrawTabbedActivity();

        assertHistogramRecorded(BEFORE_FIRST_DRAW + CUSTOM_TAB, 100);
        assertHistogramNotRecorded(BEFORE_FIRST_DRAW + TABBED);
    }

    @Test
    public void testHistogramRecordedForOnlyFirstActivity_AfterCCTDraw() {
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 111);
        mFontPreloader.onFirstDrawCustomTabActivity();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 222);
        fakeLoadAllFonts();
        SystemClock.setCurrentTimeMillis(INITIAL_TIME + 300);
        mFontPreloader.onFirstDrawTabbedActivity();

        assertHistogramRecorded(AFTER_FIRST_DRAW + CUSTOM_TAB, 111);
        assertHistogramNotRecorded(AFTER_FIRST_DRAW + TABBED);
    }

    private void fakeLoadAllFonts() {
        for (int i = 0; i < 3; i++) {
            ShadowResourcesCompat.loadFont();
        }
    }

    /**
     * @param histogram Histogram name to assert.
     * @param expectedValue The expected value to be recorded.
     */
    private void assertHistogramRecorded(String histogram, int expectedValue) {
        assertEquals(
                histogram + " isn't recorded correctly.",
                1,
                RecordHistogram.getHistogramValueCountForTesting(histogram, expectedValue));
    }

    /** @param histogram Histogram name to assert. */
    private void assertHistogramNotRecorded(String histogram) {
        assertEquals(
                histogram + " shouldn't be recorded.",
                0,
                RecordHistogram.getHistogramTotalCountForTesting(histogram));
    }
}
