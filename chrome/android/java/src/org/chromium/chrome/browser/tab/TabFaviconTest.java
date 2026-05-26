// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.anyLong;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.annotation.ColorInt;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.Promise;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabFavicon}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabFaviconTest {
    private static final int IDEAL_SIZE = 4;

    private static class EmptyIterator implements RewindableIterator<TabObserver> {
        @Override
        public boolean hasNext() {
            return false;
        }

        @Override
        public TabObserver next() {
            return null;
        }

        @Override
        public void rewind() {}
    }

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Mock private TabFavicon.Natives mTabFaviconJni;
    @Mock private TabImpl mTab;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private WebContents mWebContents;
    @Mock private FaviconHelper mFaviconHelper;

    private UserDataHost mUserDataHost;
    private TabFavicon mTabFavicon;

    @Before
    public void setUp() {
        TabFaviconJni.setInstanceForTesting(mTabFaviconJni);
        doReturn(12345L).when(mTabFaviconJni).init(any(), anyInt());

        mUserDataHost = new UserDataHost();
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(mContext).when(mTab).getThemedApplicationContext();
        doReturn(mContext).when(mTab).getContext();
        doReturn(mResources).when(mContext).getResources();
        doReturn(IDEAL_SIZE).when(mResources).getDimensionPixelSize(anyInt());
        doReturn(false).when(mTab).isNativePage();
        doReturn(true).when(mTab).isInitialized();
        doReturn(mWebContents).when(mTab).getWebContents();
        doReturn(null).when(mTab).getPendingLoadParams();
        doReturn(JUnitTestGURLs.EXAMPLE_URL).when(mTab).getUrl();
        doReturn(new EmptyIterator()).when(mTab).getTabObservers();

        mTabFavicon = TabFavicon.from(mTab);
    }

    private static Bitmap makeBitmap(int size, @ColorInt int color) {
        return makeBitmap(size, size, color);
    }

    private static Bitmap makeBitmap(int width, int height, @ColorInt int color) {
        Bitmap image = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        image.eraseColor(color);
        return image;
    }

    private void onFaviconAvailable(Bitmap bitmap) {
        // Mimic the behavior of the native call, where `TabFavicon#shouldUpdateFaviconForBrowserUi`
        // is checked first before sending the bitmap into Java layer.
        if (mTabFavicon.shouldUpdateFaviconForBrowserUi(bitmap.getWidth(), bitmap.getHeight())) {
            mTabFavicon.onFaviconAvailable(
                    bitmap, JUnitTestGURLs.EXAMPLE_URL, /* isFallback= */ false);
        }
    }

    @Test
    public void testOnFaviconAvailable_ReturnsBitmap() {
        assertNull(TabFavicon.getBitmap(mTab));

        onFaviconAvailable(makeBitmap(1, Color.GREEN));
        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testOnFaviconAvailable_IdealSize() {
        onFaviconAvailable(makeBitmap(1, Color.RED));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE, Color.GREEN));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE / 2, Color.YELLOW));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE * 2, Color.BLUE));

        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testOnFaviconAvailable_SameSize() {
        // Inspired from https://crbug.com/40234963. Some pages purposefully switch favicons to try
        // to update the current favicon. This will typically result in the same sized favicon being
        // sent. So it's important that we update in this case.
        onFaviconAvailable(makeBitmap(1, Color.RED));
        onFaviconAvailable(makeBitmap(1, Color.GREEN));

        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testOnFaviconAvailable_DensityChangeInvalidatesCache() {
        // 1. Initial favicon at IDEAL_SIZE.
        onFaviconAvailable(makeBitmap(IDEAL_SIZE, Color.GREEN));
        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        assertNotNull(bitmap);
        assertEquals(IDEAL_SIZE, bitmap.getWidth());
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));

        // 2. Change density (ideal size doubles).
        int newIdealSize = IDEAL_SIZE * 2;
        doReturn(newIdealSize).when(mResources).getDimensionPixelSize(anyInt());

        // 3. Requesting bitmap should now bypass cache because cached size (IDEAL_SIZE) !=
        // newIdealSize.
        // It should return whatever native provides (we'll mock native to return a new larger
        // bitmap).
        Bitmap newBitmap = makeBitmap(newIdealSize, Color.BLUE);
        doReturn(newBitmap).when(mTabFaviconJni).getFavicon(anyLong());

        Bitmap result = TabFavicon.getBitmap(mTab);
        assertNotNull(result);
        assertEquals(newIdealSize, result.getWidth());
        assertEquals(Color.BLUE, result.getPixel(0, 0));
    }

    @Test
    public void testGetBitmap_frozenTabWithPendingLoad() {
        // A frozen tab can have a pending load but no WebContents.
        doReturn(null).when(mTab).getWebContents();
        doReturn(new LoadUrlParams("foo.com")).when(mTab).getPendingLoadParams();

        assertNull(TabFavicon.getBitmap(mTab));
    }

    @Test
    public void testGetFavicon_ColdTabCacheOptimization() {
        // Cache a favicon for the tab
        onFaviconAvailable(makeBitmap(IDEAL_SIZE, Color.GREEN));

        // Make WebContents null to simulate a cold tab
        doReturn(null).when(mTab).getWebContents();

        // When URL matches, it should return the cached favicon even if WebContents is null!
        Bitmap bitmap = mTabFavicon.getFavicon(true);
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testNonFallbackWins() {
        // 1. Store a live favicon
        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.GREEN),
                JUnitTestGURLs.EXAMPLE_URL,
                /* isFallback= */ false);
        Bitmap bitmap = mTabFavicon.getFavicon();
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0));

        // 2. Receive a fallback database favicon. Since we already have a live favicon, it should
        // NOT overwrite it!
        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.RED),
                JUnitTestGURLs.EXAMPLE_URL,
                /* isFallback= */ true);
        bitmap = mTabFavicon.getFavicon();
        assertNotNull(bitmap);
        assertEquals(Color.GREEN, bitmap.getPixel(0, 0)); // Still green!
    }

    @Test
    public void testFallbackWinsIfNoCachedIcon() {
        // 1. Store a fallback favicon when cache is empty
        mTabFavicon.onFaviconAvailable(
                makeBitmap(IDEAL_SIZE, Color.RED),
                JUnitTestGURLs.EXAMPLE_URL,
                /* isFallback= */ true);
        Bitmap bitmap = mTabFavicon.getFavicon(true);
        assertNotNull(bitmap);
        assertEquals(Color.RED, bitmap.getPixel(0, 0)); // Red!
    }

    @Test
    public void testGetFaviconOrFallback_AsyncDBFetch() {
        mTabFavicon.mFaviconHelper = mFaviconHelper;
        ArgumentCaptor<FaviconHelper.FaviconImageCallback> callbackCaptor =
                ArgumentCaptor.forClass(FaviconHelper.FaviconImageCallback.class);
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(
                        any(), any(), anyInt(), anyBoolean(), callbackCaptor.capture());

        Promise<Bitmap> promise = mTabFavicon.getFaviconOrFallback();
        assertNotNull(promise);
        assertTrue(promise.isPending());

        Bitmap fallbackBitmap = makeBitmap(IDEAL_SIZE, Color.BLUE);
        callbackCaptor.getValue().onFaviconAvailable(fallbackBitmap, JUnitTestGURLs.EXAMPLE_URL);

        assertTrue(promise.isFulfilled());
        Bitmap result = promise.getResult();
        assertNotNull(result);
        assertEquals(Color.BLUE, result.getPixel(0, 0));
    }

    @Test
    public void testGetFaviconOrFallback_RejectsOnTabDestroyed() {
        mTabFavicon.mFaviconHelper = mFaviconHelper;
        doReturn(true)
                .when(mFaviconHelper)
                .getLocalFaviconImageForURL(any(), any(), anyInt(), anyBoolean(), any());

        Promise<Bitmap> promise1 = mTabFavicon.getFaviconOrFallback();
        Promise<Bitmap> promise2 = mTabFavicon.getFaviconOrFallback();
        assertNotNull(promise1);
        assertNotNull(promise2);
        assertTrue(promise1.isPending());
        assertTrue(promise2.isPending());

        mTabFavicon.destroyInternal();

        assertTrue(promise1.isRejected());
        assertTrue(promise2.isRejected());
    }
}
