// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
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
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.annotation.Config;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
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

    private UserDataHost mUserDataHost;
    private TabFavicon mTabFavicon;

    @Before
    public void setUp() {
        TabFaviconJni.setInstanceForTesting(mTabFaviconJni);

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
            mTabFavicon.onFaviconAvailable(bitmap, JUnitTestGURLs.EXAMPLE_URL);
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
}
