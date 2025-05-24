// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB_GROUP;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_GROUP_SYNC_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;

import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabGridView.AnimationStatus;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyListModel;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.List;

/**
 * A {@link PropertyListModel} implementation to keep information about a list of {@link
 * org.chromium.chrome.browser.tab.Tab}s.
 */
@NullMarked
class TabListModel extends ModelList {
    /** Required properties for each {@link PropertyModel} managed by this {@link ModelList}. */
    static class CardProperties {
        /** Supported Model type within this ModelList. */
        @IntDef({TAB, MESSAGE, TAB_GROUP})
        @Retention(RetentionPolicy.SOURCE)
        @Target(ElementType.TYPE_USE)
        public @interface ModelType {
            int TAB = 0;
            int MESSAGE = 1;
            int TAB_GROUP = 2;
        }

        /** This corresponds to {@link CardProperties.ModelType}*/
        public static final PropertyModel.ReadableIntPropertyKey CARD_TYPE =
                new PropertyModel.ReadableIntPropertyKey();

        public static final PropertyModel.WritableFloatPropertyKey CARD_ALPHA =
                new PropertyModel.WritableFloatPropertyKey();
    }

    /**
     * Lookup the position of a tab by its tab ID.
     *
     * @param tabId The tab ID to search for.
     * @return The index within the model list or {@link TabModel.INVALID_TAB_INDEX}.
     */
    public int indexFromTabId(int tabId) {
        for (int i = 0; i < size(); i++) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == TAB && model.get(TAB_ID) == tabId) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Lookup the position of a tab group by its sync ID.
     *
     * @param syncId The sync ID to search for.
     * @return The index within the model list or {@link TabModel.INVALID_TAB_INDEX}.
     */
    public int indexFromSyncId(String syncId) {
        for (int i = 0; i < size(); i++) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == TAB_GROUP && model.get(TAB_GROUP_SYNC_ID).equals(syncId)) {
                return i;
            }
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Lookup a {@link PropertyModel} for the tab by its ID.
     *
     * @param tabId The tab ID to search for.
     * @return The property model in the model list or null.
     */
    public @Nullable PropertyModel getModelFromTabId(int tabId) {
        for (int i = 0; i < size(); i++) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == TAB && model.get(TAB_ID) == tabId) return model;
        }
        return null;
    }

    /**
     * Lookup a {@link PropertyModel} for the tab group by its sync ID.
     *
     * @param syncId The sync ID to search for.
     * @return The property model in the model list or null.
     */
    public @Nullable PropertyModel getModelFromSyncId(String syncId) {
        for (int i = 0; i < size(); i++) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == TAB_GROUP && model.get(TAB_GROUP_SYNC_ID).equals(syncId)) {
                return model;
            }
        }
        return null;
    }

    /** Returns the property model of the first tab card or null if one does not exist. */
    public @Nullable PropertyModel getFirstTabPropertyModel() {
        for (int i = 0; i < size(); i++) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == TAB) {
                return model;
            }
        }
        return null;
    }

    /**
     * Find the Nth TAB card in the {@link TabListModel}.
     *
     * @param n N of the Nth TAB card.
     * @return The index of Nth TAB card in the {@link TabListModel} or TabModel.INVALID_TAB_INDEX
     *     if not enough tabs exist.
     */
    public int indexOfNthTabCardOrInvalid(int n) {
        int index = indexOfNthTabCard(n);
        if (index < 0 || index >= size() || get(index).model.get(CARD_TYPE) != TAB) {
            return TabModel.INVALID_TAB_INDEX;
        }
        return index;
    }

    /**
     * Find the Nth TAB card in the {@link TabListModel}.
     *
     * @param n N of the Nth TAB card.
     * @return The index of Nth TAB card in the {@link TabListModel} or the index after the last tab
     *     card if {@code n} exceeds the number of tabs.
     */
    public int indexOfNthTabCard(int n) {
        if (n < 0) return TabModel.INVALID_TAB_INDEX;
        int tabCount = 0;
        int lastTabIndex = TabModel.INVALID_TAB_INDEX;
        for (int i = 0; i < size(); i++) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == TAB || model.get(CARD_TYPE) == TAB_GROUP) {
                if (tabCount++ == n) return i;
                lastTabIndex = i;
            }
        }
        // If n >= tabCount, we return the index after the last tab. This is used when adding a new
        // tab.
        return lastTabIndex + 1;
    }

    /** Returns the filter index of a tab from its view index. */
    public int indexOfTabCardsOrInvalid(int index) {
        if (index < 0 || index >= size() || get(index).model.get(CARD_TYPE) != TAB) {
            return TabModel.INVALID_TAB_INDEX;
        }

        return getTabCardCountsBefore(index);
    }

    /** Get the number of TAB_GROUP cards in the TabListModel. */
    public int getTabGroupCardCount() {
        int tabGroupCardCount = 0;
        for (int i = 0; i < size(); i++) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == TAB_GROUP) {
                tabGroupCardCount++;
            }
        }
        return tabGroupCardCount;
    }

    /**
     * Get the number of TAB cards before the given index in TabListModel.
     *
     * @param index The given index in TabListModel.
     * @return The number of TAB cards before the given index.
     */
    public int getTabCardCountsBefore(int index) {
        if (index < 0) return TabModel.INVALID_TAB_INDEX;
        if (index > size()) index = size();
        int tabCount = 0;
        for (int i = 0; i < index; i++) {
            if (get(i).model.get(CARD_TYPE) == TAB) tabCount++;
        }
        return tabCount;
    }

    /**
     * Get the index of the last tab before the given index in TabListModel.
     * @param index The given index in TabListModel.
     * @return The index of the tab before the given index in TabListModel.
     */
    public int getTabIndexBefore(int index) {
        for (int i = index - 1; i >= 0; i--) {
            if (get(i).model.get(CARD_TYPE) == TAB) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Get the index of the first tab after the given index in TabListModel.
     * @param index The given index in TabListModel.
     * @return The index of the tab after the given index in TabListModel.
     */
    public int getTabIndexAfter(int index) {
        for (int i = index + 1; i < size(); i++) {
            if (get(i).model.get(CARD_TYPE) == TAB) return i;
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /**
     * Get the index that matches a message item that has the given message type.
     *
     * @param messageType The message type to match.
     * @return The index within the model.
     */
    public int lastIndexForMessageItemFromType(@MessageService.MessageType int messageType) {
        for (int i = size() - 1; i >= 0; i--) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == MESSAGE && model.get(MESSAGE_TYPE) == messageType) {
                return i;
            }
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    /** Get the last index of a message item. */
    public int lastIndexForMessageItem() {
        for (int i = size() - 1; i >= 0; i--) {
            PropertyModel model = get(i).model;
            if (model.get(CARD_TYPE) == MESSAGE) {
                return i;
            }
        }
        return TabModel.INVALID_TAB_INDEX;
    }

    @Override
    public void add(int position, MVCListAdapter.ListItem item) {
        assert validateListItem(item);
        super.add(position, item);
    }

    private boolean validateListItem(MVCListAdapter.ListItem item) {
        try {
            item.model.get(CARD_TYPE);
        } catch (IllegalArgumentException e) {
            return false;
        }
        return true;
    }

    @Override
    public MVCListAdapter.ListItem removeAt(int position) {
        if (position >= 0 && position < size()) {
            destroyTabGroupColorViewProviderIfNotNull(get(position).model);
        }
        return super.removeAt(position);
    }

    @Override
    public void clear() {
        for (int i = 0; i < size(); i++) {
            destroyTabGroupColorViewProviderIfNotNull(get(i).model);
        }
        super.clear();
    }

    private void destroyTabGroupColorViewProviderIfNotNull(PropertyModel model) {
        if (model.get(CARD_TYPE) == TAB) {
            @Nullable TabGroupColorViewProvider provider =
                    model.get(TabProperties.TAB_GROUP_COLOR_VIEW_PROVIDER);
            if (provider != null) provider.destroy();
        }
    }

    /**
     * Sync the {@link TabListModel} with updated information. Update tab id of the item in {@code
     * index} with the current selected {@code tab} of the group.
     *
     * @param selectedTab The current selected tab in the group.
     * @param index The index of the item in {@link TabListModel} that needs to be updated.
     */
    void updateTabListModelIdForGroup(Tab selectedTab, int index) {
        if (index < 0 || index >= size()) return;

        PropertyModel propertyModel = get(index).model;
        // TODO(crbug.com/398186407): Consider using getTabPropertyModel() here instead.
        if (propertyModel.get(CARD_TYPE) != TAB) return;

        propertyModel.set(TabProperties.TAB_ID, selectedTab.getId());
    }

    /**
     * This method gets indexes in the {@link TabListModel} of the two tabs that are merged into one
     * group. When moving a Tab to a group, we always put it at the end of the group. For example:
     * move tab1 to tab2 to form a group, tab1 is after tab2 in the TabModel (tab2, tab1); Then
     * move another Tab tab3 to (tab2, tab1) group, tab3 is after tab1, (tab2, tab1, tab3). Thus,
     * the last Tab in the related Tabs is the movedTab. When merging groups merge group1 to group2
     * then the tab will exist in (group2, group1) order. However it is not guaranteed that the
     * tab representing group1 in this model will be the last tab in the group. To account for this
     * start at the front of the group in TabModel index order to find the desIndex of the group or
     * tab to merge to. Then search the rest of the tabs that were merged for srcIndex that was
     * merged from. For undoing multi-group merges the srcIndex may be invalid while the desIndex is
     * always valid as the tab may be moving between existing groups and so has no index in this
     * model of its own.
     *
     * @param tabModel   The tabModel that owns the tabs.
     * @param tabs       The list that contains tabs of the newly merged group.
     * @return A Pair with its first member as the index of the tab that is selected to merge to and
     * the second member as the index of the tab that is being merged from.
     */
    Pair<Integer, Integer> getIndexesForMergeToGroup(TabModel tabModel, List<Tab> tabs) {
        int srcIndex = TabModel.INVALID_TAB_INDEX;
        int desIndex = TabModel.INVALID_TAB_INDEX;

        int startIndex = tabModel.indexOf(tabs.get(0));
        int endIndex = tabModel.indexOf(tabs.get(tabs.size() - 1));
        // Ensure the last tab is last in the model and the first tab is the first.
        assert endIndex - startIndex == tabs.size() - 1;
        for (int i = startIndex; i <= endIndex; i++) {
            Tab curTab = tabModel.getTabAtChecked(i);
            // Group should be contiguous.
            assert tabs.contains(curTab);
            int index = indexFromTabId(curTab.getId());
            if (index != TabModel.INVALID_TAB_INDEX && desIndex == TabModel.INVALID_TAB_INDEX) {
                desIndex = index;
            } else if (index != TabModel.INVALID_TAB_INDEX
                    && srcIndex == TabModel.INVALID_TAB_INDEX) {
                srcIndex = index;
                break;
            }
        }
        return new Pair<>(desIndex, srcIndex);
    }

    /**
     * This method updates the information in {@link TabListModel} of the selected tab when a merge
     * related operation happens.
     *
     * @param index The index of the item in {@link TabListModel} that needs to be updated.
     * @param isSelected Whether the tab is selected or not in a merge related operation. If
     *     selected, update the corresponding item in {@link TabListModel} to the selected state. If
     *     not, restore it to original state.
     */
    void updateSelectedTabForMergeToGroup(int index, boolean isSelected) {
        @Nullable PropertyModel propertyModel = getTabPropertyModel(index);
        if (propertyModel == null) return;

        int status =
                isSelected
                        ? AnimationStatus.SELECTED_CARD_ZOOM_IN
                        : AnimationStatus.SELECTED_CARD_ZOOM_OUT;
        propertyModel.set(TabProperties.CARD_ANIMATION_STATUS, status);
        propertyModel.set(CARD_ALPHA, isSelected ? 0.8f : 1f);
    }

    /**
     * This method updates the information in {@link TabListModel} of the hovered tab when a merge
     * related operation happens.
     *
     * @param index The index of the item in {@link TabListModel} that needs to be updated.
     * @param isHovered Whether the tab is hovered or not in a merge related operation. If hovered,
     *     update the corresponding item in {@link TabListModel} to the hovered state. If not,
     *     restore it to original state.
     */
    void updateHoveredTabForMergeToGroup(int index, boolean isHovered) {
        @Nullable PropertyModel propertyModel = getTabPropertyModel(index);
        if (propertyModel == null) return;

        int status =
                isHovered
                        ? AnimationStatus.HOVERED_CARD_ZOOM_IN
                        : AnimationStatus.HOVERED_CARD_ZOOM_OUT;
        propertyModel.set(TabProperties.CARD_ANIMATION_STATUS, status);
    }

    private @Nullable PropertyModel getTabPropertyModel(int index) {
        if (index < 0 || index >= size()) return null;

        PropertyModel propertyModel = get(index).model;
        assert propertyModel.get(CARD_TYPE) == TAB;
        return propertyModel;
    }
}
