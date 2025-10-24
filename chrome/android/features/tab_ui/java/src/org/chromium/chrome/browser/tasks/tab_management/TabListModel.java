// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MESSAGE_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ALPHA;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_ANIMATION_STATUS;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.CARD_TYPE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.MESSAGE;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB;
import static org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType.TAB_GROUP;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_GROUP_SYNC_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabProperties.TAB_ID;
import static org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType.ARCHIVED_TABS_MESSAGE;

import android.util.Pair;

import androidx.annotation.IntDef;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
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
public class TabListModel extends ModelList {
    @IntDef({
        AnimationStatus.SELECTED_CARD_ZOOM_IN,
        AnimationStatus.SELECTED_CARD_ZOOM_OUT,
        AnimationStatus.HOVERED_CARD_ZOOM_IN,
        AnimationStatus.HOVERED_CARD_ZOOM_OUT,
        AnimationStatus.CARD_RESTORE
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface AnimationStatus {
        int CARD_RESTORE = 0;
        int SELECTED_CARD_ZOOM_OUT = 1;
        int SELECTED_CARD_ZOOM_IN = 2;
        int HOVERED_CARD_ZOOM_OUT = 3;
        int HOVERED_CARD_ZOOM_IN = 4;
        int NUM_ENTRIES = 5;
    }

    /** Required properties for each {@link PropertyModel} managed by this {@link ModelList}. */
    public static class CardProperties {
        static final long BASE_ANIMATION_DURATION_MS = 218;

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
        public static final PropertyModel.WritableIntPropertyKey CARD_ANIMATION_STATUS =
                new PropertyModel.WritableIntPropertyKey();
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
    public int lastIndexForMessageItemFromType(
            @TabSwitcherMessageManager.MessageType int messageType) {
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
     * This method gets indexes in the {@link TabListModel} of the tab cards that are merged into a
     * group. This should always produce a valid destination index which is the index in the {@link
     * TabListModel} that the moved tab should exist in. The source index may be invalid if a group
     * of size 1 is created or the tab was moved between groups. In the case of moving between
     * groups as the other group will be updated by {@link
     * TabGroupModelFilterObserver#didMoveTabOutOfGroup(Tab, int)}.
     *
     * @param tabModel The tabModel that owns the tabs.
     * @param movedTab The tab that is being merged.
     * @param isDestinationTab Whether the moved tab is being merged to the group or is the
     *     destination.
     * @param tabs The list that contains tabs of the newly merged group.
     * @return A Pair with its first member as the index that is merged to and the the second member
     *     as the index that is being merged from.
     */
    Pair<Integer, Integer> getIndexesForMergeToGroup(
            TabModel tabModel, Tab movedTab, boolean isDestinationTab, List<Tab> tabs) {
        // The moved tab is always involved in the merge, but it may not have an index if it was
        // moved between groups.
        int movedTabListModelIndex = indexFromTabId(movedTab.getId());

        // TODO(crbug.com/433947821): The use of TabModel here is probably overkill. Consider
        // iterating through just tabs.

        // Find the other index that is involved in the merge it should be in the list of tabs.
        int otherTabListModelIndex = TabModel.INVALID_TAB_INDEX;
        int startIndex = tabModel.indexOf(tabs.get(0));
        int endIndex = tabModel.indexOf(tabs.get(tabs.size() - 1));
        // Ensure the last tab is last in the model and the first tab is the first.
        assert endIndex - startIndex == tabs.size() - 1;
        for (int i = startIndex; i <= endIndex; i++) {
            Tab curTab = tabModel.getTabAtChecked(i);
            // Group should be contiguous.
            assert tabs.contains(curTab);
            if (curTab == movedTab) continue;

            otherTabListModelIndex = indexFromTabId(curTab.getId());
            if (otherTabListModelIndex != TabModel.INVALID_TAB_INDEX) break;
        }

        // If nothing is found in the model early return, this might be a case of tab group undo.
        if (movedTabListModelIndex == TabModel.INVALID_TAB_INDEX
                && otherTabListModelIndex == TabModel.INVALID_TAB_INDEX) {
            return new Pair<>(TabModel.INVALID_TAB_INDEX, TabModel.INVALID_TAB_INDEX);
        }

        final int desIndex;
        final int srcIndex;
        if (isDestinationTab || otherTabListModelIndex == TabModel.INVALID_TAB_INDEX) {
            // We allow failing to find the other index as it might be a case of tab group undo
            // which has a intermediate sequencing and model updates that can result in failing to
            // find the tab among the related tabs.

            // The moved tab is the destination tab and should always be in the model.
            assert movedTabListModelIndex != TabModel.INVALID_TAB_INDEX;

            desIndex = movedTabListModelIndex;
            srcIndex = otherTabListModelIndex;
        } else {
            // The other tab is the destination tab and should always be in the model.
            desIndex = otherTabListModelIndex;
            srcIndex = movedTabListModelIndex;
        }
        return new Pair<>(desIndex, srcIndex);
    }

    /**
     * This method updates the information in {@link TabListModel} of the selected card when it is
     * selected or deselected.
     *
     * @param index The index of the item in {@link TabListModel} that needs to be updated.
     * @param isSelected Whether the tab is selected or not. If selected, update the corresponding
     *     item in {@link TabListModel} to the selected state. If not, restore it to original state.
     */
    public void updateSelectedCardForSelection(int index, boolean isSelected) {
        @Nullable PropertyModel propertyModel = getModelSupportingAnimations(index);
        if (propertyModel == null) return;

        int status =
                isSelected
                        ? AnimationStatus.SELECTED_CARD_ZOOM_IN
                        : AnimationStatus.SELECTED_CARD_ZOOM_OUT;
        propertyModel.set(CARD_ANIMATION_STATUS, status);
        propertyModel.set(CARD_ALPHA, isSelected ? 0.8f : 1f);
    }

    /**
     * This method updates the information in {@link TabListModel} of a card when a selected card is
     * hovered over it or moved off the previously hovered card.
     *
     * @param index The index of the item in {@link TabListModel} that needs to be updated.
     * @param isHovered Whether a card is hovered over the card represented by `index` or not. If
     *     hovered, update the corresponding item in {@link TabListModel} to the hovered state. If
     *     not, restore it to original state.
     */
    void updateHoveredCardForHover(int index, boolean isHovered) {
        @Nullable PropertyModel propertyModel = getModelSupportingAnimations(index);
        if (propertyModel == null) return;

        int status =
                isHovered
                        ? AnimationStatus.HOVERED_CARD_ZOOM_IN
                        : AnimationStatus.HOVERED_CARD_ZOOM_OUT;
        propertyModel.set(CARD_ANIMATION_STATUS, status);
    }

    private @Nullable PropertyModel getModelSupportingAnimations(int index) {
        if (index < 0 || index >= size()) return null;

        PropertyModel model = get(index).model;

        boolean isArchiveMessageCard =
                model.get(CARD_TYPE) == MESSAGE && model.get(MESSAGE_TYPE) == ARCHIVED_TABS_MESSAGE;
        if (isArchiveMessageCard && !ChromeFeatureList.sTabArchivalDragDropAndroid.isEnabled()) {
            return null;
        }

        assert model.get(CARD_TYPE) == TAB || isArchiveMessageCard;
        return model;
    }
}
