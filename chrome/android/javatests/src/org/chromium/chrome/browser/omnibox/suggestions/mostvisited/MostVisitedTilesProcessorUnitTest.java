// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.mostvisited;

import android.content.Context;
import android.content.res.Resources;
import android.graphics.Bitmap;
import android.util.DisplayMetrics;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.carousel.BaseCarouselSuggestionViewProperties;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.browser_ui.util.GlobalDiscardableReferencePool;
import org.chromium.components.embedder_support.util.UrlConstants;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatch.SuggestTile;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link MostVisitedTilesProcessor}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class MostVisitedTilesProcessorUnitTest {
    private static final GURL EXPLORE_SITES_URL = new GURL(UrlConstants.EXPLORE_URL);
    private int mCallbackCount;

    @Rule
    public TestRule mFeaturesProcessor = new Features.JUnitProcessor();

    @Mock
    Context mContext;
    @Mock
    Resources mResources;
    @Mock
    SuggestionHost mSuggestionHost;
    @Mock
    PropertyModel mPropertyModel;
    @Mock
    AutocompleteMatch mSuggestion;
    @Mock
    DisplayMetrics mDisplayMetrics;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        ArrayList<PropertyKey> allProperties =
                new ArrayList<>(Arrays.asList(BaseCarouselSuggestionViewProperties.ALL_KEYS));
        mPropertyModel = new PropertyModel(allProperties);
    }

    @Test
    @MediumTest
    public void doNotFetchExploreSiteIconWithCachedReady() {
        List<SuggestTile> tiles =
                Arrays.asList(new SuggestTile("explore_sites", EXPLORE_SITES_URL, false));
        Mockito.doReturn(tiles).when(mSuggestion).getSuggestTiles();

        Bitmap exploreSitesIcon = Bitmap.createBitmap(100, 100, Bitmap.Config.ARGB_8888);
        mCallbackCount = 0;
        ExploreIconProvider exploreIconProvider = (pixelSize, callback) -> {
            callback.onResult(exploreSitesIcon);
            mCallbackCount++;
        };

        Mockito.doReturn(mResources).when(mContext).getResources();
        Mockito.doReturn(84)
                .when(mResources)
                .getDimensionPixelSize(
                        org.chromium.chrome.browser.omnibox.R.dimen.tile_view_icon_size);
        Mockito.doReturn(24)
                .when(mResources)
                .getDimensionPixelSize(org.chromium.chrome.browser.omnibox.R.dimen
                                               .omnibox_suggestion_favicon_size);
        Mockito.doReturn(mDisplayMetrics).when(mResources).getDisplayMetrics();

        MostVisitedTilesProcessor processor = new MostVisitedTilesProcessor(mContext,
                mSuggestionHost,
                () -> null, exploreIconProvider, GlobalDiscardableReferencePool.getReferencePool());

        Assert.assertNull(processor.getExploreSitesIconForTesting());
        processor.populateModel(mSuggestion, mPropertyModel, 0);
        // Verifies that mExploreIconState#getSummaryImage() is called when there isn't any cached
        // explore sites icon.
        Assert.assertNotNull(processor.getExploreSitesIconForTesting());
        Assert.assertEquals(1, mCallbackCount);

        Assert.assertEquals(exploreSitesIcon, processor.getExploreSitesIconForTesting());
        processor.populateModel(mSuggestion, mPropertyModel, 0);
        // Verifies that mExploreIconState#getSummaryImage() is no longer called when there is a
        // cached explore sites icon.
        Assert.assertEquals(1, mCallbackCount);
        Assert.assertEquals(exploreSitesIcon, processor.getExploreSitesIconForTesting());
    }
}
