// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.ComposedTabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.ResourceTabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.StaticTabFaviconType;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.UrlTabFavicon;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.ComposedFaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

import java.util.Arrays;

/** Unit tests for {@link TabListFaviconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListFaviconProviderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mJniMocker = new JniMocker();

    @Mock private Profile mProfile;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private FaviconHelper mMockFaviconHelper;

    @Captor private ArgumentCaptor<FaviconImageCallback> mFaviconImageCallbackCaptor;

    @Captor
    private ArgumentCaptor<ComposedFaviconImageCallback> mComposedFaviconImageCallbackCaptor;

    private Activity mActivity;
    private GURL mUrl1;
    private GURL mUrl2;
    private TabListFaviconProvider mTabListFaviconProvider;
    private int mUniqueColorValue;

    private Bitmap newBitmap() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        int colorValue = ++mUniqueColorValue;
        final @ColorInt int colorInt = Color.rgb(colorValue, colorValue, colorValue);
        image.eraseColor(colorInt);
        return image;
    }

    private Drawable newDrawable() {
        Bitmap image = newBitmap();
        Resources resources = ContextUtils.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }

    @Before
    public void setUp() {
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        mJniMocker.mock(FaviconHelperJni.TEST_HOOKS, mFaviconHelperJniMock);
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mUrl1 = JUnitTestGURLs.URL_1;
        mUrl2 = JUnitTestGURLs.URL_2;

        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        mActivity,
                        false,
                        org.chromium.components.browser_ui.styles.R.dimen
                                .default_favicon_corner_radius);
        mTabListFaviconProvider.initWithNative(mProfile);
        mTabListFaviconProvider.setFaviconHelperForTesting(mMockFaviconHelper);
    }

    @Test
    public void testUrlTabFavicon() {
        TabFavicon urlTabFavicon = new UrlTabFavicon(newDrawable(), mUrl1);
        Assert.assertEquals(urlTabFavicon, new UrlTabFavicon(newDrawable(), mUrl1));
        Assert.assertNotEquals(urlTabFavicon, new UrlTabFavicon(newDrawable(), mUrl2));
        Assert.assertNotEquals(urlTabFavicon, null);
        Assert.assertNotEquals(
                urlTabFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE));
    }

    @Test
    public void testComposedTabFavicon() {
        TabFavicon composedTabFavicon = new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1});
        Assert.assertEquals(
                composedTabFavicon, new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1}));
        Assert.assertNotEquals(
                composedTabFavicon,
                new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1, mUrl2}));
        Assert.assertNotEquals(
                composedTabFavicon, new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl2}));
        Assert.assertNotEquals(
                composedTabFavicon, new ComposedTabFavicon(newDrawable(), new GURL[] {}));
        Assert.assertNotEquals(
                composedTabFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE));

        TabFavicon composedTabFavicon2 =
                new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1, mUrl2});
        Assert.assertEquals(
                composedTabFavicon2,
                new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1, mUrl2}));
        Assert.assertNotEquals(
                composedTabFavicon2,
                new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl2, mUrl1}));
        Assert.assertNotEquals(
                composedTabFavicon2, new ComposedTabFavicon(newDrawable(), new GURL[] {mUrl1}));
    }

    @Test
    public void testResourceTabFavicon() {
        TabFavicon resourceTabFavicon =
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE);
        Assert.assertEquals(
                resourceTabFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE));
        Assert.assertNotEquals(
                resourceTabFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_CHROME));
        Assert.assertNotEquals(resourceTabFavicon, new UrlTabFavicon(newDrawable(), mUrl1));
    }

    @Test
    public void testDefaultFaviconFetcher_NonIncognito() {
        TabFaviconFetcher fetcher = mTabListFaviconProvider.getDefaultFaviconFetcher(false);
        TabFavicon favicon = (ResourceTabFavicon) doFetchFavicon(fetcher);
        Assert.assertEquals(
                favicon, new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE));
    }

    @Test
    public void testDefaultFaviconFetcher_Incognito() {
        TabFaviconFetcher fetcher = mTabListFaviconProvider.getDefaultFaviconFetcher(true);
        TabFavicon favicon = (ResourceTabFavicon) doFetchFavicon(fetcher);
        Assert.assertEquals(
                favicon,
                new ResourceTabFavicon(
                        newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE_INCOGNITO));
    }

    @Test
    public void testFaviconFromBitmapFetcher() {
        TabFaviconFetcher fetcher =
                mTabListFaviconProvider.getFaviconFromBitmapFetcher(newBitmap(), mUrl2);
        TabFavicon favicon = (UrlTabFavicon) doFetchFavicon(fetcher);
        Assert.assertEquals(favicon, new UrlTabFavicon(newDrawable(), mUrl2));
    }

    @Test
    public void testFaviconForUrlFetcher() {
        TabFaviconFetcher fetcher = mTabListFaviconProvider.getFaviconForUrlFetcher(mUrl1, false);
        TabFavicon favicon =
                (UrlTabFavicon)
                        doFetchFavicon(
                                () -> {
                                    verify(mMockFaviconHelper)
                                            .getLocalFaviconImageForURL(
                                                    eq(mProfile),
                                                    eq(mUrl1),
                                                    anyInt(),
                                                    mFaviconImageCallbackCaptor.capture());
                                    mFaviconImageCallbackCaptor
                                            .getValue()
                                            .onFaviconAvailable(newBitmap(), mUrl1);
                                },
                                fetcher);
        Assert.assertEquals(favicon, new UrlTabFavicon(newDrawable(), mUrl1));
    }

    @Test
    public void testComposedFaviconImageFetcher() {
        GURL[] urls = new GURL[] {mUrl1, mUrl2};
        TabFaviconFetcher fetcher =
                mTabListFaviconProvider.getComposedFaviconImageFetcher(Arrays.asList(urls), false);
        TabFavicon favicon =
                (ComposedTabFavicon)
                        doFetchFavicon(
                                () -> {
                                    verify(mMockFaviconHelper)
                                            .getComposedFaviconImage(
                                                    eq(mProfile),
                                                    eq(Arrays.asList(urls)),
                                                    anyInt(),
                                                    mComposedFaviconImageCallbackCaptor.capture());
                                    mComposedFaviconImageCallbackCaptor
                                            .getValue()
                                            .onComposedFaviconAvailable(newBitmap(), urls);
                                },
                                fetcher);
        Assert.assertEquals(favicon, new ComposedTabFavicon(newDrawable(), urls));
    }

    private TabFavicon doFetchFavicon(Runnable after, TabFaviconFetcher fetcher) {
        TabFavicon[] faviconHolder = new TabFavicon[1];
        Callback<TabFavicon> callback =
                tabFavicon -> {
                    faviconHolder[0] = tabFavicon;
                };
        fetcher.fetch(callback);
        after.run();
        return faviconHolder[0];
    }

    private TabFavicon doFetchFavicon(TabFaviconFetcher fetcher) {
        return doFetchFavicon(CallbackUtils.emptyRunnable(), fetcher);
    }
}
