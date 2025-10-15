// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;

import androidx.annotation.ColorInt;

import org.junit.After;
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

import org.chromium.base.CallbackUtils;
import org.chromium.base.ContextUtils;
import org.chromium.base.Holder;
import org.chromium.base.Token;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.ResourceTabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.StaticTabFaviconType;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabWebContentsFaviconDelegate;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.UrlTabFavicon;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper;
import org.chromium.chrome.browser.ui.favicon.FaviconHelper.FaviconImageCallback;
import org.chromium.chrome.browser.ui.favicon.FaviconHelperJni;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

/** Unit tests for {@link TabListFaviconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabListFaviconProviderTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private Profile mOtrProfile;
    @Mock private FaviconHelper.Natives mFaviconHelperJniMock;
    @Mock private FaviconHelper mMockFaviconHelper;
    @Mock private TabWebContentsFaviconDelegate mTabWebContentsFaviconDelegate;

    @Captor private ArgumentCaptor<FaviconImageCallback> mFaviconImageCallbackCaptor;

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
        when(mProfile.getPrimaryOtrProfile(anyBoolean())).thenReturn(mOtrProfile);
        when(mFaviconHelperJniMock.init()).thenReturn(1L);
        FaviconHelperJni.setInstanceForTesting(mFaviconHelperJniMock);
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        mUrl1 = JUnitTestGURLs.URL_1;
        mUrl2 = JUnitTestGURLs.URL_2;

        mTabListFaviconProvider =
                new TabListFaviconProvider(
                        mActivity,
                        /* isTabStrip= */ false,
                        org.chromium.components.browser_ui.styles.R.dimen
                                .default_favicon_corner_radius,
                        mTabWebContentsFaviconDelegate);
        mTabListFaviconProvider.initWithNative(mProfile);
        mTabListFaviconProvider.setFaviconHelperForTesting(mMockFaviconHelper);
    }

    @After
    public void tearDown() {
        mTabListFaviconProvider.destroy();
        verify(mMockFaviconHelper).destroy();
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
    public void testFaviconForTabFetcher_WebContentsFavicon() {
        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(mUrl1);
        Bitmap bitmap = newBitmap();
        when(mTabWebContentsFaviconDelegate.getBitmap(tab)).thenReturn(bitmap);
        TabFaviconFetcher fetcher = mTabListFaviconProvider.getFaviconForTabFetcher(tab);
        TabFavicon favicon =
                (UrlTabFavicon)
                        doFetchFavicon(
                                () -> {
                                    verify(mTabWebContentsFaviconDelegate).getBitmap(tab);
                                    verify(mMockFaviconHelper, never())
                                            .getForeignFaviconImageForURL(
                                                    any(), any(), anyInt(), any());
                                    verify(mMockFaviconHelper, never())
                                            .getLocalFaviconImageForURL(
                                                    any(), any(), anyInt(), any());
                                },
                                fetcher);
        Assert.assertEquals(favicon, new UrlTabFavicon(newDrawable(), mUrl1));
    }

    @Test
    public void testFaviconForTabFetcher_Foreign() {
        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(mUrl1);
        when(tab.getTabGroupId()).thenReturn(new Token(1L, 3L));
        TabFaviconFetcher fetcher = mTabListFaviconProvider.getFaviconForTabFetcher(tab);
        TabFavicon favicon =
                (UrlTabFavicon)
                        doFetchFavicon(
                                () -> {
                                    // Returns null.
                                    verify(mTabWebContentsFaviconDelegate).getBitmap(tab);
                                    verify(mMockFaviconHelper)
                                            .getForeignFaviconImageForURL(
                                                    eq(mProfile),
                                                    eq(mUrl1),
                                                    anyInt(),
                                                    mFaviconImageCallbackCaptor.capture());
                                    verify(mMockFaviconHelper, never())
                                            .getLocalFaviconImageForURL(
                                                    any(), any(), anyInt(), any());
                                    mFaviconImageCallbackCaptor
                                            .getValue()
                                            .onFaviconAvailable(newBitmap(), mUrl1);
                                },
                                fetcher);
        Assert.assertEquals(favicon, new UrlTabFavicon(newDrawable(), mUrl1));
    }

    @Test
    public void testFaviconForTabFetcher_Local_IncognitoGroup() {
        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(mUrl1);
        when(tab.getTabGroupId()).thenReturn(new Token(1L, 3L));
        when(tab.isIncognitoBranded()).thenReturn(true);
        TabFaviconFetcher fetcher = mTabListFaviconProvider.getFaviconForTabFetcher(tab);
        TabFavicon favicon =
                (UrlTabFavicon)
                        doFetchFavicon(
                                () -> {
                                    // Returns null.
                                    verify(mTabWebContentsFaviconDelegate).getBitmap(tab);
                                    verify(mMockFaviconHelper, never())
                                            .getForeignFaviconImageForURL(
                                                    any(), any(), anyInt(), any());
                                    verify(mMockFaviconHelper)
                                            .getLocalFaviconImageForURL(
                                                    eq(mOtrProfile),
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
    public void testFaviconForTabFetcher_Local() {
        Tab tab = mock(Tab.class);
        when(tab.getUrl()).thenReturn(mUrl1);
        TabFaviconFetcher fetcher = mTabListFaviconProvider.getFaviconForTabFetcher(tab);
        TabFavicon favicon =
                (UrlTabFavicon)
                        doFetchFavicon(
                                () -> {
                                    // Returns null.
                                    verify(mTabWebContentsFaviconDelegate).getBitmap(tab);
                                    verify(mMockFaviconHelper, never())
                                            .getForeignFaviconImageForURL(
                                                    any(), any(), anyInt(), any());
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

    private TabFavicon doFetchFavicon(Runnable after, TabFaviconFetcher fetcher) {
        Holder<@Nullable TabFavicon> faviconHolder = new Holder<>(null);
        fetcher.fetch(faviconHolder);
        after.run();
        return faviconHolder.value;
    }

    private TabFavicon doFetchFavicon(TabFaviconFetcher fetcher) {
        return doFetchFavicon(CallbackUtils.emptyRunnable(), fetcher);
    }
}
