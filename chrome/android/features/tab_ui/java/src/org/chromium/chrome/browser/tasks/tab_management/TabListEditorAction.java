// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionProperties.DESTROYABLE;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.Token;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tasks.tab_management.TabUiMetricsHelper.TabListEditorExitMetricGroups;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.function.Supplier;

/** Defines the core action of a {@link TabListEditorMenuItem}. */
@NullMarked
public abstract class TabListEditorAction {
    @IntDef({ShowMode.MENU_ONLY, ShowMode.IF_ROOM, ShowMode.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShowMode {
        /** Never show an ActionView, only show a menu item. */
        int MENU_ONLY = 0;

        /**
         * Only show an ActionView if there is room. Priority is based on ordering amongst other
         * {@link MenuItem}s.
         */
        int IF_ROOM = 1;

        int NUM_ENTRIES = 2;
    }

    @IntDef({ButtonType.TEXT, ButtonType.ICON, ButtonType.ICON_AND_TEXT, ButtonType.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonType {
        /** Show text in the ActionView. */
        int TEXT = 0;

        /** Show an icon in the ActionView. If an action has no icon then nothing will be shown. */
        int ICON = 1;

        /**
         * Shows an icon and text for the ActionView. If an action has no icon this is equivalent to
         * {@code TEXT}.
         */
        int ICON_AND_TEXT = 2;

        int NUM_ENTRIES = 3;
    }

    @IntDef({IconPosition.START, IconPosition.END, IconPosition.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface IconPosition {
        /** Show icon at the start in the ActionView. */
        int START = 0;

        /** Show icon at the end in the ActionView. */
        int END = 1;

        int NUM_ENTRIES = 2;
    }

    /** Observer for watching if an action is being taken. */
    public interface ActionObserver {
        // TODO(ckitagawa): Determine if this can be removed or moved to post processing.

        /**
         * Called at the start of {@link TabListEditorAction#perform()} before an action
         * is executed.
         * @param tabs The list of tabs that will be acted on.
         */
        void preProcessSelectedTabs(List<Tab> tabs);
    }

    /**
     * Delegate for handling additional selection and control actions for the TabListEditor.
     */
    public interface ActionDelegate {
        /** Selects all tabs in the current selection editor. */
        void selectAll();

        /** Clears all selected tabs. */
        void deselectAll();

        /** Whether all the tabs in the editor are selected. */
        boolean areAllTabsSelected();

        /** Hides the selection editor. */
        void hideByAction();

        /**
         * Sync position of the client {@link TabListCoordinator}'s RecyclerView with the editor's.
         */
        void syncRecyclerViewPosition();

        /** Retrieves the SnackbarManager for the selection editor. */
        SnackbarManager getSnackbarManager();

        /** Retrieves the BottomSheetController for the selection editor. */
        @Nullable BottomSheetController getBottomSheetController();
    }

    private static final String EXPECTED_RESOURCE_TYPE_NAME = "plurals";

    private final ObserverList<ActionObserver> mObsevers = new ObserverList<>();
    private final PropertyModel mModel;
    private Supplier<@Nullable TabGroupModelFilter> mCurrentTabGroupModelFilterSupplier;
    private ActionDelegate mActionDelegate;
    private SelectionDelegate<TabListEditorItemSelectionId> mSelectionDelegate;
    private Boolean mEditorSupportsActionOnRelatedTabs;

    public TabListEditorAction(
            int menuItemId,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            int titleResourceId,
            @Nullable Integer contentDescriptionResourceId,
            @Nullable Drawable icon) {
        assert showMode >= ShowMode.MENU_ONLY && showMode < ShowMode.NUM_ENTRIES;
        assert buttonType >= ButtonType.TEXT && buttonType < ButtonType.NUM_ENTRIES;
        assert iconPosition >= IconPosition.START && iconPosition < IconPosition.NUM_ENTRIES;
        boolean titleIsPlural = isTitlePlural(titleResourceId);

        mModel =
                new PropertyModel.Builder(TabListEditorActionProperties.ACTION_KEYS)
                        .with(TabListEditorActionProperties.MENU_ITEM_ID, menuItemId)
                        .with(TabListEditorActionProperties.SHOW_MODE, showMode)
                        .with(TabListEditorActionProperties.BUTTON_TYPE, buttonType)
                        .with(TabListEditorActionProperties.ICON_POSITION, iconPosition)
                        .with(TabListEditorActionProperties.TITLE_RESOURCE_ID, titleResourceId)
                        .with(TabListEditorActionProperties.TITLE_IS_PLURAL, titleIsPlural)
                        .with(TabListEditorActionProperties.ENABLED, false)
                        .with(TabListEditorActionProperties.ITEM_COUNT, 0)
                        .with(
                                TabListEditorActionProperties.TEXT_TINT,
                                ColorStateList.valueOf(Color.TRANSPARENT))
                        .with(
                                TabListEditorActionProperties.ICON_TINT,
                                ColorStateList.valueOf(Color.TRANSPARENT))
                        .with(TabListEditorActionProperties.ON_CLICK_LISTENER, this::perform)
                        .with(TabListEditorActionProperties.SHOULD_DISMISS_MENU, true)
                        .with(
                                TabListEditorActionProperties.ON_SELECTION_STATE_CHANGE,
                                this::onSelectionStateChange)
                        .build();

        if (contentDescriptionResourceId != null) {
            mModel.set(
                    TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID,
                    contentDescriptionResourceId);

            assert isTitlePlural(contentDescriptionResourceId)
                    : "Quantity strings (plurals) with one integer format argument is needed";
        }

        if (icon != null) {
            mModel.set(TabListEditorActionProperties.ICON, icon);
        }
    }

    /**
     * @param observer an {@link ActionObserver} to observe when this action occurs.
     */
    public void addActionObserver(ActionObserver observer) {
        mObsevers.addObserver(observer);
    }

    /**
     * @param observer an {@link ActionObserver} to remove.
     */
    public void removeActionObserver(ActionObserver observer) {
        mObsevers.removeObserver(observer);
    }

    /**
     * Defaults to notifying observers of when an action is taken. Should be overridden to false if
     * the action changes the selection state rather than taking an action.
     * @return Whether to notify obsevers of the action.
     */
    public boolean shouldNotifyObserversOfAction() {
        return true;
    }

    /**
     * @return Whether the TabListEditor supports applying the actions to related tabs.
     */
    public boolean editorSupportsActionOnRelatedTabs() {
        assert mEditorSupportsActionOnRelatedTabs != null;
        return mEditorSupportsActionOnRelatedTabs;
    }

    /**
     * Actions should override this to decide if an action should be enabled and to provide the
     * enabled state and count to the PropertyModel.
     *
     * @param itemIds the list of selected tab ids for tabs and sync ids for tab groups.
     */
    public abstract void onSelectionStateChange(List<TabListEditorItemSelectionId> itemIds);

    /**
     * Processes the selected tabs from the selection list.
     *
     * @see #performAction(List, List, MotionEventInfo)
     */
    public final boolean performAction(List<Tab> tabs, List<String> tabGroupSyncIds) {
        return performAction(tabs, tabGroupSyncIds, /* triggeringMotion= */ null);
    }

    /**
     * Processes the selected tabs from the selection list this includes related tabs if {@link
     * #editorSupportsActionOnRelatedTabs()} is true.
     *
     * @param tabs A list of tabs from getTabsFromSelection().
     * @param tabGroupSyncIds A list of tab group sync ids representing {@link SavedTabGroups} that
     *     are selected as indicated in the {@link SelectionDelegate}.
     * @param triggeringMotion the {@link MotionEventInfo} that triggered the action; it is {@code
     *     null} if {@link android.view.MotionEvent} wasn't available when the action was triggered,
     *     such as in {@link android.view.View.OnClickListener}.
     * @return Whether an action was performed without an error.
     */
    public abstract boolean performAction(
            List<Tab> tabs,
            List<String> tabGroupSyncIds,
            @Nullable MotionEventInfo triggeringMotion);

    /**
     * @return Whether to hide the editor after taking the action.
     */
    public abstract boolean shouldHideEditorAfterAction();

    /**
     * Processes the selected tabs from the selection list.
     *
     * @see #perform(MotionEventInfo)
     */
    public final boolean perform() {
        return perform(/* triggeringMotion= */ null);
    }

    /**
     * Processes the selected tabs from the selection list.
     *
     * @param triggeringMotion see {@link #performAction(List, List, MotionEventInfo)}.
     * @return whether an action was taken.
     */
    public boolean perform(@Nullable MotionEventInfo triggeringMotion) {
        assert mActionDelegate != null;
        assert mCurrentTabGroupModelFilterSupplier != null;
        assert mSelectionDelegate != null;

        List<Tab> tabs = getTabsOrTabsAndRelatedTabsFromSelection();
        if (shouldNotifyObserversOfAction()) {
            for (ActionObserver obs : mObsevers) {
                obs.preProcessSelectedTabs(tabs);
            }
        }
        List<String> tabGroupSyncIds = getTabGroupSyncIdsFromSelection();
        // When hiding by action it is expected that syncRecyclerViewPosition() is called before the
        // action occurs. This is because an action may remove tabs so it needs to sync position
        // before the removal of items occurs to ensure the positions match correctly for
        // animations.
        if (shouldHideEditorAfterAction()) {
            mActionDelegate.syncRecyclerViewPosition();
        }
        if (!performAction(tabs, tabGroupSyncIds, triggeringMotion)) {
            return false;
        }

        if (shouldHideEditorAfterAction()) {
            mActionDelegate.hideByAction();
            TabUiMetricsHelper.recordSelectionEditorExitMetrics(
                    TabListEditorExitMetricGroups.CLOSED_AUTOMATICALLY, tabs.get(0).getContext());
        }
        return true;
    }

    /**
     * Called by {@link TabListEditorMediator} to supply additional dependencies.
     *
     * @param currentTabGroupModelFilterSupplier that this action should act on.
     * @param selectionDelegate to get selected tab IDs from.
     * @param actionDelegate to control the TabListEditor.
     * @param editorSupportsActionOnRelatedTabs whether the TabListEditor supports actions on
     *     related tabs.
     */
    @Initializer
    void configure(
            Supplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            SelectionDelegate<TabListEditorItemSelectionId> selectionDelegate,
            ActionDelegate actionDelegate,
            boolean editorSupportsActionOnRelatedTabs) {
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mSelectionDelegate = selectionDelegate;
        mActionDelegate = actionDelegate;
        mEditorSupportsActionOnRelatedTabs = editorSupportsActionOnRelatedTabs;
        onSelectionStateChange(mSelectionDelegate.getSelectedItemsAsList());
    }

    PropertyModel getPropertyModel() {
        return mModel;
    }

    protected TabGroupModelFilter getTabGroupModelFilter() {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assert filter != null;
        return filter;
    }

    protected ActionDelegate getActionDelegate() {
        assert mActionDelegate != null;
        return mActionDelegate;
    }

    protected void setEnabledAndItemCount(boolean enabled, int itemCount) {
        mModel.set(TabListEditorActionProperties.ENABLED, enabled);
        mModel.set(TabListEditorActionProperties.ITEM_COUNT, itemCount);
    }

    private List<Tab> getTabsFromSelection() {
        List<Tab> selectedTabs = new ArrayList<>();
        for (TabListEditorItemSelectionId itemId : mSelectionDelegate.getSelectedItems()) {
            // Only items of type tabId representing a tab are considered. Synced tab groups
            // represented by a syncId will be ignored.
            if (itemId.isTabId()) {
                Tab tab = getTabGroupModelFilter().getTabModel().getTabById(itemId.getTabId());
                if (tab == null) continue;

                selectedTabs.add(tab);
            }
        }
        return selectedTabs;
    }

    private List<Tab> getTabsAndRelatedTabsFromSelection() {
        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);

        List<Tab> tabs = new ArrayList<>();
        for (TabListEditorItemSelectionId itemId : mSelectionDelegate.getSelectedItems()) {
            // Only items of type tabId representing a tab are considered. Synced tab groups
            // represented by a syncId will be ignored.
            if (itemId.isTabId()) {
                tabs.addAll(filter.getRelatedTabList(itemId.getTabId()));
            }
        }
        return tabs;
    }

    protected List<Tab> getTabsOrTabsAndRelatedTabsFromSelection() {
        return editorSupportsActionOnRelatedTabs()
                ? getTabsAndRelatedTabsFromSelection()
                : getTabsFromSelection();
    }

    private List<String> getTabGroupSyncIdsFromSelection() {
        List<String> tabGroupSyncIds = new ArrayList<>();
        for (TabListEditorItemSelectionId itemId : mSelectionDelegate.getSelectedItems()) {
            // Only items of type syncId representing a {@link SavedTabGroup} are considered.
            // Regular tabs or other representations of tab groups will be ignored.
            if (itemId.isTabGroupSyncId()) {
                tabGroupSyncIds.add(itemId.getTabGroupSyncId());
            }
        }
        return tabGroupSyncIds;
    }

    protected void setDestroyable(Destroyable destroyable) {
        mModel.set(DESTROYABLE, destroyable);
    }

    protected void setActionText(int titleRes, int descRes) {
        PropertyModel model = getPropertyModel();
        model.set(TabListEditorActionProperties.TITLE_RESOURCE_ID, titleRes);
        model.set(TabListEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID, descRes);
        model.set(TabListEditorActionProperties.TITLE_IS_PLURAL, isTitlePlural(titleRes));
    }

    private static boolean isTitlePlural(int titleResourceId) {
        return EXPECTED_RESOURCE_TYPE_NAME.equals(
                ContextUtils.getApplicationContext()
                        .getResources()
                        .getResourceTypeName(titleResourceId));
    }

    public static int getTabCountIncludingRelatedTabs(
            TabGroupModelFilter tabGroupModelFilter, List<TabListEditorItemSelectionId> itemIds) {
        int tabCount = 0;
        for (TabListEditorItemSelectionId itemId : itemIds) {
            if (itemId.isTabId()) {
                Tab tab = tabGroupModelFilter.getTabModel().getTabById(itemId.getTabId());
                // TODO(crbug.com/41495189): Find out how we can have a tab ID that is no longer
                // in the tab model here.
                if (tab == null) continue;

                @Nullable Token tabGroupId = tab.getTabGroupId();
                if (tabGroupId != null) {
                    tabCount += tabGroupModelFilter.getTabCountForGroup(tabGroupId);
                } else {
                    tabCount++;
                }
            } else if (itemId.isTabGroupSyncId()) {
                String syncId = itemId.getTabGroupSyncId();
                if (syncId == null) continue;
                tabCount += 1;
            } else {
                assert false : "Unexpected itemId type.";
            }
        }
        return tabCount;
    }
}
