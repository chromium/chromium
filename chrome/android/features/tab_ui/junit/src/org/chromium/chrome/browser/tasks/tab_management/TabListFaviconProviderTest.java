// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.support.annotation.ColorInt;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.ComposedTabFavicon;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.ResourceTabFavicon;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.StaticTabFaviconType;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tasks.tab_management.TabListFaviconProvider.UrlTabFavicon;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabListFaviconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListFaviconProviderTest {
    private GURL mUrl1;
    private GURL mUrl2;

    private int mUniqueColorValue;

    private Drawable newDrawable() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        int colorValue = ++mUniqueColorValue;
        final @ColorInt int colorInt = Color.rgb(colorValue, colorValue, colorValue);
        image.eraseColor(colorInt);
        Resources resources = ContextUtils.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }

    @Before
    public void setUp() {
        mUrl1 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_1);
        mUrl2 = JUnitTestGURLs.getGURL(JUnitTestGURLs.URL_2);
    }

    @Test
    public void testUrlTabFavicon() {
        TabFavicon urlTabFavicon = new UrlTabFavicon(newDrawable(), mUrl1);
        Assert.assertEquals(urlTabFavicon, new UrlTabFavicon(newDrawable(), mUrl1));
        Assert.assertNotEquals(urlTabFavicon, new UrlTabFavicon(newDrawable(), mUrl2));
        Assert.assertNotEquals(urlTabFavicon, null);
        Assert.assertNotEquals(urlTabFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE));
    }

    @Test
    public void testComposedTabFavicon() {
        TabFavicon composedTabFavicon = new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1});
        Assert.assertEquals(
                composedTabFavicon, new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1}));
        Assert.assertNotEquals(composedTabFavicon,
                new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1, mUrl2}));
        Assert.assertNotEquals(
                composedTabFavicon, new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl2}));
        Assert.assertNotEquals(
                composedTabFavicon, new ComposedTabFavicon(newDrawable(), new GURL[] {}));
        Assert.assertNotEquals(composedTabFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE));

        TabFavicon composedTabFavicon2 =
                new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1, mUrl2});
        Assert.assertEquals(composedTabFavicon2,
                new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1, mUrl2}));
        Assert.assertNotEquals(composedTabFavicon2,
                new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl2, mUrl1}));
        Assert.assertNotEquals(
                composedTabFavicon2, new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1}));
    }

    @Test
    public void testResourceTabFavicon() {
        TabFavicon resourceTabFavicon =
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE);
        Assert.assertEquals(resourceTabFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE));
        Assert.assertNotEquals(resourceTabFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_CHROME));
        Assert.assertNotEquals(resourceTabFavicon, new UrlTabFavicon(newDrawable(), mUrl1));
    }
}