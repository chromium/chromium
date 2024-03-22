// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.Matchers.instanceOf;

import android.app.Activity;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.graphics.drawable.LayerDrawable;

import androidx.annotation.ColorInt;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.ResourceTabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.StaticTabFaviconType;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFavicon;
import org.chromium.chrome.browser.tab_ui.TabListFaviconProvider.TabFaviconFetcher;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupColorFaviconProvider.TabGroupColorFavicon;
import org.chromium.components.tab_groups.TabGroupColorId;

/** Unit tests for {@link TabGroupColorFaviconProvider}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TabGroupColorFaviconProviderTest {
    private static final @TabGroupColorId int COLOR_ID_1 = TabGroupColorId.BLUE;
    private static final @TabGroupColorId int COLOR_ID_2 = TabGroupColorId.YELLOW;

    private Activity mActivity;
    private TabGroupColorFaviconProvider mTabGroupColorFaviconProvider;
    private int mUniqueColorValue;

    @Before
    public void setUp() {
        mActivity = Robolectric.setupActivity(Activity.class);
        mActivity.setTheme(R.style.Theme_BrowserUI_DayNight);

        MockitoAnnotations.initMocks(this);

        mTabGroupColorFaviconProvider = new TabGroupColorFaviconProvider(mActivity);
    }

    @Test
    public void testFaviconFromTabGroupColorFetcher() {
        TabFaviconFetcher fetcher =
                mTabGroupColorFaviconProvider.getFaviconFromTabGroupColorFetcher(COLOR_ID_2, false);
        TabGroupColorFavicon favicon = (TabGroupColorFavicon) doFetchFavicon(fetcher);
        Assert.assertEquals(favicon, new TabGroupColorFavicon(newDrawable(), COLOR_ID_2));

        LayerDrawable layerDrawable = (LayerDrawable) favicon.getDefaultDrawable();
        // Check the color drawable
        Assert.assertThat(layerDrawable.getDrawable(1), instanceOf(GradientDrawable.class));
        GradientDrawable drawable1 = (GradientDrawable) layerDrawable.getDrawable(1);
        Assert.assertEquals(GradientDrawable.OVAL, drawable1.getShape());
        Assert.assertEquals(
                ColorStateList.valueOf(
                        ColorPickerUtils.getTabGroupColorPickerItemColor(
                                mActivity, COLOR_ID_2, false)),
                drawable1.getColor());
    }

    @Test
    public void testTabGroupColorFavicon() {
        TabFavicon tabGroupColorFavicon = new TabGroupColorFavicon(newDrawable(), COLOR_ID_1);
        Assert.assertEquals(
                tabGroupColorFavicon, new TabGroupColorFavicon(newDrawable(), COLOR_ID_1));
        Assert.assertNotEquals(
                tabGroupColorFavicon, new TabGroupColorFavicon(newDrawable(), COLOR_ID_2));
        Assert.assertNotEquals(tabGroupColorFavicon, null);
        Assert.assertNotEquals(
                tabGroupColorFavicon,
                new ResourceTabFavicon(newDrawable(), StaticTabFaviconType.ROUNDED_GLOBE));
    }

    private Drawable newDrawable() {
        Bitmap image = newBitmap();
        Resources resources = ContextUtils.getApplicationContext().getResources();
        return new BitmapDrawable(resources, image);
    }

    private Bitmap newBitmap() {
        Bitmap image = Bitmap.createBitmap(1, 1, Bitmap.Config.ARGB_8888);
        int colorValue = ++mUniqueColorValue;
        final @ColorInt int colorInt = Color.rgb(colorValue, colorValue, colorValue);
        image.eraseColor(colorInt);
        return image;
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
        return doFetchFavicon(() -> {}, fetcher);
    }
}
