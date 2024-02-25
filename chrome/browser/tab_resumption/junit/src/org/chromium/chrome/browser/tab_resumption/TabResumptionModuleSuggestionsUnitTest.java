// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab_resumption.UrlImageProvider.UrlImageSource;
import org.chromium.components.browser_ui.widget.RoundedIconGenerator;
import org.chromium.components.favicon.IconType;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabResumptionModuleSuggestionsUnitTest extends TestSupport {

    // Fake LargeIconBridge, which is explicitly faked for testing since it uses
    // a callback to pass results.
    class FakeLargeIconBridge extends LargeIconBridge {
        private static final int DEFAULT_FALLBACK_COLOR = 0xff0000ff;

        GURL mFakeCachedUrl;
        Bitmap mFakeCachedBitmap;

        FakeLargeIconBridge(GURL fakeCachedUrl, Bitmap fakeCachedBitmap) {
            mFakeCachedUrl = fakeCachedUrl;
            mFakeCachedBitmap = fakeCachedBitmap;
        }

        @Override
        public boolean getLargeIconForUrl(
                final GURL pageUrl,
                int minSizePx,
                int desiredSizePx,
                final LargeIconCallback callback) {
            // For simplicity, fake with a synchronous call.
            callback.onLargeIconAvailable(
                    pageUrl.equals(mFakeCachedUrl) ? mFakeCachedBitmap : null,
                    DEFAULT_FALLBACK_COLOR,
                    /*isFallbackColorDefault*/ true,
                    IconType.FAVICON);
            return true;
        }
    }

    // Various test value that satisfy FOO_LO < FOO_0 < FOO_HI.
    private static final String SOURCE_NAME_LO = "Desktop";
    private static final String SOURCE_NAME_0 = "My Phone";
    private static final String SOURCE_NAME_HI = "Phone 2";
    private static final GURL URL_LO = JUnitTestGURLs.BLUE_1;
    private static final GURL URL_0 = JUnitTestGURLs.GOOGLE_URL_DOG;
    private static final GURL URL_HI = JUnitTestGURLs.RED_2;
    private static final String TITLE_LO = "Blue 1";
    private static final String TITLE_0 = "Google";
    private static final String TITLE_HI = "Red 2";
    private static final long TIMESTAMP_LO = makeTimestamp(3, 2, 1);
    private static final long TIMESTAMP_0 = makeTimestamp(5, 5, 5);
    private static final long TIMESTAMP_HI = makeTimestamp(7, 8, 9);
    private static final int ID_LO = 1;
    private static final int ID_0 = 5;
    private static final int ID_HI = 10;

    private int mCallbackCounter;

    @Before
    public void setUp() {}

    @After
    public void tearDown() {}

    @Test
    @SmallTest
    public void testAssignSuggestions() {
        SuggestionEntry entry0 =
                new SuggestionEntry(SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_0);
        SuggestionEntry entryLo =
                new SuggestionEntry(SOURCE_NAME_LO, URL_LO, TITLE_LO, TIMESTAMP_LO, ID_LO);
        SuggestionBundle bundle = new SuggestionBundle(TIMESTAMP_HI);
        Assert.assertEquals(TIMESTAMP_HI, bundle.referenceTimeMs);
        bundle.entries.add(entry0);
        bundle.entries.add(entryLo);
        Assert.assertEquals(2, bundle.entries.size());

        Resources res = ApplicationProvider.getApplicationContext().getResources();
        Drawable drawable = new BitmapDrawable(res, makeBitmap(1, 1));

        entry0.setUrlDrawable(drawable);
        Assert.assertEquals(drawable, entry0.getUrlDrawable());
    }

    @Test
    @SmallTest
    public void testCompareSuggestions() {
        SuggestionEntry entry0 =
                new SuggestionEntry(SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_0);
        Assert.assertEquals(
                0,
                entry0.compareTo(
                        new SuggestionEntry(SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_0)));

        // Timestamps dominate source name.
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_LO, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_0, TIMESTAMP_LO, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_0, TIMESTAMP_LO, ID_0))
                        < 0);

        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_HI, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_0, TIMESTAMP_HI, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_0, TIMESTAMP_HI, ID_0))
                        > 0);

        // Source name dominates title.
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_0, TIMESTAMP_0, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_LO, TIMESTAMP_0, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_HI, TIMESTAMP_0, ID_0))
                        > 0);

        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_0, TIMESTAMP_0, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_LO, TIMESTAMP_0, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_HI, TIMESTAMP_0, ID_0))
                        < 0);

        // Title dominates id.
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_LO, TIMESTAMP_0, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_LO, TIMESTAMP_0, ID_LO))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_LO, TIMESTAMP_0, ID_HI))
                        > 0);

        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_HI, TIMESTAMP_0, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_HI, TIMESTAMP_0, ID_LO))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_HI, TIMESTAMP_0, ID_HI))
                        < 0);

        // Id as final tie-breaker.
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_LO))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                new SuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_HI))
                        < 0);

        // URL doesn't matter.
        Assert.assertEquals(
                0,
                entry0.compareTo(
                        new SuggestionEntry(SOURCE_NAME_0, URL_LO, TITLE_0, TIMESTAMP_0, ID_0)));
        Assert.assertEquals(
                0,
                entry0.compareTo(
                        new SuggestionEntry(SOURCE_NAME_0, URL_HI, TITLE_0, TIMESTAMP_0, ID_0)));
    }

    @Test
    @SmallTest
    public void testUrlImageProvider() {
        GURL urlWithFavicon = URL_0;
        GURL urlWithoutFavicon = URL_LO;
        Bitmap expectedRealIcon = makeBitmap(64, 64);
        Bitmap expectedFallbackIcon = makeBitmap(64, 64);
        LargeIconBridge largeIconBridge = new FakeLargeIconBridge(urlWithFavicon, expectedRealIcon);
        RoundedIconGenerator roundedIconGenerator = Mockito.mock(RoundedIconGenerator.class);
        when(roundedIconGenerator.generateIconForUrl(urlWithoutFavicon))
                .thenReturn(expectedFallbackIcon);

        UrlImageSource urlImageSource = Mockito.mock(UrlImageSource.class);
        when(urlImageSource.createLargeIconBridge()).thenReturn(largeIconBridge);
        when(urlImageSource.createIconGenerator()).thenReturn(roundedIconGenerator);
        Context context = ApplicationProvider.getApplicationContext();
        UrlImageProvider urlImageProvider = new UrlImageProvider(urlImageSource, context);

        urlImageProvider.fetchImageForUrl(
                urlWithFavicon,
                (Bitmap icon) -> {
                    Assert.assertEquals(icon, expectedRealIcon);
                    ++mCallbackCounter;
                });

        urlImageProvider.fetchImageForUrl(
                urlWithoutFavicon,
                (Bitmap icon) -> {
                    Assert.assertEquals(icon, expectedFallbackIcon);
                    ++mCallbackCounter;
                });

        Assert.assertEquals(2, mCallbackCounter);
    }
}
