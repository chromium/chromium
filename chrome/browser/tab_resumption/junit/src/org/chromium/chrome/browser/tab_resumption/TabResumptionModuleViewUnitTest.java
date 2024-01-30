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

        mModuleView.setSuggestionBundleThenRender(mSuggestionBundle);
        Assert.assertEquals(1, mTileContainerView.getChildCount());

        // Capture call to fetch image.
        verify(mUrlImageProvider, atLeastOnce())
                .fetchImageForUrl(
                        mFetchImagePageUrlCaptor.capture(), mFetchImageCallbackCaptor.capture());
        Assert.assertEquals(1, mFetchImagePageUrlCaptor.getAllValues().size());
        Assert.assertEquals(1, mFetchImageCallbackCaptor.getAllValues().size());
        Assert.assertEquals(
                JUnitTestGURLs.GOOGLE_URL_DOG, mFetchImagePageUrlCaptor.getAllValues().get(0));

        TabResumptionTileView tile1 = (TabResumptionTileView) mTileContainerView.getChildAt(0);
        Assert.assertEquals(
                "From Desktop", ((TextView) tile1.findViewById(R.id.tile_pre_info_text)).getText());
        Assert.assertEquals(
                "Google Dog", ((TextView) tile1.findViewById(R.id.tile_display_text)).getText());
        Assert.assertEquals(
                "3 hours ago \u2022 www.google.com",
                ((TextView) tile1.findViewById(R.id.tile_post_info_text)).getText());
        // Image not loaded yet.
        Assert.assertNull(((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable());

        Bitmap bitmap1 = makeBitmap(64, 64);
        // Pass test image, and check that it's shown as icon.
        mFetchImageCallbackCaptor.getAllValues().get(0).onBitmap(bitmap1);
        BitmapDrawable drawable1 =
                (BitmapDrawable) ((ImageView) tile1.findViewById(R.id.tile_icon)).getDrawable();
        Assert.assertNotNull(drawable1);
        Assert.assertEquals(bitmap1, drawable1.getBitmap());
    }
}
