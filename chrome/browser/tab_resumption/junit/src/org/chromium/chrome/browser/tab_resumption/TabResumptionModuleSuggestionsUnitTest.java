// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doAnswer;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.Size;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.invocation.InvocationOnMock;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_resumption.UrlImageProvider.UrlImageSource;
import org.chromium.chrome.browser.tab_ui.ThumbnailProvider;
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
    static class FakeLargeIconBridge extends LargeIconBridge {
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
                    pageUrl.equals(mFakeCachedUrl) ? mFakeCachedBitmap : /* tab= */ null,
                    DEFAULT_FALLBACK_COLOR,
                    /*isFallbackColorDefault*/ true,
                    IconType.FAVICON);
            return true;
        }
    }

    @Mock private Profile mProfile;

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

    private static SuggestionEntry createSuggestionEntry(
            String source, GURL url, String title, long time, int id) {
        return new SuggestionEntry(
                SuggestionEntryType.LOCAL_TAB,
                source,
                url,
                title,
                time,
                id,
                null,
                null,
                /* needMatchLocalTab= */ false);
    }

    @Test
    @SmallTest
    public void testCompareSuggestions() {
        SuggestionEntry entry0 =
                createSuggestionEntry(SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_0);
        Assert.assertEquals(
                0,
                entry0.compareTo(
                        createSuggestionEntry(SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_0)));

        // Timestamps dominate source name.
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_LO, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_0, TIMESTAMP_LO, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_0, TIMESTAMP_LO, ID_0))
                        < 0);

        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_HI, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_0, TIMESTAMP_HI, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_0, TIMESTAMP_HI, ID_0))
                        > 0);

        // Source name dominates title.
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_0, TIMESTAMP_0, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_LO, TIMESTAMP_0, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_LO, URL_0, TITLE_HI, TIMESTAMP_0, ID_0))
                        > 0);

        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_0, TIMESTAMP_0, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_LO, TIMESTAMP_0, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_HI, URL_0, TITLE_HI, TIMESTAMP_0, ID_0))
                        < 0);

        // Title dominates id.
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_LO, TIMESTAMP_0, ID_0))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_LO, TIMESTAMP_0, ID_LO))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_LO, TIMESTAMP_0, ID_HI))
                        > 0);

        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_HI, TIMESTAMP_0, ID_0))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_HI, TIMESTAMP_0, ID_LO))
                        < 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_HI, TIMESTAMP_0, ID_HI))
                        < 0);

        // Id as final tie-breaker.
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_LO))
                        > 0);
        Assert.assertTrue(
                entry0.compareTo(
                                createSuggestionEntry(
                                        SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_HI))
                        < 0);

        // URL doesn't matter.
        Assert.assertEquals(
                0,
                entry0.compareTo(
                        createSuggestionEntry(SOURCE_NAME_0, URL_LO, TITLE_0, TIMESTAMP_0, ID_0)));
        Assert.assertEquals(
                0,
                entry0.compareTo(
                        createSuggestionEntry(SOURCE_NAME_0, URL_HI, TITLE_0, TIMESTAMP_0, ID_0)));
    }

    @Test
    @SmallTest
    public void testCompareSuggestionsWithTraingIds() {
        SuggestionEntry entry =
                createSuggestionEntry(SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_0);
        SuggestionEntry entryWithTrainingInfo =
                createSuggestionEntry(SOURCE_NAME_0, URL_0, TITLE_0, TIMESTAMP_0, ID_0);
        entryWithTrainingInfo.trainingInfo =
                new TrainingInfo(
                        /* nativeVisitedUrlRankingBackend= */ 0L,
                        /* visitId= */ "www.google.com",
                        /* requestId= */ 123L);

        // The presence of `trainingInfo` does not affect comparison.
        Assert.assertEquals(0, entry.compareTo(entryWithTrainingInfo));
    }

    @Test
    @SmallTest
    public void testUrlImageProvider() {
        GURL urlWithFavicon = URL_0;
        GURL urlWithoutFavicon = URL_LO;
        Bitmap expectedRealIcon = makeBitmap(64, 64);
        Bitmap expectedThumbnail = makeBitmap(32, 32);
        Bitmap expectedFallbackIcon = makeBitmap(64, 64);
        LargeIconBridge largeIconBridge = new FakeLargeIconBridge(urlWithFavicon, expectedRealIcon);
        ThumbnailProvider thumbnailProvider = Mockito.mock(ThumbnailProvider.class);
        doAnswer(
                        (InvocationOnMock invocation) -> {
                            ((Callback<Drawable>) invocation.getArguments()[3])
                                    .onResult(new BitmapDrawable(expectedThumbnail));
                            return null;
                        })
                .when(thumbnailProvider)
                .getTabThumbnailWithCallback(
                        /* tabId= */ anyInt(),
                        /* thumbnailSize= */ any(Size.class),
                        /* isSelected= */ anyBoolean(),
                        /* callback= */ any(Callback.class));
        RoundedIconGenerator roundedIconGenerator = Mockito.mock(RoundedIconGenerator.class);
        when(roundedIconGenerator.generateIconForUrl(urlWithoutFavicon))
                .thenReturn(expectedFallbackIcon);

        UrlImageSource urlImageSource = Mockito.mock(UrlImageSource.class);
        when(urlImageSource.createThumbnailProvider()).thenReturn(thumbnailProvider);
        when(urlImageSource.createIconGenerator()).thenReturn(roundedIconGenerator);
        Context context = ApplicationProvider.getApplicationContext();
        UrlImageProvider urlImageProvider =
                new UrlImageProvider(context, urlImageSource, null, largeIconBridge);

        urlImageProvider.fetchImageForUrl(
                urlWithFavicon,
                (Bitmap icon) -> {
                    Assert.assertEquals(icon, expectedRealIcon);
                    ++mCallbackCounter;
                });

        Assert.assertEquals(1, mCallbackCounter);

        urlImageProvider.getTabThumbnail(
                /* tabId= */ 0,
                /* thumbnailSize= */ new Size(32, 32),
                /* tabThumbnailCallback= */ (Drawable icon) -> {
                    Assert.assertEquals(((BitmapDrawable) icon).getBitmap(), expectedThumbnail);
                    ++mCallbackCounter;
                });
        Assert.assertEquals(2, mCallbackCounter);

        urlImageProvider.fetchImageForUrl(
                urlWithoutFavicon,
                (Bitmap icon) -> {
                    Assert.assertEquals(icon, expectedFallbackIcon);
                    ++mCallbackCounter;
                });

        Assert.assertEquals(3, mCallbackCounter);

        urlImageProvider.destroy();
        assertNull(urlImageProvider.getImageServiceBridgeForTesting());
        assertNull(urlImageProvider.getLargeIconBridgeForTesting());
        assertTrue(urlImageProvider.isDestroyed());
    }

    @Test
    public void testIsLocalTab() {
        SuggestionEntry entry =
                new SuggestionEntry(
                        SuggestionEntryType.LOCAL_TAB,
                        SOURCE_NAME_0,
                        URL_0,
                        TITLE_0,
                        TIMESTAMP_0,
                        ID_0,
                        null,
                        null,
                        /* needMatchLocalTab= */ false);
        assertTrue(entry.isLocalTab());

        entry =
                new SuggestionEntry(
                        SuggestionEntryType.HISTORY,
                        SOURCE_NAME_0,
                        URL_0,
                        TITLE_0,
                        TIMESTAMP_0,
                        ID_0,
                        null,
                        null,
                        /* needMatchLocalTab= */ false);
        assertTrue(entry.isLocalTab());

        SuggestionEntry invalidEntry =
                new SuggestionEntry(
                        SuggestionEntryType.LOCAL_TAB,
                        SOURCE_NAME_0,
                        URL_0,
                        TITLE_0,
                        TIMESTAMP_0,
                        Tab.INVALID_TAB_ID,
                        null,
                        null,
                        /* needMatchLocalTab= */ false);
        assertFalse(invalidEntry.isLocalTab());
    }
}
