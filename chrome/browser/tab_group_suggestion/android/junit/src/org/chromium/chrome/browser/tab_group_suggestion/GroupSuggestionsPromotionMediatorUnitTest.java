// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.junit.Assert.assertEquals;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.anyBoolean;
import static org.mockito.ArgumentMatchers.anyInt;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;

import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.PROMO_CONTENTS;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.PROMO_HEADER;
import static org.chromium.chrome.browser.tab_group_suggestion.GroupSuggestionsPromotionProperties.SUGGESTED_NAME;

import android.view.View;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestion;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestions;
import org.chromium.components.visited_url_ranking.url_grouping.GroupSuggestionsService;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;

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

    @Mock GroupSuggestionsService mGroupSuggestionsService;
    @Mock BottomSheetController mBottomSheetController;
    @Mock View mContainerView;

    private PropertyModel mModel;
    private GroupSuggestionsPromotionMediator mMediator;

    @Before
    public void setup() {
        mModel = new PropertyModel(GroupSuggestionsPromotionProperties.ALL_KEYS);
        mMediator =
                new GroupSuggestionsPromotionMediator(
                        mModel, mGroupSuggestionsService, mBottomSheetController, mContainerView);
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
    public void testShowSuggestion_FirstSuggestion() {
        int[] tabIds = {1, 2};
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
        verify(mBottomSheetController)
                .requestShowContent(any(GroupSuggestionsBottomSheetContent.class), anyBoolean());
    }
}
