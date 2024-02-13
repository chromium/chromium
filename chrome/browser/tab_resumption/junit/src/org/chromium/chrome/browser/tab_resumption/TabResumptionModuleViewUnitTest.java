// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import static org.mockito.Mockito.atLeastOnce;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.Features.JUnitProcessor;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab_resumption.TabResumptionModuleUtils.SuggestionClickCallback;
import org.chromium.chrome.browser.tab_resumption.UrlImageProvider.UrlImageCallback;
import org.chromium.url.GURL;
import org.chromium.url.JUnitTestGURLs;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@EnableFeatures({ChromeFeatureList.TAB_RESUMPTION_MODULE_ANDROID})
public class TabResumptionModuleViewUnitTest extends TestSupport {
    @Rule public JUnitProcessor mFeaturesProcessor = new JUnitProcessor();

    @Mock private TabResumptionDataProvider mDataProvider;
    @Mock private UrlImageProvider mUrlImageProvider;

    @Captor private ArgumentCaptor<GURL> mFetchImagePageUrlCaptor;
    @Captor private ArgumentCaptor<UrlImageCallback> mFetchImageCallbackCaptor;

    private TabResumptionModuleView mModuleView;
    private TabResumptionTileContainerView mTileContainerView;

    private SuggestionClickCallback mClickCallback;
    private SuggestionBundle mSuggestionBundle;

    private GURL mLastClickUrl;
    private int mClickCount;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        Context context = ApplicationProvider.getApplicationContext();
        context.setTheme(R.style.Theme_BrowserUI_DayNight);
        mModuleView =
                (TabResumptionModuleView)
                        LayoutInflater.from(context)
                                .inflate(R.layout.tab_resumption_module_layout, null);
        mClickCallback =
                (GURL url) -> {
                    mLastClickUrl = url;
                    ++mClickCount;
                };
        mSuggestionBundle = new SuggestionBundle(CURRENT_TIME_MS);
        mModuleView.setUrlImageProvider(mUrlImageProvider);
        mModuleView.setClickCallback(mClickCallback);
        mTileContainerView = mModuleView.getTileContainerViewForTesting();
    }

    @After
    public void tearDown() {
        mModuleView.destroy();
        mModuleView = null;
    }

    @Test
    @SmallTest
    public void testSetTitle() {
        String testTitle1 = "This is a test title";
        String testTitle2 = "Here is another test title";
        TextView titleTextView =
                ((TextView) mModuleView.findViewById(R.id.tab_resumption_title_description));

        mModuleView.setTitle(testTitle1);
        Assert.assertEquals(testTitle1, titleTextView.getText());
        mModuleView.setTitle(testTitle2);
        Assert.assertEquals(testTitle2, titleTextView.getText());
        mModuleView.setTitle(null);
        Assert.assertEquals("", titleTextView.getText());
    }

    @Test
    @SmallTest
    public void testRenderSingle() {
        SuggestionEntry entry1 =
                new SuggestionEntry(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0),
                        /* id= */ 90);
        mSuggestionBundle.entries.add(entry1);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(0));

        // Check tile texts.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                "From Desktop", ((TextView) tile1.findViewById(R.id.tile_pre_info_text)).getText());
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "3 hours ago \u2022 www.google.com",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());

        // Image is not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test image, and check that it's shown as icon.
        Bitmap bitmap1 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertEquals(null, mLastClickUrl);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.GOOGLE_URL_DOG, mLastClickUrl);
    }

    @Test
    @SmallTest
    public void testRenderDouble() {
        SuggestionEntry entry1 =
                new SuggestionEntry(
                        /* sourceName= */ "My Tablet",
                        /* url= */ JUnitTestGURLs.BLUE_3,
                        /* title= */ "Blue website with a very long title that might not fit",
                        /* timestamp= */ makeTimestamp(24 - 1, 60 - 16, 0),
                        /* id= */ 50);
        SuggestionEntry entry2 =
                new SuggestionEntry(
                        /* sourceName= */ "Desktop",
                        /* url= */ JUnitTestGURLs.GOOGLE_URL_DOG,
                        /* title= */ "Google Dog",
                        /* timestamp= */ makeTimestamp(24 - 3, 0, 0),
                        /* id= */ 90);
        mSuggestionBundle.entries.add(entry1);
        mSuggestionBundle.entries.add(entry2);

        Assert.assertEquals(0, mTileContainerView.getChildCount());

        mModuleView.setSuggestionBundle(mSuggestionBundle);
        Assert.assertEquals(3, mTileContainerView.getChildCount()); // 2 tiles, 1 divider.

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(2, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(2, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(JUnitTestGURLs.BLUE_3, mFetchImagePageUrlCaptor.getAllValues().get(0));
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(1));

        // Check tiles texts, and presence of divider.
        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                "Blue website with a very long title that might not fit",
                ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "16 minutes ago \u2022 From My Tablet",
                ((TextView) tile1.findViewById(R.id.tile_info_text)).getText());

        View divider = (View) mTileContainerView.getChildAt(1);
        Assert.assertEquals(View.VISIBLE, divider.getVisibility());

        TabResumptionTileView tile2 = (TabResumptionTileView) mTileContainerView.getChildAt(2);
        Assert.assertEquals(
                "Google Dog", ((TextView) tile2.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "3 hours ago \u2022 From Desktop",
                ((TextView) tile2.findViewById(R.id.tile_info_text)).getText());

        // Images are not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());
        Assert.assertNull(((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable());

        // Provide test images, and check that they're shown as icons.
        Bitmap bitmap1 = makeBitmap(48, 48);
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());

        Bitmap bitmap2 = makeBitmap(64, 64);
        mFetchImageCallbackCaptor.getAllValues().get(1).onBitmap(bitmap2);
        BitmapDrawable drawable2 =
                (BitmapDrawable) ((ImageView) tile2.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable2);
        Assert.assertEquals(bitmap2, drawable2.getBitmap());

        // Simulate click.
        Assert.assertEquals(0, mClickCount);
        Assert.assertEquals(null, mLastClickUrl);
        tile1.performClick();
        Assert.assertEquals(1, mClickCount);
        Assert.assertEquals(JUnitTestGURLs.BLUE_3, mLastClickUrl);
    }
}
