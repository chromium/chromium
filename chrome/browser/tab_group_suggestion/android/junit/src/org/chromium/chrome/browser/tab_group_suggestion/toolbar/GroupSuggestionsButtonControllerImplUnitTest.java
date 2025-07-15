// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion.toolbar;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.visited_url_ranking.url_grouping.CachedSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponseMetadata;

import java.util.List;

/** Unit tests for {@link GroupSuggestionsButtonControllerImpl} */
@RunWith(BaseRobolectricTestRunner.class)
public class GroupSuggestionsButtonControllerImplUnitTest {

    private static final int WINDOW_ID = 1;
    private static final int TAB_ID = 123;
    private static final int SECOND_TAB_ID = 456;
    private static final int THIRD_TAB_ID = 789;
    private static final int SUGGESTION_ID = 11;
    private static final int SECOND_SUGGESTION_ID = 12;

    @Rule public final MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private GroupSuggestionsService mMockGroupSuggestionService;
    @Mock private Tab mMockTab;
    @Mock private Tab mSecondTab;
    @Mock private Tab mThirdTab;
    @Mock private TabModel mTabModel;
    @Mock private TabGroupModelFilter mTabGroupModelFilter;

    @Before
    public void setUp() throws Exception {
        when(mMockTab.getId()).thenReturn(TAB_ID);
        when(mSecondTab.getId()).thenReturn(SECOND_TAB_ID);
        when(mThirdTab.getId()).thenReturn(THIRD_TAB_ID);

        when(mTabGroupModelFilter.getTabModel()).thenReturn(mTabModel);
        when(mTabModel.getTabById(TAB_ID)).thenReturn(mMockTab);
        when(mTabModel.getTabById(SECOND_TAB_ID)).thenReturn(mSecondTab);
        when(mTabModel.getTabById(THIRD_TAB_ID)).thenReturn(mThirdTab);
    }

    @Test
    public void shouldShowButtonCallsService() {
        var suggestionCallback = mock(Callback.class);
        var mockCachedSuggestion =
                createCachedSuggestions(
                        /* suggestionId= */ 0, /* tabId= */ TAB_ID, suggestionCallback);
        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(mockCachedSuggestion);
        var controller = new GroupSuggestionsButtonControllerImpl(mMockGroupSuggestionService);

        var shouldShow = controller.shouldShowButton(mMockTab, WINDOW_ID);

        verify(mMockGroupSuggestionService).getCachedSuggestions(WINDOW_ID);
        verify(suggestionCallback, never()).onResult(any());
        assertTrue(shouldShow);
    }

    @Test
    public void shouldShowButtonReturnsFalseWhenSuggestionIsForOtherTab() {
        var suggestionCallback = mock(Callback.class);
        var resultCallbackArgumentCaptor = ArgumentCaptor.forClass(UserResponseMetadata.class);
        var mockCachedSuggestion =
                createCachedSuggestions(
                        /* suggestionId= */ SUGGESTION_ID,
                        /* tabId= */ SECOND_TAB_ID,
                        suggestionCallback);
        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(mockCachedSuggestion);
        var controller = new GroupSuggestionsButtonControllerImpl(mMockGroupSuggestionService);

        var shouldShow = controller.shouldShowButton(mMockTab, WINDOW_ID);

        verify(suggestionCallback).onResult(resultCallbackArgumentCaptor.capture());
        verify(mMockGroupSuggestionService).getCachedSuggestions(WINDOW_ID);
        Assert.assertFalse(shouldShow);

        var responseMetadata = resultCallbackArgumentCaptor.getValue();
        Assert.assertEquals(SUGGESTION_ID, responseMetadata.getSuggestionId());
        Assert.assertEquals(UserResponse.NOT_SHOWN, responseMetadata.getUserResponse());
    }

    @Test
    public void callingShouldShowButtonAgainClearsPreviousSuggestion() {
        var firstSuggestionCallback = mock(Callback.class);
        var firstResultCallbackArgumentCaptor = ArgumentCaptor.forClass(UserResponseMetadata.class);
        var firstCachedSuggestion =
                createCachedSuggestions(SUGGESTION_ID, TAB_ID, firstSuggestionCallback);
        var secondSuggestionCallback = mock(Callback.class);
        var secondCachedSuggestion =
                createCachedSuggestions(SECOND_SUGGESTION_ID, TAB_ID, secondSuggestionCallback);

        var controller = new GroupSuggestionsButtonControllerImpl(mMockGroupSuggestionService);

        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(firstCachedSuggestion);
        var firstShouldShow = controller.shouldShowButton(mMockTab, WINDOW_ID);
        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID))
                .thenReturn(secondCachedSuggestion);
        var secondShouldShow = controller.shouldShowButton(mMockTab, WINDOW_ID);

        assertTrue(firstShouldShow);
        verify(firstSuggestionCallback).onResult(firstResultCallbackArgumentCaptor.capture());
        var firstResponseMetadata = firstResultCallbackArgumentCaptor.getValue();
        assertEquals(SUGGESTION_ID, firstResponseMetadata.getSuggestionId());
        assertEquals(UserResponse.NOT_SHOWN, firstResponseMetadata.getUserResponse());
        assertTrue(secondShouldShow);
        verify(secondSuggestionCallback, never()).onResult(any());
    }

    @Test
    public void showingButtonForOtherTabShouldClearSuggestion() {
        var suggestionCallback = mock(Callback.class);
        var suggestionCallbackArgumentCaptor = ArgumentCaptor.forClass(UserResponseMetadata.class);
        var suggestion = createCachedSuggestions(SUGGESTION_ID, TAB_ID, suggestionCallback);
        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID)).thenReturn(suggestion);

        var controller = new GroupSuggestionsButtonControllerImpl(mMockGroupSuggestionService);

        var shouldShow = controller.shouldShowButton(mMockTab, WINDOW_ID);

        controller.onButtonShown(mSecondTab);

        verify(suggestionCallback).onResult(suggestionCallbackArgumentCaptor.capture());
        var responseMetadata = suggestionCallbackArgumentCaptor.getValue();

        assertTrue(shouldShow);
        assertEquals(UserResponse.NOT_SHOWN, responseMetadata.getUserResponse());
    }

    @Test
    public void ignoringButtonShouldProvideCallback() {
        var suggestionCallback = mock(Callback.class);
        var suggestionCallbackArgumentCaptor = ArgumentCaptor.forClass(UserResponseMetadata.class);
        var suggestion = createCachedSuggestions(SUGGESTION_ID, TAB_ID, suggestionCallback);
        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID)).thenReturn(suggestion);

        var controller = new GroupSuggestionsButtonControllerImpl(mMockGroupSuggestionService);

        var shouldShow = controller.shouldShowButton(mMockTab, WINDOW_ID);
        controller.onButtonShown(mMockTab);
        controller.onButtonHidden();

        verify(suggestionCallback).onResult(suggestionCallbackArgumentCaptor.capture());
        var responseMetadata = suggestionCallbackArgumentCaptor.getValue();

        assertTrue(shouldShow);
        assertEquals(UserResponse.IGNORED, responseMetadata.getUserResponse());
    }

    @Test
    public void hidingWithoutShowingShouldProvideCallback() {
        var suggestionCallback = mock(Callback.class);
        var suggestionCallbackArgumentCaptor = ArgumentCaptor.forClass(UserResponseMetadata.class);
        var suggestion = createCachedSuggestions(SUGGESTION_ID, TAB_ID, suggestionCallback);
        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID)).thenReturn(suggestion);

        var controller = new GroupSuggestionsButtonControllerImpl(mMockGroupSuggestionService);

        var shouldShow = controller.shouldShowButton(mMockTab, WINDOW_ID);
        controller.onButtonHidden();

        verify(suggestionCallback).onResult(suggestionCallbackArgumentCaptor.capture());
        var responseMetadata = suggestionCallbackArgumentCaptor.getValue();

        assertTrue(shouldShow);
        assertEquals(UserResponse.NOT_SHOWN, responseMetadata.getUserResponse());
    }

    @Test
    public void clickingWithAnotherTabDoesNothing() {
        var suggestionCallback = mock(Callback.class);
        var suggestionCallbackArgumentCaptor = ArgumentCaptor.forClass(UserResponseMetadata.class);
        var suggestion = createCachedSuggestions(SUGGESTION_ID, TAB_ID, suggestionCallback);
        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID)).thenReturn(suggestion);

        var controller = new GroupSuggestionsButtonControllerImpl(mMockGroupSuggestionService);

        controller.shouldShowButton(mMockTab, WINDOW_ID);
        controller.onButtonClicked(mSecondTab, mTabGroupModelFilter);

        verify(suggestionCallback).onResult(suggestionCallbackArgumentCaptor.capture());
        var responseMetadata = suggestionCallbackArgumentCaptor.getValue();

        assertEquals(UserResponse.UNKNOWN, responseMetadata.getUserResponse());
    }

    @Test
    public void clickingShouldGroupTabs() {
        var suggestionCallback = mock(Callback.class);
        var suggestionCallbackArgumentCaptor = ArgumentCaptor.forClass(UserResponseMetadata.class);
        var suggestion =
                createCachedSuggestions(
                        SUGGESTION_ID,
                        new int[] {TAB_ID, SECOND_TAB_ID, THIRD_TAB_ID},
                        suggestionCallback);
        when(mMockGroupSuggestionService.getCachedSuggestions(WINDOW_ID)).thenReturn(suggestion);

        var controller = new GroupSuggestionsButtonControllerImpl(mMockGroupSuggestionService);

        controller.shouldShowButton(mMockTab, WINDOW_ID);
        controller.onButtonClicked(mMockTab, mTabGroupModelFilter);

        verify(suggestionCallback).onResult(suggestionCallbackArgumentCaptor.capture());
        var responseMetadata = suggestionCallbackArgumentCaptor.getValue();
        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(eq(List.of(mSecondTab, mThirdTab)), eq(mMockTab), eq(true));

        assertEquals(UserResponse.ACCEPTED, responseMetadata.getUserResponse());
    }

    private CachedSuggestions createCachedSuggestions(
            int suggestionId, int tabId, Callback<UserResponseMetadata> suggestionResultCallback) {
        return createCachedSuggestions(suggestionId, new int[] {tabId}, suggestionResultCallback);
    }

    private CachedSuggestions createCachedSuggestions(
            int suggestionId,
            int[] tabIds,
            Callback<UserResponseMetadata> suggestionResultCallback) {
        List<GroupSuggestion> suggestionList =
                List.of(
                        new GroupSuggestion(
                                tabIds,
                                suggestionId,
                                /* suggestionReason= */ 0,
                                /* suggestedName= */ "",
                                /* promoHeader= */ "",
                                /* promoContents= */ ""));
        var suggestions = new GroupSuggestions(suggestionList);

        return new CachedSuggestions(suggestions, suggestionResultCallback);
    }
}
