// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.base;

import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.verify;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.drawable.BitmapDrawable;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestionUiType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.components.favicon.LargeIconBridge;
import org.chromium.components.favicon.LargeIconBridge.LargeIconCallback;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Tests for {@link BaseSuggestionViewProcessor}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class BaseSuggestionProcessorTest {
    private class TestBaseSuggestionProcessor extends BaseSuggestionViewProcessor {
        private final Context mContext;
        private final LargeIconBridge mLargeIconBridge;
        private final Runnable mRunable;
        public TestBaseSuggestionProcessor(Context context, SuggestionHost suggestionHost,
                LargeIconBridge largeIconBridge, Runnable runable) {
            super(context, suggestionHost);
            mContext = context;
            mLargeIconBridge = largeIconBridge;
            mRunable = runable;
        }

        @Override
        public PropertyModel createModel() {
            return new PropertyModel(BaseSuggestionViewProperties.ALL_KEYS);
        }

        @Override
        public boolean doesProcessSuggestion(AutocompleteMatch suggestion, int position) {
            return true;
        }

        @Override
        public int getViewTypeId() {
            return OmniboxSuggestionUiType.DEFAULT;
        }

        @Override
        public void populateModel(AutocompleteMatch suggestion, PropertyModel model, int position) {
            super.populateModel(suggestion, model, position);
            setSuggestionDrawableState(model,
                    SuggestionDrawableState.Builder.forBitmap(mContext, mDefaultBitmap).build());
            fetchSuggestionFavicon(model, suggestion.getUrl(), mLargeIconBridge, mRunable);
        }
    }

    @Mock
    SuggestionHost mSuggestionHost;
    @Mock
    LargeIconBridge mIconBridge;
    @Mock
    Runnable mRunnable;

    private TestBaseSuggestionProcessor mProcessor;
    private AutocompleteMatch mSuggestion;
    private PropertyModel mModel;
    private Bitmap mBitmap;
    private Bitmap mDefaultBitmap;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mBitmap = Bitmap.createBitmap(1, 1, Config.ALPHA_8);
        mProcessor = new TestBaseSuggestionProcessor(
                ContextUtils.getApplicationContext(), mSuggestionHost, mIconBridge, mRunnable);
    }

    /**
     * Create Suggestion for test.
     */
    private void createSuggestion(int type, GURL url) {
        mSuggestion = AutocompleteMatchBuilder.searchWithType(type).setUrl(url).build();
        mModel = mProcessor.createModel();
        mProcessor.populateModel(mSuggestion, mModel, 0);
    }

    @Test
    @SmallTest
    public void suggestionFavicons_showFaviconWhenAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final GURL url = new GURL("http://url");
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, url);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(url), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(mBitmap, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertNotEquals(icon1, icon2);
        Assert.assertEquals(mBitmap, ((BitmapDrawable) icon2.drawable).getBitmap());
    }

    @Test
    @SmallTest
    public void suggestionFavicons_doNotReplaceFallbackIconWhenNoFaviconIsAvailable() {
        final ArgumentCaptor<LargeIconCallback> callback =
                ArgumentCaptor.forClass(LargeIconCallback.class);
        final GURL url = new GURL("http://url");
        createSuggestion(OmniboxSuggestionType.URL_WHAT_YOU_TYPED, url);
        SuggestionDrawableState icon1 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon1);

        verify(mIconBridge).getLargeIconForUrl(eq(url), anyInt(), callback.capture());
        callback.getValue().onLargeIconAvailable(null, 0, false, 0);
        SuggestionDrawableState icon2 = mModel.get(BaseSuggestionViewProperties.ICON);
        Assert.assertNotNull(icon2);

        Assert.assertEquals(icon1, icon2);
    }
}
