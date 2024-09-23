// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.Mockito.doReturn;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;

import androidx.annotation.ColorInt;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.base.UserDataHost;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
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

    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private TabFavicon.Natives mTabFaviconJni;
    @Mock private TabImpl mTab;
    @Mock private Context mContext;
    @Mock private Resources mResources;
    @Mock private WebContents mWebContents;

    private UserDataHost mUserDataHost;
    private TabFavicon mTabFavicon;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mJniMocker.mock(TabFaviconJni.TEST_HOOKS, mTabFaviconJni);

        mUserDataHost = new UserDataHost();
        doReturn(mUserDataHost).when(mTab).getUserDataHost();
        doReturn(mContext).when(mTab).getThemedApplicationContext();
        doReturn(mResources).when(mContext).getResources();
        doReturn(IDEAL_SIZE).when(mResources).getDimensionPixelSize(anyInt());
        doReturn(false).when(mTab).isNativePage();
        doReturn(true).when(mTab).isInitialized();
        doReturn(mWebContents).when(mTab).getWebContents();
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
        // Mimic the behavior of the native call, where `TabFavicon#shouldUpdateFaviconForBrowserUI`
        // is checked first before sending the bitmap into Java layer.
        if (mTabFavicon.shouldUpdateFaviconForBrowserUI(bitmap.getWidth(), bitmap.getHeight())) {
            mTabFavicon.onFaviconAvailable(bitmap, JUnitTestGURLs.EXAMPLE_URL);
        }
    }

    @Test
    public void testOnFaviconAvailable_ReturnsBitmap() {
        Assert.assertNull(TabFavicon.getBitmap(mTab));

        onFaviconAvailable(makeBitmap(1, Color.GREEN));
        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        Assert.assertNotNull(bitmap);
        Assert.assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testOnFaviconAvailable_IdealSize() {
        onFaviconAvailable(makeBitmap(1, Color.RED));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE, Color.GREEN));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE / 2, Color.YELLOW));
        onFaviconAvailable(makeBitmap(IDEAL_SIZE * 2, Color.BLUE));

        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        Assert.assertNotNull(bitmap);
        Assert.assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }

    @Test
    public void testOnFaviconAvailable_SameSize() {
        // Inspired from https://crbug.com/1352674. Some pages purposefully switch favicons to try
        // to update the current favicon. This will typically result in the same sized favicon being
        // sent. So it's important that we update in this case.
        onFaviconAvailable(makeBitmap(1, Color.RED));
        onFaviconAvailable(makeBitmap(1, Color.GREEN));

        Bitmap bitmap = TabFavicon.getBitmap(mTab);
        Assert.assertNotNull(bitmap);
        Assert.assertEquals(Color.GREEN, bitmap.getPixel(0, 0));
    }
}
