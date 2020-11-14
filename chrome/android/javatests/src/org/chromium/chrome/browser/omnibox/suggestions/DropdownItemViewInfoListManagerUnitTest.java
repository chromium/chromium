// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.verifyNoMoreInteractions;
import static org.mockito.Mockito.when;

import android.util.SparseArray;
import android.view.View;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.MockitoAnnotations;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omnibox.styles.OmniboxTheme;
import org.chromium.components.omnibox.AutocompleteResult.GroupDetails;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Tests for {@link DropdownItemViewInfoListManager}.
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DropdownItemViewInfoListManagerUnitTest {
    private static final int MINIMUM_NUMBER_OF_SUGGESTIONS_TO_SHOW = 5;
    private static final int SUGGESTION_MIN_HEIGHT = 20;
    private static final int HEADER_MIN_HEIGHT = 15;

    @Mock
    DropdownItemProcessor mBasicSuggestionProcessor;

    @Mock
    DropdownItemProcessor mHeaderProcessor;

    @Mock
    PropertyModel mModel;

    @Mock
    ListObserver<Void> mListObserver;

    private ModelList mSuggestionModels;
    private DropdownItemViewInfoListManager mManager;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        when(mBasicSuggestionProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.DEFAULT);
        when(mHeaderProcessor.getViewTypeId()).thenReturn(OmniboxSuggestionUiType.HEADER);

        mSuggestionModels = new ModelList();
        mSuggestionModels.addObserver(mListObserver);
        mManager = new DropdownItemViewInfoListManager(mSuggestionModels);
    }

    /**
     * Verify that the content of the resulting Suggestions list matches the supplied list.
     * Asserts if the two lists differ.
     */
    private void verifyModelEquals(List<DropdownItemViewInfo> expected) {
        Assert.assertEquals(expected.size(), mSuggestionModels.size());

        for (int index = 0; index < expected.size(); index++) {
            Assert.assertEquals("Element at position " + index + " does not match",
                    expected.get(index), mSuggestionModels.get(index));
        }
    }

    /**
     * Verify that PropertyModels of all suggestions on managed list reflect the expected values.
     */
    private void verifyPropertyValues(int layoutDirection, @OmniboxTheme int omniboxTheme) {
        for (int index = 0; index < mSuggestionModels.size(); index++) {
            Assert.assertEquals("Unexpected layout direction for suggestion at position " + index,
                    layoutDirection,
                    mSuggestionModels.get(index).model.get(
                            SuggestionCommonProperties.LAYOUT_DIRECTION));
            Assert.assertEquals("Unexpected visual theme for suggestion at position " + index,
                    omniboxTheme,
                    mSuggestionModels.get(index).model.get(
                            SuggestionCommonProperties.OMNIBOX_THEME));
        }
    }

    @Test
    @SmallTest
    public void modelUpdates_visibilityChangesOnlyUpdateTheModel() {
        // This change confirms that we do not re-create entire model, but instead insert/remove
        // items that have been added/removed from the list.
        final List<DropdownItemViewInfo> list = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(list, new SparseArray<GroupDetails>());
        verifyModelEquals(list);

        // Monitor updates moving forward.
        reset(mListObserver);

        // Collapse group 1.
        mManager.setGroupCollapsedState(1, true);
        verify(mListObserver, times(1)).onItemRangeRemoved(any(), eq(1), eq(2));
        // Collapse group 2.
        mManager.setGroupCollapsedState(2, true);
        verify(mListObserver, times(1)).onItemRangeRemoved(any(), eq(2), eq(2));

        // Expand group 1.
        mManager.setGroupCollapsedState(1, false);
        verify(mListObserver, times(1)).onItemRangeInserted(any(), eq(1), eq(2));

        // Expand group 2.
        mManager.setGroupCollapsedState(2, false);
        verify(mListObserver, times(1)).onItemRangeInserted(any(), eq(4), eq(2));

        verifyNoMoreInteractions(mListObserver);
    }

    @Test
    @SmallTest
    public void groupHandling_toggleGroupExpandedState() {
        final List<DropdownItemViewInfo> listWithBothGroupsExpanded = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(listWithBothGroupsExpanded, new SparseArray<GroupDetails>());
        verifyModelEquals(listWithBothGroupsExpanded);

        // Toggle group 1.
        final List<DropdownItemViewInfo> listWithGroup1Collapsed = new ArrayList<>();
        listWithGroup1Collapsed.add(listWithBothGroupsExpanded.get(0));
        listWithGroup1Collapsed.addAll(listWithBothGroupsExpanded.subList(3, 6));
        mManager.setGroupCollapsedState(1, true);
        verifyModelEquals(listWithGroup1Collapsed);
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(listWithBothGroupsExpanded);

        // Toggle group 2.
        final List<DropdownItemViewInfo> listWithGroup2Collapsed =
                listWithBothGroupsExpanded.subList(0, 4);
        mManager.setGroupCollapsedState(2, true);
        verifyModelEquals(listWithGroup2Collapsed);
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(listWithBothGroupsExpanded);

        // Toggle both groups.
        final List<DropdownItemViewInfo> listWithBothGroupsCollapsed =
                listWithGroup1Collapsed.subList(0, 2);
        mManager.setGroupCollapsedState(2, true);
        verifyModelEquals(listWithGroup2Collapsed);
        mManager.setGroupCollapsedState(1, true);
        verifyModelEquals(listWithBothGroupsCollapsed);
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(listWithGroup1Collapsed);
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(listWithBothGroupsExpanded);
    }

    @Test
    @SmallTest
    public void groupHandling_defaultGroupExpandedState() {
        final List<DropdownItemViewInfo> listWithBothGroupsExpanded = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        // Receive suggestions list with group 1 default-collapsed.
        mManager.setSourceViewInfoList(listWithBothGroupsExpanded, new SparseArray<GroupDetails>() {
            {
                put(1, new GroupDetails("Collapsed", true));
                put(2, new GroupDetails("Expanded", false));
            }
        });

        final List<DropdownItemViewInfo> listWithGroup1Collapsed = new ArrayList<>();
        listWithGroup1Collapsed.add(listWithBothGroupsExpanded.get(0));
        listWithGroup1Collapsed.addAll(listWithBothGroupsExpanded.subList(3, 6));
        verifyModelEquals(listWithGroup1Collapsed);

        // Expand suggestions for group 1.
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(listWithBothGroupsExpanded);

        // Receive suggestions list with group 2 default-collapsed.
        mManager.setSourceViewInfoList(listWithBothGroupsExpanded, new SparseArray<GroupDetails>() {
            {
                put(1, new GroupDetails("Expanded", false));
                put(2, new GroupDetails("Collapsed", true));
            }
        });
        final List<DropdownItemViewInfo> listWithGroup2Collapsed =
                listWithBothGroupsExpanded.subList(0, 4);
        verifyModelEquals(listWithGroup2Collapsed);
        // Expand suggestions for group 2.
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(listWithBothGroupsExpanded);

        // Receive suggestions list with both groups default-collapsed.
        mManager.setSourceViewInfoList(listWithBothGroupsExpanded, new SparseArray<GroupDetails>() {
            {
                put(1, new GroupDetails("Collapsed", true));
                put(2, new GroupDetails("Collapsed", true));
            }
        });
        final List<DropdownItemViewInfo> listWithBothGroupsCollapsed =
                listWithGroup1Collapsed.subList(0, 2);
        verifyModelEquals(listWithBothGroupsCollapsed);
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(listWithGroup1Collapsed);
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(listWithBothGroupsExpanded);
    }

    @Test
    @SmallTest
    public void groupHandling_expandingAlreadyExpandedGroupAddsNoNewElementns() {
        final List<DropdownItemViewInfo> list = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(list, new SparseArray<GroupDetails>());
        verifyModelEquals(list);

        // Expand group 1.
        mManager.setGroupCollapsedState(1, false);
        verifyModelEquals(list);

        // Expand group 2.
        mManager.setGroupCollapsedState(2, false);
        verifyModelEquals(list);
    }

    @Test
    @SmallTest
    public void groupHandling_collapseAlreadyCollapsedListIsNoOp() {
        final List<DropdownItemViewInfo> list = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(list, new SparseArray<GroupDetails>());
        verifyModelEquals(list);

        // Collapse group 1.
        final List<DropdownItemViewInfo> listWithGroup1Collapsed = new ArrayList<>();
        listWithGroup1Collapsed.add(list.get(0));
        listWithGroup1Collapsed.addAll(list.subList(3, 6));
        mManager.setGroupCollapsedState(1, true);
        verifyModelEquals(listWithGroup1Collapsed);
        mManager.setGroupCollapsedState(1, true);
        verifyModelEquals(listWithGroup1Collapsed);

        // Collapse group 2.
        final List<DropdownItemViewInfo> listWithBothGroupsCollapsed =
                listWithGroup1Collapsed.subList(0, 2);
        mManager.setGroupCollapsedState(2, true);
        verifyModelEquals(listWithBothGroupsCollapsed);
        mManager.setGroupCollapsedState(2, true);
        verifyModelEquals(listWithBothGroupsCollapsed);
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_suggestionsAreRebuiltOnSubsequentInteractions() {
        // This test validates scenario:
        // 1. user focuses omnibox
        // 2. AutocompleteMediator receives suggestions
        // 3. user sees suggestions, but leaves omnibox
        // 4. user focuses omnibox again
        // 5. AutocompleteMediator receives same suggestions as in (2)
        // 6. user sees suggestions again.
        final List<DropdownItemViewInfo> list1 = new ArrayList<>();
        list1.add(new DropdownItemViewInfo(mHeaderProcessor, mModel, 1));
        list1.add(new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1));
        list1.add(new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1));

        final List<DropdownItemViewInfo> list2 =
                Arrays.asList(new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                        new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                        new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1));

        mManager.setSourceViewInfoList(list1, new SparseArray<GroupDetails>());
        verifyModelEquals(list1);

        mManager.clear();

        mManager.setSourceViewInfoList(list2, new SparseArray<GroupDetails>());
        verifyModelEquals(list2);
    }

    @Test
    @SmallTest
    public void updateSuggestionsList_uiChangesArePropagatedToSuggestions() {
        List<DropdownItemViewInfo> list =
                Arrays.asList(new DropdownItemViewInfo(mHeaderProcessor,
                                      new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 1),
                        new DropdownItemViewInfo(mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 1),
                        new DropdownItemViewInfo(mBasicSuggestionProcessor,
                                new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 1));

        mManager.setSourceViewInfoList(list, new SparseArray<GroupDetails>());
        verifyModelEquals(list);
        verifyPropertyValues(View.LAYOUT_DIRECTION_INHERIT, OmniboxTheme.LIGHT_THEME);

        mManager.setLayoutDirection(View.LAYOUT_DIRECTION_RTL);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, OmniboxTheme.LIGHT_THEME);

        mManager.setOmniboxTheme(OmniboxTheme.DARK_THEME);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, OmniboxTheme.DARK_THEME);

        mManager.setOmniboxTheme(OmniboxTheme.INCOGNITO);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, OmniboxTheme.INCOGNITO);

        // Finally, set the new list and confirm that the values are still applied.
        list = Arrays.asList(new DropdownItemViewInfo(mHeaderProcessor,
                                     new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor,
                        new PropertyModel(SuggestionCommonProperties.ALL_KEYS), 2));
        mManager.setSourceViewInfoList(list, new SparseArray<GroupDetails>());
        verifyModelEquals(list);
        verifyPropertyValues(View.LAYOUT_DIRECTION_RTL, OmniboxTheme.INCOGNITO);
    }

    @Test
    @SmallTest
    public void visibilityReporting_reportsOnlyForVisibleSuggestions() {
        final List<DropdownItemViewInfo> list = Arrays.asList(
                // Group 1: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 1),
                // Group 2: header + 2 suggestions
                new DropdownItemViewInfo(mHeaderProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2),
                new DropdownItemViewInfo(mBasicSuggestionProcessor, mModel, 2));

        mManager.setSourceViewInfoList(list, new SparseArray<GroupDetails>());
        verifyModelEquals(list);
        reset(mBasicSuggestionProcessor);
        reset(mHeaderProcessor);

        mManager.recordSuggestionsShown();
        verify(mHeaderProcessor, times(2)).recordItemPresented(any());
        verify(mBasicSuggestionProcessor, times(4)).recordItemPresented(any());

        // Collapse group 1.
        mManager.setGroupCollapsedState(1, true);
        reset(mBasicSuggestionProcessor);
        reset(mHeaderProcessor);

        mManager.recordSuggestionsShown();
        verify(mHeaderProcessor, times(2)).recordItemPresented(any());
        // This time we only show the 2 suggestions belonging to group 2.
        verify(mBasicSuggestionProcessor, times(2)).recordItemPresented(any());

        mManager.setGroupCollapsedState(2, true);
        reset(mBasicSuggestionProcessor);
        reset(mHeaderProcessor);

        mManager.recordSuggestionsShown();
        verify(mHeaderProcessor, times(2)).recordItemPresented(any());
        // No suggestions recorded, all have been removed.
        verifyNoMoreInteractions(mBasicSuggestionProcessor);
    }
}
