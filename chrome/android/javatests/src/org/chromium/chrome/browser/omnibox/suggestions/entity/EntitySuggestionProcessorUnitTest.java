// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.entity;

import static org.hamcrest.core.IsInstanceOf.instanceOf;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Color;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.BaseSwitches;
import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.SuggestionHost;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.url.GURL;

/**
 * Tests for {@link EntitySuggestionProcessor}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
@CommandLineFlags.Add(BaseSwitches.DISABLE_LOW_END_DEVICE_MODE)
public class EntitySuggestionProcessorUnitTest {
    @Mock
    SuggestionHost mSuggestionHost;

    @Mock
    ImageFetcher mImageFetcher;

    private Bitmap mBitmap;
    private EntitySuggestionProcessor mProcessor;

    /**
     * Base Suggestion class that can be used for testing.
     * Holds all mechanisms that are required to processSuggestion and validate suggestions.
     */
    class SuggestionTestHelper {
        // Stores created AutocompleteMatch
        protected final AutocompleteMatch mSuggestion;
        // Stores PropertyModel for the suggestion.
        protected final PropertyModel mModel;

        private SuggestionTestHelper(AutocompleteMatch suggestion, PropertyModel model) {
            mSuggestion = suggestion;
            mModel = model;
        }

        /** Get Drawable associated with the suggestion. */
        Drawable getIcon() {
            final SuggestionDrawableState state = mModel.get(BaseSuggestionViewProperties.ICON);
            return state == null ? null : state.drawable;
        }
    }

    /** Create fake Entity suggestion. */
    SuggestionTestHelper createSuggestion(
            String subject, String description, String color, GURL url) {
        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST_ENTITY)
                        .setDisplayText(subject)
                        .setDescription(description)
                        .setImageUrl(url)
                        .setImageDominantColor(color)
                        .build();
        PropertyModel model = mProcessor.createModel();
        return new SuggestionTestHelper(suggestion, model);
    }

    /** Populate model for associated suggestion. */
    void processSuggestion(SuggestionTestHelper helper) {
        mProcessor.populateModel(helper.mSuggestion, helper.mModel, 0);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mBitmap = Bitmap.createBitmap(1, 1, Config.ALPHA_8);

        mProcessor = new EntitySuggestionProcessor(
                ContextUtils.getApplicationContext(), mSuggestionHost, () -> mImageFetcher);
    }

    ImageFetcher.Params createParams(String url) {
        int size = ContextUtils.getApplicationContext().getResources().getDimensionPixelSize(
                R.dimen.omnibox_suggestion_decoration_image_size);
        return ImageFetcher.Params.create(
                url, ImageFetcher.ENTITY_SUGGESTIONS_UMA_CLIENT_NAME, size, size);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void contentTest_basicContent() {
        SuggestionTestHelper suggHelper =
                createSuggestion("subject", "details", null, GURL.emptyGURL());
        processSuggestion(suggHelper);
        Assert.assertEquals(
                "subject", suggHelper.mModel.get(EntitySuggestionViewProperties.SUBJECT_TEXT));
        Assert.assertEquals(
                "details", suggHelper.mModel.get(EntitySuggestionViewProperties.DESCRIPTION_TEXT));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_noColorOrImage() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", null, GURL.emptyGURL());
        processSuggestion(suggHelper);

        Assert.assertNotNull(suggHelper.getIcon());
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_validHexColor() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", "#fedcba", GURL.emptyGURL());
        processSuggestion(suggHelper);

        Assert.assertThat(suggHelper.getIcon(), instanceOf(ColorDrawable.class));
        ColorDrawable icon = (ColorDrawable) suggHelper.getIcon();
        Assert.assertEquals(icon.getColor(), 0xfffedcba);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_validNamedColor() {
        SuggestionTestHelper suggHelper = createSuggestion("", "", "red", GURL.emptyGURL());
        processSuggestion(suggHelper);

        Assert.assertThat(suggHelper.getIcon(), instanceOf(ColorDrawable.class));
        ColorDrawable icon = (ColorDrawable) suggHelper.getIcon();
        Assert.assertEquals(icon.getColor(), Color.RED);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_invalidColor() {
        // Note, fallback is the bitmap drawable representing a search loupe.
        SuggestionTestHelper suggHelper = createSuggestion("", "", "", GURL.emptyGURL());
        processSuggestion(suggHelper);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));

        suggHelper = createSuggestion("", "", "#", GURL.emptyGURL());
        processSuggestion(suggHelper);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));

        suggHelper = createSuggestion("", "", "invalid", GURL.emptyGURL());
        processSuggestion(suggHelper);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_basicSuccessfulBitmapFetch() {
        final GURL url = new GURL("http://site.com");
        SuggestionTestHelper suggHelper = createSuggestion("", "", "red", url);
        processSuggestion(suggHelper);

        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher).fetchImage(eq(createParams(url.getSpec())), callback.capture());

        Assert.assertThat(suggHelper.getIcon(), instanceOf(ColorDrawable.class));
        callback.getValue().onResult(mBitmap);
        Assert.assertThat(suggHelper.getIcon(), instanceOf(BitmapDrawable.class));
        Assert.assertEquals(mBitmap, ((BitmapDrawable) suggHelper.getIcon()).getBitmap());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_repeatedUrlsAreFetchedOnlyOnce() {
        final GURL url1 = new GURL("http://site1.com");
        final GURL url2 = new GURL("http://site2.com");
        final SuggestionTestHelper sugg1 = createSuggestion("", "", "", url1);
        final SuggestionTestHelper sugg2 = createSuggestion("", "", "", url1);
        final SuggestionTestHelper sugg3 = createSuggestion("", "", "", url2);
        final SuggestionTestHelper sugg4 = createSuggestion("", "", "", url2);

        processSuggestion(sugg1);
        processSuggestion(sugg2);
        processSuggestion(sugg3);
        processSuggestion(sugg4);

        verify(mImageFetcher).fetchImage(eq(createParams(url1.getSpec())), any());
        verify(mImageFetcher).fetchImage(eq(createParams(url2.getSpec())), any());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_bitmapReplacesIconForAllSuggestionsWithSameUrl() {
        final GURL url = new GURL("http://site.com");
        final SuggestionTestHelper sugg1 = createSuggestion("", "", "", url);
        final SuggestionTestHelper sugg2 = createSuggestion("", "", "", url);
        final SuggestionTestHelper sugg3 = createSuggestion("", "", "", url);

        processSuggestion(sugg1);
        processSuggestion(sugg2);
        processSuggestion(sugg3);

        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher).fetchImage(eq(createParams(url.getSpec())), callback.capture());

        final Drawable icon1 = sugg1.getIcon();
        final Drawable icon2 = sugg2.getIcon();
        final Drawable icon3 = sugg3.getIcon();

        callback.getValue().onResult(mBitmap);
        final Drawable newIcon1 = sugg1.getIcon();
        final Drawable newIcon2 = sugg2.getIcon();
        final Drawable newIcon3 = sugg3.getIcon();

        Assert.assertNotEquals(icon1, newIcon1);
        Assert.assertNotEquals(icon2, newIcon2);
        Assert.assertNotEquals(icon3, newIcon3);

        Assert.assertThat(newIcon1, instanceOf(BitmapDrawable.class));
        Assert.assertThat(newIcon2, instanceOf(BitmapDrawable.class));
        Assert.assertThat(newIcon3, instanceOf(BitmapDrawable.class));

        Assert.assertEquals(mBitmap, ((BitmapDrawable) newIcon1).getBitmap());
        Assert.assertEquals(mBitmap, ((BitmapDrawable) newIcon2).getBitmap());
        Assert.assertEquals(mBitmap, ((BitmapDrawable) newIcon3).getBitmap());
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_failedBitmapFetchDoesNotReplaceIcon() {
        final GURL url = new GURL("http://site.com");
        final SuggestionTestHelper suggHelper = createSuggestion("", "", null, url);
        processSuggestion(suggHelper);

        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher).fetchImage(eq(createParams(url.getSpec())), callback.capture());

        final Drawable oldIcon = suggHelper.getIcon();
        callback.getValue().onResult(null);
        final Drawable newIcon = suggHelper.getIcon();

        Assert.assertEquals(oldIcon, newIcon);
        Assert.assertThat(oldIcon, instanceOf(BitmapDrawable.class));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_failedBitmapFetchDoesNotReplaceColor() {
        final GURL url = new GURL("http://site.com");
        final SuggestionTestHelper suggHelper = createSuggestion("", "", "red", url);
        processSuggestion(suggHelper);

        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher).fetchImage(eq(createParams(url.getSpec())), callback.capture());

        final Drawable oldIcon = suggHelper.getIcon();
        callback.getValue().onResult(null);
        final Drawable newIcon = suggHelper.getIcon();

        Assert.assertEquals(oldIcon, newIcon);
        Assert.assertThat(oldIcon, instanceOf(ColorDrawable.class));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void decorationTest_updatedModelsAreRemovedFromPendingRequestsList() {
        final GURL url = new GURL("http://site1.com");
        final SuggestionTestHelper sugg1 = createSuggestion("", "", "", url);
        final SuggestionTestHelper sugg2 = createSuggestion("", "", "", url);

        processSuggestion(sugg1);
        processSuggestion(sugg2);

        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        verify(mImageFetcher).fetchImage(eq(createParams(url.getSpec())), callback.capture());
        verify(mImageFetcher).fetchImage(any(), any());

        final Drawable icon1 = sugg1.getIcon();
        final Drawable icon2 = sugg2.getIcon();

        // Invoke callback twice. If models were not erased, these should be updated.
        callback.getValue().onResult(null);
        callback.getValue().onResult(mBitmap);

        final Drawable newIcon1 = sugg1.getIcon();
        final Drawable newIcon2 = sugg2.getIcon();

        // Observe no change, despite updated image.
        Assert.assertEquals(icon1, newIcon1);
        Assert.assertEquals(icon2, newIcon2);
    }
}
