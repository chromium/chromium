// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.tiles;

import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Context;
import android.content.res.Resources;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.query_tiles.QueryTile;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Unit tests for the "query tiles" omnibox suggestion.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public final class TileSuggestionProcessorUnitTest {
    private PropertyModel mModel;
    private TileSuggestionProcessor mProcessor;

    @Mock
    Context mContext;

    @Mock
    Resources mResources;

    @Mock
    Callback<List<QueryTile>> mSuggestionCallback;

    @Mock
    AutocompleteMatch mSuggestion;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        when(mContext.getResources()).thenReturn(mResources);

        mModel = new PropertyModel();
        mProcessor = new TileSuggestionProcessor(mContext, mSuggestionCallback);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void testDuplicatesDoNotPropagateToUi() {
        List<QueryTile> tiles = Arrays.asList(buildTile("1"), buildTile("2"));
        List<QueryTile> tilesSameRef = new ArrayList<>(tiles);
        List<QueryTile> tilesSameVal = Arrays.asList(buildTile("1"), buildTile("2"));
        List<QueryTile> tilesDifferent = Arrays.asList(buildTile("3"));

        doReturn(tiles, tilesSameRef, tilesSameVal, tilesDifferent, tilesDifferent)
                .when(mSuggestion)
                .getQueryTiles();

        mProcessor.populateModel(mSuggestion, mModel, 0);
        mProcessor.populateModel(mSuggestion, mModel, 0);
        mProcessor.populateModel(mSuggestion, mModel, 0);
        mProcessor.populateModel(mSuggestion, mModel, 0);

        mProcessor.onUrlFocusChange(false);
        mProcessor.populateModel(mSuggestion, mModel, 0);

        ArgumentCaptor<List<QueryTile>> captor = ArgumentCaptor.forClass(List.class);
        verify(mSuggestionCallback, times(4)).onResult(captor.capture());

        List<List<QueryTile>> results = captor.getAllValues();
        Assert.assertEquals(tiles, results.get(0));
        Assert.assertEquals(tilesDifferent, results.get(1));
        Assert.assertEquals(new ArrayList<>(), results.get(2));
        Assert.assertEquals(tilesDifferent, results.get(3));
    }

    private static QueryTile buildTile(String suffix) {
        return new QueryTile("id" + suffix, "displayTitle" + suffix, "accessibilityText" + suffix,
                "queryText" + suffix, new String[] {"url" + suffix}, new String[] {},
                new ArrayList<>());
    }
}
