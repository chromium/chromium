// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.doNothing;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_LISTENER;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_TEXT;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.GROUP_CONTENT_STRING;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.PROMO_CONTENTS;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.PROMO_HEADER;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.REJECT_BUTTON_LISTENER;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.REJECT_BUTTON_TEXT;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.SUGGESTED_NAME;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.Captor;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter.MergeNotificationType;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.components.visited_url_ranking.url_grouping.UserResponse;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.concurrent.atomic.AtomicInteger;

@RunWith(BaseRobolectricTestRunner.class)
public class GroupSuggestionsPromotionMediatorUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final int SUGGESTION_ID_1 = 135;
    private static final int SUGGESTION_ID_2 = 642;
    private static final String SUGGESTION_NAME_1 = "suggestion_name_1";
    private static final String SUGGESTION_NAME_2 = "suggestion_name_2";
    private static final String PROMO_HEADER_1 = "promo_header_1";
    private static final String PROMO_HEADER_2 = "promo_header_2";
    private static final String PROMO_CONTENTS_1 = "promo_contents_1";
    private static final String PROMO_CONTENTS_2 = "promo_contents_2";
    private static final int TAB_1_ID = 123;
    private static final int TAB_2_ID = 654;
    private static final int INVALID_TAB_ID_1 = 357;
    private static final int INVALID_TAB_ID_2 = 987;

    private static final String TAB_1_TITLE = "Tab 1 title";
    private static final String TAB_2_TITLE = "Tab 2 title";

    @Mock GroupSuggestionsService mGroupSuggestionsService;
    @Mock BottomSheetController mBottomSheetController;
    @Mock View mContainerView;
    @Mock TabGroupModelFilter mTabGroupModelFilter;
    @Mock TabModel mTabModel;
    @Mock Tab mTab1;
    @Mock Tab mTab2;

    @Captor ArgumentCaptor<EmptyBottomSheetObserver> mBottomSheetObserver;

    private PropertyModel mModel;
    private GroupSuggestionsPromotionMediator mMediator;

    @Before
    public void setup() {
        mModel = new PropertyModel(GroupSuggestionsPromotionProperties.ALL_KEYS);
        doReturn(mTabModel).when(mTabGroupModelFilter).getTabModel();
        doNothing().when(mBottomSheetController).addObserver(mBottomSheetObserver.capture());
        mMediator =
                new GroupSuggestionsPromotionMediator(
                        mModel,
                        mGroupSuggestionsService,
                        mBottomSheetController,
                        mTabGroupModelFilter,
                        mContainerView);
        doReturn(mTab1).when(mTabModel).getTabById(TAB_1_ID);
        doReturn(TAB_1_TITLE).when(mTab1).getTitle();
        doReturn(mTab2).when(mTabModel).getTabById(TAB_2_ID);
        doReturn(TAB_2_TITLE).when(mTab2).getTitle();
        doReturn(mTab2).when(mTabModel).getTabById(TAB_2_ID);
        doReturn(null).when(mTabModel).getTabById(INVALID_TAB_ID_1);
        doReturn(null).when(mTabModel).getTabById(INVALID_TAB_ID_2);
        ObservableSupplier<Tab> currentTabSupplier =
                new ObservableSupplier<>() {
                    @Override
                    public @Nullable Tab addObserver(Callback<Tab> obs, int behavior) {
                        return null;
                    }

                    @Override
                    public void removeObserver(Callback<Tab> obs) {}

                    @Override
                    public Tab get() {
                        return mTab1;
                    }
                };
        doReturn(currentTabSupplier).when(mTabModel).getCurrentTabSupplier();
        doReturn(true).when(mBottomSheetController).requestShowContent(any(), anyBoolean());
    }

    @Test
    public void tesInitialization() {
        verify(mGroupSuggestionsService).registerDelegate(eq(mMediator), anyInt());
    }

    @Test
    public void testDestroy() {
        mMediator.destroy();

        verify(mGroupSuggestionsService).unregisterDelegate(eq(mMediator));
    }

    @Test
    public void testShowSuggestion_EmptySuggestion() {
        mMediator.showSuggestion(null, meta -> {});

        GroupSuggestions suggestions = new GroupSuggestions(new ArrayList<>());
        mMediator.showSuggestion(suggestions, meta -> {});

        verify(mBottomSheetController, never())
                .requestShowContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testShowSuggestion_InvalidSuggestion() {
        int[] tabIds = {INVALID_TAB_ID_1, INVALID_TAB_ID_2, TAB_1_ID};
        GroupSuggestion suggestion =
                new GroupSuggestion(
                        tabIds,
                        SUGGESTION_ID_1,
                        0,
                        SUGGESTION_NAME_1,
                        PROMO_HEADER_1,
                        PROMO_CONTENTS_1);
        AtomicInteger userResponse = new AtomicInteger(-1);
        GroupSuggestions suggestions =
                new GroupSuggestions(new ArrayList<>(Arrays.asList(suggestion)));
        mMediator.showSuggestion(
                suggestions,
                meta -> {
                    userResponse.set(meta.mUserResponse);
                });

        verify(mBottomSheetController, never())
                .requestShowContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
        assertEquals(UserResponse.NOT_SHOWN, userResponse.get());
    }

    @Test
    public void testShowSuggestion_FirstSuggestion() {
        int[] tabIds = {TAB_1_ID, TAB_2_ID};
        GroupSuggestion suggestion1 =
                new GroupSuggestion(
                        tabIds,
                        SUGGESTION_ID_1,
                        0,
                        SUGGESTION_NAME_1,
                        PROMO_HEADER_1,
                        PROMO_CONTENTS_1);
        GroupSuggestion suggestion2 =
                new GroupSuggestion(
                        tabIds,
                        SUGGESTION_ID_2,
                        0,
                        SUGGESTION_NAME_2,
                        PROMO_HEADER_2,
                        PROMO_CONTENTS_2);

        GroupSuggestions suggestions =
                new GroupSuggestions(new ArrayList<>(Arrays.asList(suggestion1, suggestion2)));
        mMediator.showSuggestion(suggestions, meta -> {});

        assertEquals(SUGGESTION_NAME_1, mModel.get(SUGGESTED_NAME));
        assertEquals(PROMO_HEADER_1, mModel.get(PROMO_HEADER));
        assertEquals(PROMO_CONTENTS_1, mModel.get(PROMO_CONTENTS));
        assertEquals("Accept", mModel.get(ACCEPT_BUTTON_TEXT));
        assertEquals("Reject", mModel.get(REJECT_BUTTON_TEXT));
        assertNotNull(mModel.get(ACCEPT_BUTTON_LISTENER));
        assertNotNull(mModel.get(REJECT_BUTTON_LISTENER));
        assertEquals("Tab 1: Tab 1 title\nTab 2: Tab 2 title\n", mModel.get(GROUP_CONTENT_STRING));
        verify(mBottomSheetController)
                .requestShowContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
    }

    @Test
    public void testAcceptSuggestion() {
        int[] tabIds = {TAB_1_ID, TAB_2_ID};
        GroupSuggestion suggestion =
                new GroupSuggestion(
                        tabIds,
                        SUGGESTION_ID_1,
                        0,
                        SUGGESTION_NAME_1,
                        PROMO_HEADER_1,
                        PROMO_CONTENTS_1);

        GroupSuggestions suggestions =
                new GroupSuggestions(new ArrayList<>(Arrays.asList(suggestion)));
        AtomicInteger userResponse = new AtomicInteger(-1);
        mMediator.showSuggestion(
                suggestions,
                meta -> {
                    userResponse.set(meta.mUserResponse);
                });

        verify(mBottomSheetController)
                .requestShowContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
        GroupSuggestionsBottomSheetContent currentContent = mMediator.getCurrentSheetContent();
        assertNotNull(currentContent);

        mModel.get(GroupSuggestionsPromotionProperties.ACCEPT_BUTTON_LISTENER)
                .onClick(mock(View.class));

        verify(mTabGroupModelFilter)
                .mergeListOfTabsToGroup(
                        eq(Arrays.asList(mTab1, mTab2)),
                        eq(mTab1),
                        eq(MergeNotificationType.NOTIFY_IF_NOT_NEW_GROUP));
        verify(mBottomSheetController).hideContent(eq(currentContent), eq(true));
        assertNull(mMediator.getCurrentSheetContent());
        assertEquals(UserResponse.ACCEPTED, userResponse.get());
    }

    @Test
    public void testRejectSuggestion() {
        int[] tabIds = {TAB_1_ID, TAB_2_ID};
        GroupSuggestion suggestion =
                new GroupSuggestion(
                        tabIds,
                        SUGGESTION_ID_1,
                        0,
                        SUGGESTION_NAME_1,
                        PROMO_HEADER_1,
                        PROMO_CONTENTS_1);

        GroupSuggestions suggestions =
                new GroupSuggestions(new ArrayList<>(Arrays.asList(suggestion)));
        AtomicInteger userResponse = new AtomicInteger(-1);
        mMediator.showSuggestion(
                suggestions,
                meta -> {
                    userResponse.set(meta.mUserResponse);
                });

        verify(mBottomSheetController)
                .requestShowContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
        GroupSuggestionsBottomSheetContent currentContent = mMediator.getCurrentSheetContent();
        assertNotNull(currentContent);

        mModel.get(GroupSuggestionsPromotionProperties.REJECT_BUTTON_LISTENER)
                .onClick(mock(View.class));

        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(any(List.class), any(Tab.class), anyInt());
        verify(mBottomSheetController).hideContent(eq(currentContent), eq(true));
        assertNull(mMediator.getCurrentSheetContent());
        assertEquals(UserResponse.REJECTED, userResponse.get());
    }

    @Test
    public void testIgnoreSuggestion() {
        int[] tabIds = {TAB_1_ID, TAB_2_ID};
        GroupSuggestion suggestion =
                new GroupSuggestion(
                        tabIds,
                        SUGGESTION_ID_1,
                        0,
                        SUGGESTION_NAME_1,
                        PROMO_HEADER_1,
                        PROMO_CONTENTS_1);

        GroupSuggestions suggestions =
                new GroupSuggestions(new ArrayList<>(Arrays.asList(suggestion)));
        AtomicInteger userResponse = new AtomicInteger(-1);
        mMediator.showSuggestion(
                suggestions,
                meta -> {
                    userResponse.set(meta.mUserResponse);
                });

        verify(mBottomSheetController)
                .requestShowContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
        GroupSuggestionsBottomSheetContent currentContent = mMediator.getCurrentSheetContent();
        assertNotNull(currentContent);

        mBottomSheetObserver.getValue().onSheetClosed(0);

        verify(mTabGroupModelFilter, never())
                .mergeListOfTabsToGroup(any(List.class), any(Tab.class), anyInt());
        verify(mBottomSheetController, never())
                .hideContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
        assertNull(mMediator.getCurrentSheetContent());
        assertEquals(UserResponse.IGNORED, userResponse.get());
    }

    @Test
    public void testShowSuggestion_IgnoreSubsequentSuggestions() {
        int[] tabIds = {TAB_1_ID, TAB_2_ID};
        GroupSuggestion suggestion =
                new GroupSuggestion(
                        tabIds,
                        SUGGESTION_ID_1,
                        0,
                        SUGGESTION_NAME_1,
                        PROMO_HEADER_1,
                        PROMO_CONTENTS_1);

        // Try to show suggestions before users take any action.
        GroupSuggestions suggestions =
                new GroupSuggestions(new ArrayList<>(Arrays.asList(suggestion)));
        AtomicInteger userResponse1 = new AtomicInteger(-1);
        mMediator.showSuggestion(
                suggestions,
                meta -> {
                    userResponse1.set(meta.mUserResponse);
                });

        AtomicInteger userResponse2 = new AtomicInteger(-1);
        mMediator.showSuggestion(
                suggestions,
                meta -> {
                    userResponse2.set(meta.mUserResponse);
                });

        assertEquals(-1, userResponse1.get());
        assertEquals(UserResponse.NOT_SHOWN, userResponse2.get());
        verify(mBottomSheetController, times(1))
                .requestShowContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
    }
}
