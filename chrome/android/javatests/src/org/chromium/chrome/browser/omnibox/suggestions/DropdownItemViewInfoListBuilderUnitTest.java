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

        when(mMockSuggestionProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockSuggestionProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.DEFAULT);

        when(mMockHeaderProcessor.createModel())
                .thenAnswer((mock) -> new PropertyModel(SuggestionCommonProperties.ALL_KEYS));
        when(mMockHeaderProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.HEADER);

        mBuilder = new DropdownItemViewInfoListBuilder(mAutocompleteController, () -> null);
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
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);

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

        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
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

    @DisableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER})
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
    public void visibleSuggestions_missingDropdownHeightAssumesDefaultGroupSize() {
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(AutocompleteMatch.class), anyInt()))
                .thenReturn(true);
        // Create AutocompleteResult with a lot of suggestions.
        final AutocompleteMatch match = builder.build();
        final AutocompleteResult result = new AutocompleteResult(
                Arrays.asList(match, match, match, match, match, match, match, match, match, match),
                null);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(result));

        // Same, with a shorter list of suggestions; in this case we don't know the height of the
        // dropdown view, so we assume we can comfortably fit 5 suggestions.
        final AutocompleteResult shortResult =
                new AutocompleteResult(Arrays.asList(match, match, match, match, match), null);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(shortResult));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void visibleSuggestions_computeNumberOfVisibleSuggestionsFromDropdownHeight() {
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(AutocompleteMatch.class), anyInt()))
                .thenReturn(true);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);

        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final AutocompleteMatch match = builder.build();
        final AutocompleteResult result = new AutocompleteResult(
                Arrays.asList(match, match, match, match, match, match, match, match, match, match),
                null);

        mBuilder.setDropdownHeightWithKeyboardActive(60);
        Assert.assertEquals(6, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(90);
        Assert.assertEquals(9, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(30);
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(result));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void visibleSuggestions_partiallyVisibleSuggestionsAreCountedAsVisible() {
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(AutocompleteMatch.class), anyInt()))
                .thenReturn(true);
        final AutocompleteMatch match = builder.build();
        final AutocompleteResult result = new AutocompleteResult(
                Arrays.asList(match, match, match, match, match, match, match, match, match, match),
                null);

        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        mBuilder.setDropdownHeightWithKeyboardActive(45);
        Assert.assertEquals(5, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(51);
        Assert.assertEquals(6, mBuilder.getVisibleSuggestionsCount(result));
    }

    @Test
    @SmallTest
    @UiThreadTest
    public void visibleSuggestions_queriesCorrespondingProcessorsToDetermineViewAllocation() {
        final SuggestionProcessor mockProcessor1 = mock(SuggestionProcessor.class);
        final SuggestionProcessor mockProcessor2 = mock(SuggestionProcessor.class);
        mBuilder.registerSuggestionProcessor(mockProcessor1);
        mBuilder.registerSuggestionProcessor(mockProcessor2);
        final AutocompleteMatchBuilder builder =
                AutocompleteMatchBuilder.searchWithType(OmniboxSuggestionType.SEARCH_SUGGEST);
        final AutocompleteMatch match1 = builder.setDescription("1").build();
        final AutocompleteMatch match2 = builder.setDescription("2").build();
        final AutocompleteMatch match3 = builder.setDescription("3").build();
        final AutocompleteResult result =
                new AutocompleteResult(Arrays.asList(match1, match2, match3), null);

        // Heights reported by processors for suggestions 1, 2 and 3.
        when(mMockSuggestionProcessor.doesProcessSuggestion(eq(match1), anyInt())).thenReturn(true);
        when(mMockSuggestionProcessor.getMinimumViewHeight()).thenReturn(10);
        when(mockProcessor1.doesProcessSuggestion(eq(match2), anyInt())).thenReturn(true);
        when(mockProcessor1.getMinimumViewHeight()).thenReturn(20);
        when(mockProcessor2.doesProcessSuggestion(eq(match3), anyInt())).thenReturn(true);
        when(mockProcessor2.getMinimumViewHeight()).thenReturn(30);

        mBuilder.setDropdownHeightWithKeyboardActive(
                90); // fits all three suggestions and then some.
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(
                45); // fits 2 suggestions fully, and 3rd partially.
        Assert.assertEquals(3, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(30); // fits only 2 suggestions.
        Assert.assertEquals(2, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(
                20); // fits one suggestion fully and one partially.
        Assert.assertEquals(2, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(10); // fits only one suggestion.
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(result));

        mBuilder.setDropdownHeightWithKeyboardActive(5); // fits one suggestion partiall.
        Assert.assertEquals(1, mBuilder.getVisibleSuggestionsCount(result));
    }

    @EnableFeatures({ChromeFeatureList.OMNIBOX_ADAPTIVE_SUGGESTIONS_COUNT,
            ChromeFeatureList.OMNIBOX_NATIVE_VOICE_SUGGEST_PROVIDER})
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
        when(mMockSuggestionProcessor.doesProcessSuggestion(any(), anyInt())).thenReturn(true);
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
}
