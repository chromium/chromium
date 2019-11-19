// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions.answer;

import static org.hamcrest.core.IsInstanceOf.instanceOf;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyString;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.text.Spannable;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.image_fetcher.ImageFetcher;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.UrlBarEditingTextStateProvider;
import org.chromium.chrome.browser.omnibox.suggestions.OmniboxSuggestion;
import org.chromium.chrome.browser.omnibox.suggestions.base.BaseSuggestionViewProperties;
import org.chromium.chrome.browser.omnibox.suggestions.base.SuggestionDrawableState;
import org.chromium.chrome.browser.omnibox.suggestions.basic.SuggestionHost;
import org.chromium.components.omnibox.AnswerTextStyle;
import org.chromium.components.omnibox.AnswerTextType;
import org.chromium.components.omnibox.AnswerType;
import org.chromium.components.omnibox.SuggestionAnswer;
import org.chromium.components.omnibox.SuggestionAnswer.ImageLine;
import org.chromium.components.omnibox.SuggestionAnswer.TextField;
import org.chromium.testing.local.LocalRobolectricTestRunner;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModel.WritableIntPropertyKey;
import org.chromium.ui.modelutil.PropertyModel.WritableObjectPropertyKey;

import java.util.Arrays;

/**
 * Tests for {@link AnswerSuggestionProcessor}.
 */
@RunWith(LocalRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class AnswerSuggestionProcessorUnitTest {
    private static final @AnswerType int ANSWER_TYPES[] = {AnswerType.DICTIONARY,
            AnswerType.FINANCE, AnswerType.KNOWLEDGE_GRAPH, AnswerType.SPORTS, AnswerType.SUNRISE,
            AnswerType.TRANSLATION, AnswerType.WEATHER, AnswerType.WHEN_IS, AnswerType.CURRENCY};

    @Mock
    SuggestionHost mSuggestionHost;

    @Mock
    UrlBarEditingTextStateProvider mUrlStateProvider;

    @Mock
    ImageFetcher mImageFetcher;

    @Mock
    Bitmap mFakeBitmap;

    private Activity mActivity;
    private AnswerSuggestionProcessor mProcessor;

    /**
     * Base Suggestion class that can be used for testing.
     * Holds all mechanisms that are required to processSuggestion and validate suggestions.
     */
    class SuggestionTestHelper {
        // Stores created OmniboxSuggestion
        protected final OmniboxSuggestion mSuggestion;
        // Stores PropertyModel for the suggestion.
        protected final PropertyModel mModel;
        // Stores Answer object associated with OmniboxSuggestion (if any).
        private final SuggestionAnswer mAnswer;
        // Current user input, used by calculation suggestion.
        private final String mUserQuery;

        private SuggestionTestHelper(OmniboxSuggestion suggestion, SuggestionAnswer answer,
                PropertyModel model, String userQuery) {
            mSuggestion = suggestion;
            mAnswer = answer;
            mModel = model;
            mUserQuery = userQuery;
        }

        /** Check the content of first suggestion line. */
        private void verifyLine(String expectedTitle, int expectedMaxLineCount,
                String expectedDescription, WritableObjectPropertyKey<Spannable> titleKey,
                WritableIntPropertyKey maxLineCountKey,
                WritableObjectPropertyKey<String> descriptionKey) {
            final Spannable actualTitleSpan = mModel.get(titleKey);
            final String actualTitle = actualTitleSpan == null ? null : actualTitleSpan.toString();
            final String actualDescription = mModel.get(descriptionKey);

            Assert.assertNotNull(actualTitle);
            Assert.assertEquals(expectedTitle, actualTitle);

            Assert.assertEquals(expectedDescription, actualDescription);
            Assert.assertEquals(expectedMaxLineCount, mModel.get(maxLineCountKey));
        }

        void verifyLine1(
                String expectedTitle, int expectedMaxLineCount, String expectedDescription) {
            verifyLine(expectedTitle, expectedMaxLineCount, expectedDescription,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_TEXT,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_MAX_LINES,
                    AnswerSuggestionViewProperties.TEXT_LINE_1_ACCESSIBILITY_DESCRIPTION);
        }

        /** Check the content of first suggestion line. */
        void verifyLine2(
                String expectedTitle, int expectedMaxLineCount, String expectedDescription) {
            verifyLine(expectedTitle, expectedMaxLineCount, expectedDescription,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_TEXT,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_MAX_LINES,
                    AnswerSuggestionViewProperties.TEXT_LINE_2_ACCESSIBILITY_DESCRIPTION);
        }

        /** Get Drawable associated with the suggestion. */
        Drawable getIcon() {
            final SuggestionDrawableState state = mModel.get(BaseSuggestionViewProperties.ICON);
            return state == null ? null : state.drawable;
        }
    }

    /** Create Calculation Suggestion. */
    SuggestionTestHelper createCalculationSuggestion(String displayText, String userQuery) {
        OmniboxSuggestion suggestion = new OmniboxSuggestion(OmniboxSuggestionType.CALCULATOR,
                /* isSearchType */ true, /* relevance */ 0, /* transition */ 0, displayText,
                /* displayTextClassifications */ null, /* description */ null,
                /* descriptionClassifications */ null,
                /* suggestionAnswer */ null, /* fillIntoEdit */ "", /* url */ "",
                /* imageUrl */ "", /* imageDominantColor */ "",
                /* isStarred */ false, /* isDeletable */ false);
        PropertyModel model = mProcessor.createModelForSuggestion(suggestion);
        return new SuggestionTestHelper(suggestion, null, model, userQuery);
    }

    /** Create Answer Suggestion. */
    SuggestionTestHelper createAnswerSuggestion(@AnswerType int type, String line1Text,
            int line1Size, String line2Text, int line2Size, String url) {
        SuggestionAnswer answer =
                new SuggestionAnswer(type, createAnswerImageLine(line1Text, line1Size, null),
                        createAnswerImageLine(line2Text, line2Size, url));
        OmniboxSuggestion suggestion = new OmniboxSuggestion(
                /* nativeType */ 0,
                /* isSearchType */ true, /* relevance */ 0, /* transition */ 0,
                /* displayText */ null, /* displayTextClassifications */ null,
                /* description */ null, /* descriptionClassifications */ null, answer,
                /* fillIntoEdit */ "", /* url */ "", /* imageUrl */ "",
                /* imageDominantColor */ "",
                /* isStarred */ false, /* isDeletable */ false);
        PropertyModel model = mProcessor.createModelForSuggestion(suggestion);
        return new SuggestionTestHelper(suggestion, answer, model, null);
    }

    /** Create a (possibly) multi-line ImageLine object with default formatting. */
    private ImageLine createAnswerImageLine(String text, int lines, String url) {
        return new ImageLine(Arrays.asList(new TextField(AnswerTextType.SUGGESTION, text,
                                     AnswerTextStyle.NORMAL, lines)),
                /* additionalText */ null, /* statusText */ null, url);
    }

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        mActivity = Robolectric.buildActivity(Activity.class).setup().get();

        mProcessor = new AnswerSuggestionProcessor(
                mActivity, mSuggestionHost, mUrlStateProvider, () -> mImageFetcher);
    }

    /** Populate model for associated suggestion. */
    void processSuggestion(SuggestionTestHelper helper) {
        // Note: Calculation needs access to raw, unmodified content of the Omnibox to present
        // the formula the user typed in.
        when(mUrlStateProvider.getTextWithoutAutocomplete()).thenReturn(helper.mUserQuery);

        mProcessor.populateModel(helper.mSuggestion, helper.mModel, 0);

        when(mUrlStateProvider.getTextWithoutAutocomplete()).thenReturn(null);
    }

    @Test
    public void regularAnswer_order() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.KNOWLEDGE_GRAPH, "Query", 1, "Answer", 1, null);
        processSuggestion(suggHelper);

        // Note: Most answers are shown in Answer -> Question order, but announced in
        // Question -> Answer order.
        suggHelper.verifyLine1("Answer", 1, "Query");
        suggHelper.verifyLine2("Query", 1, "Answer");
    }

    @Test
    public void dictionaryAnswer_order() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "Query", 1, "Answer", 1, null);
        processSuggestion(suggHelper);

        // Note: Dictionary answers are shown in Question -> Answer order.
        suggHelper.verifyLine1("Query", 1, "Query");
        suggHelper.verifyLine2("Answer", 1, "Answer");
    }

    @Test
    public void calculationAnswer_order() {
        final SuggestionTestHelper suggHelper = createCalculationSuggestion("12345", "123 + 45");
        processSuggestion(suggHelper);

        suggHelper.verifyLine1("123 + 45", 1, null);
        suggHelper.verifyLine2("12345", 1, null);
    }

    @Test
    public void regularAnswer_shortMultiline() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.KNOWLEDGE_GRAPH, "", 1, "", 3, null);
        processSuggestion(suggHelper);

        suggHelper.verifyLine1("", 3, "");
        suggHelper.verifyLine2("", 1, "");
    }

    @Test
    public void dictionaryAnswer_shortMultiline() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 1, "", 3, null);
        processSuggestion(suggHelper);

        suggHelper.verifyLine1("", 1, "");
        suggHelper.verifyLine2("", 3, "");
    }

    // Check that multiline answers that span across more than 3 lines - are reduced to 3 lines.
    // Check that multiline titles are truncated to a single line.
    @Test
    public void regularAnswer_truncatedMultiline() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.KNOWLEDGE_GRAPH, "", 3, "", 10, null);
        processSuggestion(suggHelper);

        suggHelper.verifyLine1("", 3, "");
        suggHelper.verifyLine2("", 1, "");
    }

    @Test
    public void dictionaryAnswer_truncatedMultiline() {
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 3, "", 10, null);
        processSuggestion(suggHelper);

        suggHelper.verifyLine1("", 1, "");
        suggHelper.verifyLine2("", 3, "");
    }

    // Image fetching and icon association tests.
    @Test
    public void answerImage_fallbackIcons() {
        for (@AnswerType int type : ANSWER_TYPES) {
            SuggestionTestHelper suggHelper = createAnswerSuggestion(type, "", 1, "", 1, null);
            processSuggestion(suggHelper);
            // Note: model is re-created on every iteration.
            Assert.assertNotNull("No icon associated with type: " + type, suggHelper.getIcon());
        }
    }

    @Test
    public void answerImage_iconAssociation() {
        SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 1, "", 1, null);
        Assert.assertEquals(
                R.drawable.ic_book_round, mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createAnswerSuggestion(AnswerType.FINANCE, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.ic_swap_vert_round,
                mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createAnswerSuggestion(AnswerType.KNOWLEDGE_GRAPH, "", 1, "", 1, null);
        Assert.assertEquals(
                R.drawable.ic_google_round, mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createAnswerSuggestion(AnswerType.SPORTS, "", 1, "", 1, null);
        Assert.assertEquals(
                R.drawable.ic_google_round, mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createAnswerSuggestion(AnswerType.SUNRISE, "", 1, "", 1, null);
        Assert.assertEquals(
                R.drawable.ic_wb_sunny_round, mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createAnswerSuggestion(AnswerType.TRANSLATION, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.logo_translate_round,
                mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createAnswerSuggestion(AnswerType.WEATHER, "", 1, "", 1, null);
        Assert.assertEquals(R.drawable.logo_partly_cloudy,
                mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createAnswerSuggestion(AnswerType.WHEN_IS, "", 1, "", 1, null);
        Assert.assertEquals(
                R.drawable.ic_event_round, mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createAnswerSuggestion(AnswerType.CURRENCY, "", 1, "", 1, null);
        Assert.assertEquals(
                R.drawable.ic_loop_round, mProcessor.getSuggestionIcon(suggHelper.mSuggestion));

        suggHelper = createCalculationSuggestion("", "");
        Assert.assertEquals(R.drawable.ic_equals_sign_round,
                mProcessor.getSuggestionIcon(suggHelper.mSuggestion));
    }

    @Test
    public void answerImage_repeatedUrlsAreFetchedOnlyOnce() {
        final String url1 = "http://site1.com";
        final String url2 = "http://site2.com";
        final SuggestionTestHelper sugg1 =
                createAnswerSuggestion(AnswerType.WEATHER, "", 1, "", 1, url1);
        final SuggestionTestHelper sugg2 =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 1, "", 1, url1);
        final SuggestionTestHelper sugg3 =
                createAnswerSuggestion(AnswerType.SPORTS, "", 1, "", 1, url2);
        final SuggestionTestHelper sugg4 =
                createAnswerSuggestion(AnswerType.CURRENCY, "", 1, "", 1, url2);

        processSuggestion(sugg1);
        processSuggestion(sugg2);
        processSuggestion(sugg3);
        processSuggestion(sugg4);

        verify(mImageFetcher, times(1)).fetchImage(eq(url1), anyString(), any());
        verify(mImageFetcher, times(1)).fetchImage(eq(url2), anyString(), any());
        verify(mImageFetcher, times(2)).fetchImage(anyString(), anyString(), any());
    }

    @Test
    public void answerImage_bitmapReplacesIconForAllSuggestionsWithSameUrl() {
        final String url = "http://site.com";
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        final SuggestionTestHelper sugg1 =
                createAnswerSuggestion(AnswerType.WEATHER, "", 1, "", 1, url);
        final SuggestionTestHelper sugg2 =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 1, "", 1, url);
        final SuggestionTestHelper sugg3 =
                createAnswerSuggestion(AnswerType.SPORTS, "", 1, "", 1, url);

        processSuggestion(sugg1);
        processSuggestion(sugg2);
        processSuggestion(sugg3);

        verify(mImageFetcher).fetchImage(eq(url), anyString(), callback.capture());

        final Drawable icon1 = sugg1.getIcon();
        final Drawable icon2 = sugg2.getIcon();
        final Drawable icon3 = sugg3.getIcon();
        callback.getValue().onResult(mFakeBitmap);
        final Drawable newIcon1 = sugg1.getIcon();
        final Drawable newIcon2 = sugg2.getIcon();
        final Drawable newIcon3 = sugg3.getIcon();

        Assert.assertNotEquals(icon1, newIcon1);
        Assert.assertNotEquals(icon2, newIcon2);
        Assert.assertNotEquals(icon3, newIcon3);

        Assert.assertThat(newIcon1, instanceOf(BitmapDrawable.class));
        Assert.assertThat(newIcon2, instanceOf(BitmapDrawable.class));
        Assert.assertThat(newIcon3, instanceOf(BitmapDrawable.class));

        Assert.assertEquals(mFakeBitmap, ((BitmapDrawable) newIcon1).getBitmap());
        Assert.assertEquals(mFakeBitmap, ((BitmapDrawable) newIcon2).getBitmap());
        Assert.assertEquals(mFakeBitmap, ((BitmapDrawable) newIcon3).getBitmap());
    }

    @Test
    public void answerImage_failedBitmapFetchDoesNotClearIcons() {
        final String url = "http://site.com";
        final ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.WEATHER, "", 1, "", 1, url);

        processSuggestion(suggHelper);

        verify(mImageFetcher).fetchImage(eq(url), anyString(), callback.capture());

        final Drawable icon = suggHelper.getIcon();
        callback.getValue().onResult(null);
        final Drawable newIcon = suggHelper.getIcon();

        Assert.assertEquals(icon, newIcon);
    }

    @Test
    public void answerImage_noImageFetchWhenFetcherIsUnavailable() {
        final String url = "http://site.com";
        mImageFetcher = null;
        final SuggestionTestHelper suggHelper =
                createAnswerSuggestion(AnswerType.WEATHER, "", 1, "", 1, url);
        processSuggestion(suggHelper);
        Assert.assertNotNull(suggHelper.getIcon());
    }

    @Test
    public void answerImage_associatedModelsAreErasedFromPendingListAfterImageFetch() {
        ArgumentCaptor<Callback<Bitmap>> callback = ArgumentCaptor.forClass(Callback.class);
        final String url = "http://site1.com";

        final SuggestionTestHelper sugg1 =
                createAnswerSuggestion(AnswerType.WEATHER, "", 1, "", 1, url);
        final SuggestionTestHelper sugg2 =
                createAnswerSuggestion(AnswerType.DICTIONARY, "", 1, "", 1, url);

        processSuggestion(sugg1);
        processSuggestion(sugg2);

        verify(mImageFetcher, times(1)).fetchImage(eq(url), anyString(), callback.capture());

        final Drawable icon1 = sugg1.getIcon();
        final Drawable icon2 = sugg2.getIcon();

        // Invoke callback twice. If models were not erased, these should be updated.
        callback.getValue().onResult(null);
        callback.getValue().onResult(mFakeBitmap);

        final Drawable newIcon1 = sugg1.getIcon();
        final Drawable newIcon2 = sugg2.getIcon();

        // Observe no change, despite updated image.
        Assert.assertEquals(icon1, newIcon1);
        Assert.assertEquals(icon2, newIcon2);
    }
}
