// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omnibox.suggestions;

import android.util.SparseArray;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.omnibox.styles.OmniboxTheme;
import org.chromium.components.omnibox.AutocompleteResult;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/** Manages the list of DropdownItemViewInfo elements. */
class DropdownItemViewInfoListManager {
    private final ModelList mManagedModel;
    private int mLayoutDirection;
    private @OmniboxTheme int mOmniboxTheme;
    private List<DropdownItemViewInfo> mSourceViewInfoList;
    private SparseArray<Boolean> mGroupsCollapsedState;

    DropdownItemViewInfoListManager(@NonNull ModelList managedModel) {
        assert managedModel != null : "Must specify a non-null model.";
        mLayoutDirection = View.LAYOUT_DIRECTION_INHERIT;
        mOmniboxTheme = OmniboxTheme.LIGHT_THEME;
        mSourceViewInfoList = Collections.emptyList();
        mGroupsCollapsedState = new SparseArray<>();
        mManagedModel = managedModel;
    }

    /** @return Total count of view infos that may be shown in the Omnibox Suggestions list. */
    int getSuggestionsCount() {
        return mSourceViewInfoList.size();
    }

    /**
     * Sets the layout direction to be used for any new suggestion views.
     * @see View#setLayoutDirection(int)
     */
    void setLayoutDirection(int layoutDirection) {
        if (mLayoutDirection == layoutDirection) return;

        mLayoutDirection = layoutDirection;
        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            PropertyModel model = mSourceViewInfoList.get(i).model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, layoutDirection);
        }
    }

    /**
     * Specifies the visual theme to be used by the suggestions.
     * @param omniboxTheme Specifies which {@link OmniboxTheme} should be used.
     */
    void setOmniboxTheme(@OmniboxTheme int omniboxTheme) {
        if (mOmniboxTheme == omniboxTheme) return;

        mOmniboxTheme = omniboxTheme;
        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            PropertyModel model = mSourceViewInfoList.get(i).model;
            model.set(SuggestionCommonProperties.OMNIBOX_THEME, omniboxTheme);
        }
    }

    /**
     * Toggle the collapsed state of suggestions belonging to specific group.
     *
     * @param groupId ID of the group whose collapsed state is expected to change.
     * @param groupIsCollapsed Collapsed state of the group.
     */
    void setGroupCollapsedState(int groupId, boolean groupIsCollapsed) {
        if (getGroupCollapsedState(groupId) == groupIsCollapsed) return;
        mGroupsCollapsedState.put(groupId, groupIsCollapsed);
        if (groupIsCollapsed) {
            removeSuggestionsForGroup(groupId);
        } else {
            insertSuggestionsForGroup(groupId);
        }
    }

    /**
     * @param groupId ID of the suggestions group.
     * @return True, if group should be collapsed, otherwise false.
     */
    private boolean getGroupCollapsedState(int groupId) {
        return mGroupsCollapsedState.get(groupId, /* defaultCollapsedState= */ false);
    }

    /** @return Whether the supplied view info is a header for the specific group of suggestions. */
    private boolean isGroupHeaderWithId(DropdownItemViewInfo info, int groupId) {
        return (info.type == OmniboxSuggestionUiType.HEADER && info.groupId == groupId);
    }

    /** Clear all DropdownItemViewInfo lists. */
    void clear() {
        mSourceViewInfoList.clear();
        mManagedModel.clear();
        mGroupsCollapsedState.clear();
    }

    /** Record histograms for all currently presented suggestions. */
    void recordSuggestionsShown() {
        for (int index = 0; index < mManagedModel.size(); index++) {
            DropdownItemViewInfo info = (DropdownItemViewInfo) mManagedModel.get(index);
            info.processor.recordItemPresented(info.model);
        }
    }

    /**
     * Specify the input list of DropdownItemViewInfo elements.
     *
     * @param sourceList Source list of ViewInfo elements.
     * @param groupsDetails Group ID to GroupDetails map carrying group collapsed state information.
     */
    void setSourceViewInfoList(@NonNull List<DropdownItemViewInfo> sourceList,
            @NonNull SparseArray<AutocompleteResult.GroupDetails> groupsDetails) {
        mSourceViewInfoList = sourceList;
        mGroupsCollapsedState.clear();

        // Clone information about the recommended group collapsed state.
        for (int index = 0; index < groupsDetails.size(); index++) {
            mGroupsCollapsedState.put(
                    groupsDetails.keyAt(index), groupsDetails.valueAt(index).collapsedByDefault);
        }

        // Build a new list of suggestions. Honor the default collapsed state.
        final List<ListItem> suggestionsList = new ArrayList<>();
        for (int i = 0; i < mSourceViewInfoList.size(); i++) {
            final DropdownItemViewInfo item = mSourceViewInfoList.get(i);
            final PropertyModel model = item.model;
            model.set(SuggestionCommonProperties.LAYOUT_DIRECTION, mLayoutDirection);
            model.set(SuggestionCommonProperties.OMNIBOX_THEME, mOmniboxTheme);

            final boolean groupIsDefaultCollapsed = getGroupCollapsedState(item.groupId);
            if (!groupIsDefaultCollapsed || isGroupHeaderWithId(item, item.groupId)) {
                suggestionsList.add(item);
            }
        }

        mManagedModel.set(suggestionsList);
    }

    /**
     * Remove all suggestions that belong to specific group.
     *
     * @param groupId Group ID of suggestions that should be removed.
     */
    private void removeSuggestionsForGroup(int groupId) {
        int index;
        int count = 0;

        for (index = mManagedModel.size() - 1; index >= 0; index--) {
            DropdownItemViewInfo viewInfo = (DropdownItemViewInfo) mManagedModel.get(index);
            if (isGroupHeaderWithId(viewInfo, groupId)) {
                break;
            } else if (viewInfo.groupId == groupId) {
                count++;
            } else if (count > 0 && viewInfo.groupId != groupId) {
                break;
            }
        }
        if (count > 0) {
            // Skip group header when dropping items.
            mManagedModel.removeRange(index + 1, count);
        }
    }

    /**
     * Insert all suggestions that belong to specific group.
     *
     * @param groupId Group ID of suggestions that should be removed.
     */
    private void insertSuggestionsForGroup(int groupId) {
        int insertPosition = 0;

        // Search for the insert position.
        // Iterate through all *available* view infos until we find the first element that we
        // should insert. To determine the insertion point we skip past all *displayed* view
        // infos that were also preceding elements that we want to insert.
        for (; insertPosition < mManagedModel.size(); insertPosition++) {
            final DropdownItemViewInfo viewInfo =
                    (DropdownItemViewInfo) mManagedModel.get(insertPosition);
            // Insert suggestions directly below their header.
            if (isGroupHeaderWithId(viewInfo, groupId)) break;
        }

        // Check if reached the end of the list.
        if (insertPosition == mManagedModel.size()) return;

        // insertPosition points to header - advance the index and see if we already have
        // elements belonging to that group on the list.
        insertPosition++;
        if (insertPosition < mManagedModel.size()
                && ((DropdownItemViewInfo) mManagedModel.get(insertPosition)).groupId == groupId) {
            return;
        }

        // Find elements to insert.
        int firstElementIndex = -1;
        int count = 0;
        for (int index = 0; index < mSourceViewInfoList.size(); index++) {
            final DropdownItemViewInfo viewInfo = mSourceViewInfoList.get(index);
            if (isGroupHeaderWithId(viewInfo, groupId)) {
                firstElementIndex = index + 1;
            } else if (viewInfo.groupId == groupId) {
                count++;
            } else if (count > 0 && viewInfo.groupId != groupId) {
                break;
            }
        }

        if (count != 0 && firstElementIndex != -1) {
            mManagedModel.addAll(
                    mSourceViewInfoList.subList(firstElementIndex, firstElementIndex + count),
                    insertPosition);
        }
    }
}
