// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.anyInt;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.util.Pair;
import android.util.SparseArray;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.UiThreadTest;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.omnibox.OmniboxSuggestionType;
import org.chromium.chrome.browser.omnibox.suggestions.header.HeaderProcessor;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.chrome.test.util.browser.Features.DisableFeatures;
import org.chromium.chrome.test.util.browser.Features.EnableFeatures;
import org.chromium.components.omnibox.AutocompleteMatch;
import org.chromium.components.omnibox.AutocompleteMatchBuilder;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.components.omnibox.AutocompleteResult.GroupDetails;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link DropdownItemViewInfoListBuilder}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DropdownItemViewInfoListBuilderUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    @Mock
    AutocompleteController mAutocompleteController;

    @Mock
    SuggestionProcessor mMockSuggestionProcessor;

    @Mock
    HeaderProcessor mMockHeaderProcessor;

    DropdownItemViewInfoListBuilder mBuilder;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
        when(mMockSuggestionProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockSuggestionProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.DEFAULT);

        when(mMockHeaderProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockHeaderProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.HEADER);

        mBuilder = new DropdownItemViewInfoListBuilder(mAutocompleteController);
        mBuilder.registerSuggestionProcessor(mMockSuggestionProcessor);
        mBuilder.setHeaderProcessorForTest(mMockHeaderProcessor);
    }

    /**
     * Verify that two lists have exactly same content.
     * Note: this works similarly to Assert.assertEquals(list1, list2), but instead of printing out
     * the content of both lists, simply reports elements that differ.
     * AutocompleteMatch.toString() is verbose enough that the result analysis may be difficult or
     * even impossible for a small list if the output exceeds the Android's logcat entry length
     * limit.
     */
    private <T> void verifyListsMatch(List<T> expected, List<T> actual) {
        Assert.assertEquals(expected.size(), actual.size());
        for (int index = 0; index < expected.size(); index++) {
            Assert.assertEquals("Item at position " + index + " does not match",
                    expected.get(index), actual.get(index));
        }
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void headers_buildsHeaderForFirstSuggestion() {
        final List<AutocompleteMatch> actualList = new ArrayList<>();
        final SparseArray<GroupDetails> groupsDetails = new SparseArray<>();
        groupsDetails.put(1, new GroupDetails("Header 1", false));

        AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();

        actualList.add(suggestion);
        actualList.add(suggestion);

        final InOrder verifier = inOrder(mMockSuggestionProcessor, mMockHeaderProcessor);
        final List<DropdownItemViewInfo> model = mBuilder.buildDropdownViewInfoList(
                new AutocompleteResult(actualList, groupsDetails));

        verifier.verify(mMockHeaderProcessor, times(1)).populateModel(any(), eq(1), eq("Header 1"));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestion), any(), eq(0));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestion), any(), eq(1));
        Assert.assertEquals(3, model.size()); // 1 header + 2 suggestions.

        Assert.assertEquals(model.get(0).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(0).processor, mMockHeaderProcessor);
        Assert.assertEquals(model.get(0).groupId, 1);
        Assert.assertEquals(model.get(1).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(1).processor, mMockSuggestionProcessor);
        Assert.assertEquals(model.get(1).groupId, 1);
        Assert.assertEquals(model.get(2).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(2).processor, mMockSuggestionProcessor);
        Assert.assertEquals(model.get(2).groupId, 1);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void headers_buildsHeadersOnlyWhenGroupChanges() {
        final List<AutocompleteMatch> actualList = new ArrayList<>();
        final SparseArray<GroupDetails> groupsDetails = new SparseArray<>();
        groupsDetails.put(1, new GroupDetails("Header 1", false));
        groupsDetails.put(2, new GroupDetails("Header 2", false));

        AutocompleteMatch suggestionWithNoGroup =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        AutocompleteMatch suggestionForGroup1 =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1)
                        .build();
        AutocompleteMatch suggestionForGroup2 =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(2)
                        .build();

        actualList.add(suggestionWithNoGroup);
        actualList.add(suggestionForGroup1);
        actualList.add(suggestionForGroup1);
        actualList.add(suggestionForGroup2);
        actualList.add(suggestionForGroup2);

        final InOrder verifier = inOrder(mMockSuggestionProcessor, mMockHeaderProcessor);
        final List<DropdownItemViewInfo> model = mBuilder.buildDropdownViewInfoList(
                new AutocompleteResult(actualList, groupsDetails));

        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionWithNoGroup), any(), eq(0));
        verifier.verify(mMockHeaderProcessor, times(1)).populateModel(any(), eq(1), eq("Header 1"));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup1), any(), eq(1));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup1), any(), eq(2));
        verifier.verify(mMockHeaderProcessor, times(1)).populateModel(any(), eq(2), eq("Header 2"));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup2), any(), eq(3));
        verifier.verify(mMockSuggestionProcessor, times(1))
                .populateModel(eq(suggestionForGroup2), any(), eq(4));
        Assert.assertEquals(7, model.size()); // 2 headers + 5 suggestions.

        Assert.assertEquals(model.get(0).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(0).groupId, -1);

        Assert.assertEquals(model.get(1).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(1).groupId, 1);
        Assert.assertEquals(model.get(2).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(2).groupId, 1);
        Assert.assertEquals(model.get(3).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(3).groupId, 1);

        Assert.assertEquals(model.get(4).type, OmniboxSuggestionUiType.HEADER);
        Assert.assertEquals(model.get(4).groupId, 2);
        Assert.assertEquals(model.get(5).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(5).groupId, 2);
        Assert.assertEquals(model.get(6).type, OmniboxSuggestionUiType.DEFAULT);
        Assert.assertEquals(model.get(6).groupId, 2);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void builder_propagatesFocusChangeEvents() {
        mBuilder.onUrlFocusChange(true);
        verify(mMockHeaderProcessor, times(1)).onUrlFocusChange(eq(true));
        verify(mMockSuggestionProcessor, times(1)).onUrlFocusChange(eq(true));

        mBuilder.onUrlFocusChange(false);
        verify(mMockHeaderProcessor, times(1)).onUrlFocusChange(eq(false));
        verify(mMockSuggestionProcessor, times(1)).onUrlFocusChange(eq(false));

        verifyNoMoreInteractions(mMockHeaderProcessor);
        verifyNoMoreInteractions(mMockSuggestionProcessor);
    }

    @DisableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Test
    @SmallTest
    @UiThreadTest
    public void builder_propagatesNativeInitializedEvent() {
        mBuilder.onNativeInitialized();
        verify(mMockHeaderProcessor, times(1)).onNativeInitialized();
        verify(mMockSuggestionProcessor, times(1)).onNativeInitialized();

        verifyNoMoreInteractions(mMockHeaderProcessor);
        verifyNoMoreInteractions(mMockSuggestionProcessor);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void grouping_noGroupingForSuggestionsWithHeaders() {
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> actualList = new ArrayList<>();
        final SparseArray<GroupDetails> groupsDetails = new SparseArray<>();
        groupsDetails.put(1, new GroupDetails("Header 1", false));

        AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .setGroupId(1);

        // Build 4 mixed search/url suggestions with headers.
        actualList.add(new Pair<>(builder.setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(builder.setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(builder.setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(builder.setIsSearch(false).build(), mMockSuggestionProcessor));

        final List<Pair<AutocompleteMatch, SuggestionProcessor>> expectedList = new ArrayList<>();
        expectedList.addAll(actualList);

        mBuilder.groupSuggestionsBySearchVsURL(actualList, 4);
        verifyNoMoreInteractions(mAutocompleteController);
        Assert.assertEquals(actualList, expectedList);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void grouping_shortMixedContentGrouping() {
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> actualList = new ArrayList<>();

        AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);

        // Default match.
        actualList.add(new Pair<>(
                builder.setRelevance(0).setIsSearch(false).build(), mMockSuggestionProcessor));
        // Build 4 mixed search/url suggestions.
        actualList.add(new Pair<>(
                builder.setRelevance(16).setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(14).setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(12).setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(10).setIsSearch(true).build(), mMockSuggestionProcessor));

        // Build 4 mixed search/url suggestions with headers.
        builder.setGroupId(1);
        actualList.add(new Pair<>(builder.setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(builder.setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(builder.setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(builder.setIsSearch(false).build(), mMockSuggestionProcessor));

        final List<Pair<AutocompleteMatch, SuggestionProcessor>> expectedList = new ArrayList<>();
        expectedList.add(actualList.get(0)); // Default match.
        expectedList.add(actualList.get(2)); // Highest scored search suggestion
        expectedList.add(actualList.get(4)); // Next highest scored search suggestion
        expectedList.add(actualList.get(1)); // Highest scored url suggestion
        expectedList.add(actualList.get(3)); // Next highest scored url suggestion
        expectedList.addAll(actualList.subList(5, 9));

        mBuilder.groupSuggestionsBySearchVsURL(actualList, 8);
        verify(mAutocompleteController, times(1)).groupSuggestionsBySearchVsURL(1, 5);
        verifyNoMoreInteractions(mAutocompleteController);
        verifyListsMatch(expectedList, actualList);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void grouping_longMixedContentGrouping() {
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> actualList = new ArrayList<>();

        AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);

        // Default match.
        actualList.add(new Pair<>(
                builder.setRelevance(0).setIsSearch(false).build(), mMockSuggestionProcessor));
        // Build 6 mixed search/url suggestions.
        actualList.add(new Pair<>(
                builder.setRelevance(18).setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(16).setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(14).setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(12).setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(10).setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(8).setIsSearch(false).build(), mMockSuggestionProcessor));

        // Build 4 mixed search/url suggestions with headers.
        builder.setGroupId(1);
        actualList.add(new Pair<>(
                builder.setRelevance(100).setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(100).setIsSearch(false).build(), mMockSuggestionProcessor));

        // Request splitting point to be at 4 suggestions.
        // This should split suggestions into 2 groups:
        // - relevance 18, 14, 16 (in this order)
        // - relevance 10, 18, 8 and 100 (in this order)
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> expectedList = new ArrayList<>();

        // 3 visible suggestions
        expectedList.add(actualList.get(0)); // Default match.
        expectedList.add(actualList.get(1)); // Search suggestion scored 18
        expectedList.add(actualList.get(2)); // URL suggestion scored 16

        // Remaining, invisible suggestions
        expectedList.add(actualList.get(3)); // Search suggestion scored 14
        expectedList.add(actualList.get(5)); // Search suggestion scored 10
        expectedList.add(actualList.get(4)); // URL suggestion scored 12
        expectedList.add(actualList.get(6)); // URL suggestion scored 8
        expectedList.addAll(actualList.subList(7, 9)); // Grouped suggestions.

        mBuilder.groupSuggestionsBySearchVsURL(actualList, 3);
        verify(mAutocompleteController, times(1)).groupSuggestionsBySearchVsURL(1, 3);
        verify(mAutocompleteController, times(1)).groupSuggestionsBySearchVsURL(3, 7);
        verifyNoMoreInteractions(mAutocompleteController);
        verifyListsMatch(expectedList, actualList);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void grouping_longHeaderlessContentGrouping() {
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> actualList = new ArrayList<>();

        AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);

        // Default match.
        actualList.add(new Pair<>(
                builder.setRelevance(0).setIsSearch(false).build(), mMockSuggestionProcessor));
        // Build 8 mixed search/url suggestions.
        // The order is intentionally descending, as SortAndCull would order these items like this
        // for us.
        actualList.add(new Pair<>(
                builder.setRelevance(20).setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(18).setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(16).setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(14).setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(12).setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(10).setIsSearch(true).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(8).setIsSearch(false).build(), mMockSuggestionProcessor));
        actualList.add(new Pair<>(
                builder.setRelevance(6).setIsSearch(false).build(), mMockSuggestionProcessor));

        // Request splitting point to be at 4 suggestions.
        // This should split suggestions into 2 groups:
        // - relevance 18, 14, 20, 16 (in this order)
        // - relevance 10, 18, 8 and 100 (in this order)
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> expectedList = Arrays.asList(
                // Top 4, visible suggestions
                actualList.get(0), // Default match
                actualList.get(2), // Search suggestion scored 18
                actualList.get(1), // URL suggestion scored 20
                actualList.get(3), // URL suggestion scored 16

                actualList.get(4), // Search suggestion scored 14
                actualList.get(6), // Search suggestion scored 10
                actualList.get(5), // URL suggestion scored 12
                actualList.get(7), // URL suggestion scored 8
                actualList.get(8)); // URL suggestion scored 6

        mBuilder.groupSuggestionsBySearchVsURL(actualList, 4);
        verify(mAutocompleteController, times(1)).groupSuggestionsBySearchVsURL(1, 4);
        verify(mAutocompleteController, times(1)).groupSuggestionsBySearchVsURL(4, 9);
        verifyNoMoreInteractions(mAutocompleteController);
        verifyListsMatch(expectedList, actualList);
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void visibleSuggestions_missingDropdownHeightAssumesDefaultGroupSize() {
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final Pair<AutocompleteMatch, SuggestionProcessor> pair =
                new Pair<>(builder.build(), mMockSuggestionProcessor);
        // Create a list of large enough count of suggestions.
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> list =
                Arrays.asList(pair, pair, pair, pair, pair, pair, pair, pair, pair, pair);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(list));

        // Same, with a shorter list of suggestions; in this case we don't know the height of the
        // dropdown view, so we assume we can comfortably fit 5 suggestions.
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> shortList =
                Arrays.asList(pair, pair, pair, pair, pair);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(shortList));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void visibleSuggestions_computeNumberOfVisibleSuggestionsFromDropdownHeight() {
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final Pair<AutocompleteMatch, SuggestionProcessor> pair =
                new Pair<>(builder.build(), mMockSuggestionProcessor);
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> list =
                Arrays.asList(pair, pair, pair, pair, pair, pair, pair, pair, pair, pair);

        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        mBuilder.setDropdownHeightWithKeyboardActive(60);
        Assert.assertEquals(6, mBuilder.getVisibleSuggestionsCount(list));

        mBuilder.setDropdownHeightWithKeyboardActive(90);
        Assert.assertEquals(9, mBuilder.getVisibleSuggestionsCount(list));

        mBuilder.setDropdownHeightWithKeyboardActive(30);
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(list));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void visibleSuggestions_partiallyVisibleSuggestionsAreCountedAsVisible() {
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final Pair<AutocompleteMatch, SuggestionProcessor> pair =
                new Pair<>(builder.build(), mMockSuggestionProcessor);
        final List<Pair<AutocompleteMatch, SuggestionProcessor>> list =
                Arrays.asList(pair, pair, pair, pair, pair, pair, pair, pair, pair, pair);

        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        mBuilder.setDropdownHeightWithKeyboardActive(45);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(list));

        mBuilder.setDropdownHeightWithKeyboardActive(51);
        Assert.assertEquals(6, mBuilder.getVisibleSuggestionsCount(list));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void visibleSuggestions_queriesCorrespondingProcessorsToDetermineViewAllocation() {
        final SuggestionProcessor mockProcessor1 = mock(SuggestionProcessor.class);
        final SuggestionProcessor mockProcessor2 = mock(SuggestionProcessor.class);
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final Pair<AutocompleteMatch, SuggestionProcessor> pair1 =
                new Pair<>(builder.build(), mMockSuggestionProcessor);
        final Pair<AutocompleteMatch, SuggestionProcessor> pair2 =
                new Pair<>(builder.build(), mockProcessor1);
        final Pair<AutocompleteMatch, SuggestionProcessor> pair3 =
                new Pair<>(builder.build(), mockProcessor2);

        final List<Pair<AutocompleteMatch, SuggestionProcessor>> list =
                Arrays.asList(pair1, pair2, pair3);

        // Heights reported by processors for suggestions 1, 2 and 3.
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        when(mockProcessor1.getMinimumViewHeight()).thenReturn(20);
        when(mockProcessor2.getMinimumViewHeight()).thenReturn(30);

        mBuilder.setDropdownHeightWithKeyboardActive(
                90); // fits all three suggestions and then some.
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(list));

        mBuilder.setDropdownHeightWithKeyboardActive(
                45); // fits 2 suggestions fully, and 3rd partially.
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(list));

        mBuilder.setDropdownHeightWithKeyboardActive(30); // fits only 2 suggestions.
        Assert.assertEquals(2, mBuilder.getVisibleSuggestionsCount(list));

        mBuilder.setDropdownHeightWithKeyboardActive(
                20); // fits one suggestion fully and one partially.
        Assert.assertEquals(2, mBuilder.getVisibleSuggestionsCount(list));

        mBuilder.setDropdownHeightWithKeyboardActive(10); // fits only one suggestion.
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(list));

        mBuilder.setDropdownHeightWithKeyboardActive(5); // fits one suggestion partiall.
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(list));
    }

    @EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Test
    @SmallTest
    @UiThreadTest
    public void visibleSuggestions_calculatesPresenceOfConcealedSuggestionsFromDropdownHeight() {
        mBuilder.onNativeInitialized();
        final AutocompleteMatch suggestion =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        final int viewHeight = 20;

        final AutocompleteResult result =
                new AutocompleteResult(Arrays.asList(suggestion, suggestion, suggestion), null);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(viewHeight);

        mBuilder.setDropdownHeightWithKeyboardActive(3 * viewHeight);
        mBuilder.buildDropdownViewInfoList(result);
        Assert.assertFalse(mBuilder.hasFullyConcealedElements());

        mBuilder.setDropdownHeightWithKeyboardActive(2 * viewHeight);
        mBuilder.buildDropdownViewInfoList(result);
        Assert.assertTrue(mBuilder.hasFullyConcealedElements());

        // Third suggestion is partially visible, so counts as visible.
        mBuilder.setDropdownHeightWithKeyboardActive(3 * viewHeight - 1);
        mBuilder.buildDropdownViewInfoList(result);
        Assert.assertFalse(mBuilder.hasFullyConcealedElements());

        mBuilder.setDropdownHeightWithKeyboardActive(2 * viewHeight + 1);
        mBuilder.buildDropdownViewInfoList(result);
        Assert.assertFalse(mBuilder.hasFullyConcealedElements());
    }

    @EnableFeatures(ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT)
    @Test
    @SmallTest
    @UiThreadTest
    public void grouping_verifySpecializedSuggestionsAreNotIncludedInGrouping() {
        mBuilder.onNativeInitialized();
        final int viewHeight = 10;
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_WHAT_YOU_TYPED)
                        .setIsSearch(false) // Pretend all specialized suggestions are URLs so that
                                            // these would get demoted.
                        .setRelevance(1);

        final AutocompleteMatch defaultSuggestion = builder.build();
        final AutocompleteMatch tileSuggestion =
                builder.setType(OmniboxSuggestionType.TILE_SUGGESTION).build();
        final AutocompleteMatch clipboardTextSuggestion =
                builder.setType(OmniboxSuggestionType.CLIPBOARD_TEXT).build();
        final AutocompleteMatch clipboardImageSuggestion =
                builder.setType(OmniboxSuggestionType.CLIPBOARD_IMAGE).build();
        final AutocompleteMatch clipboardUrlSuggestion =
                builder.setType(OmniboxSuggestionType.CLIPBOARD_URL).build();

        final AutocompleteMatch searchSuggestion =
                builder.setRelevance(100)
                        .setIsSearch(true)
                        .setType(OmniboxSuggestionType.SEARCH_SUGGEST)
                        .build();
        final AutocompleteMatch urlSuggestion =
                builder.setRelevance(100).setIsSearch(false).build();

        final List<Pair<AutocompleteMatch, SuggestionProcessor>> pairs = Arrays.asList(
                new Pair(defaultSuggestion, null), // Default match, never participates in grouping.
                new Pair(clipboardUrlSuggestion, null), // Clipboard, specialized suggestion.
                new Pair(tileSuggestion, null), // Query tiles, specialized suggestion.
                new Pair(clipboardTextSuggestion, null), // Clipboard, specialized suggestion.
                new Pair(clipboardImageSuggestion, null), // Clipboard, specialized suggestion.
                new Pair(searchSuggestion, null), new Pair(searchSuggestion, null),
                new Pair(urlSuggestion, null), new Pair(urlSuggestion, null),
                new Pair(searchSuggestion, null), new Pair(searchSuggestion, null));

        final List<Pair<AutocompleteMatch, SuggestionProcessor>> expected = Arrays.asList(
                // Specialized suggestions are in the same order as received.
                pairs.get(0), pairs.get(1), pairs.get(2), pairs.get(3), pairs.get(4),
                // Other suggestions get grouped.
                pairs.get(5), pairs.get(6), pairs.get(9), pairs.get(10), pairs.get(7),
                pairs.get(8));

        mBuilder.groupSuggestionsBySearchVsURL(pairs, pairs.size());
        verify(mAutocompleteController, times(1)).groupSuggestionsBySearchVsURL(5, 11);
        verifyNoMoreInteractions(mAutocompleteController);
        verifyListsMatch(expected, pairs);
    }
}
