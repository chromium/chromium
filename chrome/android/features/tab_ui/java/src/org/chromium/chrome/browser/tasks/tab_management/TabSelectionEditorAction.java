// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.chrome.browser.tasks.tab_groups.TabGroupModelFilter;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;

/**
 * Defines the core action of a {@link TabSelectionEditorMenuItem}.
 */
public abstract class TabSelectionEditorAction {
    @IntDef({ShowMode.MENU_ONLY, ShowMode.IF_ROOM, ShowMode.NUM_ENTRIES})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ShowMode {
        /**
         * Never show an ActionView, only show a menu item.
         */
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
        /**
         * Show text in the ActionView.
         */
        int TEXT = 0;
        /**
         * Show an icon in the ActionView. If an action has no icon then nothing will be shown.
         */
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
        /**
         * Show icon at the start in the ActionView.
         */
        int START = 0;
        /**
         * Show icon at the end in the ActionView.
         */
        int END = 1;
        int NUM_ENTRIES = 2;
    }

    /**
     * Observer for watching if an action is being taken.
     */
    public interface ActionObserver {
        // TODO(ckitagawa): Determine if this can be removed or moved to post processing.

        /**
         * Called at the start of {@link TabSelectionEditorAction#perform()} before an action
         * is executed.
         * @param tabs The list of tabs that will be acted on.
         */
        void preProcessSelectedTabs(List<Tab> tabs);
    }

    /**
     * Delegate for handling additional selection and control actions for the TabSelectionEditor.
     */
    public interface ActionDelegate {
        /**
         * Selects all tabs in the current selection editor.
         */
        void selectAll();

        /**
         * Clears all selected tabs.
         */
        void deselectAll();

        /**
         * Whether all the tabs in the editor are selected.
         */
        boolean areAllTabsSelected();

        /**
         * Hides the selection editor.
         */
        void hideByAction();

        /**
         * Sync position of the client {@link TabListCoordinator}'s RecyclerView with the editor's.
         */
        void syncRecyclerViewPosition();

        /**
         * Retrieves the SnackbarManager for the selection editor.
         */
        SnackbarManager getSnackbarManager();
    }

    private ObserverList<ActionObserver> mObsevers = new ObserverList<>();
    private PropertyModel mModel;
    private TabModelSelector mTabModelSelector;
    private ActionDelegate mActionDelegate;
    private SelectionDelegate<Integer> mSelectionDelegate;
    private Boolean mEditorSupportsActionOnRelatedTabs;

    public TabSelectionEditorAction(int menuItemId, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition, int titleResourceId,
            @Nullable Integer contentDescriptionResourceId, @Nullable Drawable icon) {
        assert showMode >= ShowMode.MENU_ONLY && showMode < ShowMode.NUM_ENTRIES;
        assert buttonType >= ButtonType.TEXT && buttonType < ButtonType.NUM_ENTRIES;
        assert iconPosition >= IconPosition.START && iconPosition < IconPosition.NUM_ENTRIES;

        final String expectedResourceourceTypeName = "plurals";
        boolean titleIsPlural = expectedResourceourceTypeName.equals(
                ContextUtils.getApplicationContext().getResources().getResourceTypeName(
                        titleResourceId));

        mModel =
                new PropertyModel.Builder(TabSelectionEditorActionProperties.ACTION_KEYS)
                        .with(TabSelectionEditorActionProperties.MENU_ITEM_ID, menuItemId)
                        .with(TabSelectionEditorActionProperties.SHOW_MODE, showMode)
                        .with(TabSelectionEditorActionProperties.BUTTON_TYPE, buttonType)
                        .with(TabSelectionEditorActionProperties.ICON_POSITION, iconPosition)
                        .with(TabSelectionEditorActionProperties.TITLE_RESOURCE_ID, titleResourceId)
                        .with(TabSelectionEditorActionProperties.TITLE_IS_PLURAL, titleIsPlural)
                        .with(TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID,
                                contentDescriptionResourceId)
                        .with(TabSelectionEditorActionProperties.ICON, icon)
                        .with(TabSelectionEditorActionProperties.ENABLED, false)
                        .with(TabSelectionEditorActionProperties.ITEM_COUNT, 0)
                        .with(TabSelectionEditorActionProperties.TEXT_TINT,
                                ColorStateList.valueOf(Color.TRANSPARENT))
                        .with(TabSelectionEditorActionProperties.ICON_TINT,
                                ColorStateList.valueOf(Color.TRANSPARENT))
                        .with(TabSelectionEditorActionProperties.ON_CLICK_LISTENER, this::perform)
                        .with(TabSelectionEditorActionProperties.SHOULD_DISMISS_MENU, true)
                        .with(TabSelectionEditorActionProperties.ON_SELECTION_STATE_CHANGE,
                                this::onSelectionStateChange)
                        .build();

        if (contentDescriptionResourceId == null) return;

        assert expectedResourceourceTypeName.equals(
                ContextUtils.getApplicationContext().getResources().getResourceTypeName(
                        contentDescriptionResourceId))
            : "Quantity strings (plurals) with one integer format argument is needed";
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
     * @return Whether the TabSelectionEditor supports applying the actions to related tabs.
     */
    public boolean editorSupportsActionOnRelatedTabs() {
        assert mEditorSupportsActionOnRelatedTabs != null;
        return mEditorSupportsActionOnRelatedTabs;
    }

    /**
     * Actions should override this to decide if an action should be enabled and
     * to provide the enabled state and count to the PropertyModel.
     * @param tabIds the list of selected tab ids.
     * @return Whether the action should be enabled.
     */
    public abstract void onSelectionStateChange(List<Integer> tabIds);

    /**
     * Processes the selected tabs from the selection list this includes related tabs if
     * {@link #editorSupportsActionOnRelatedTabs()} is true.
     * @param tabs a list of tabs from getTabsFromSelection().
     * @return Whether an action was performed without an error.
     */
    public abstract boolean performAction(List<Tab> tabs);

    /**
     * @return Whether to hide the editor after tabking the action.
     */
    public abstract boolean shouldHideEditorAfterAction();

    /**
     * Processes the selected tabs from the selection list.
     * @return whether an action was taken.
     */
    public boolean perform() {
        assert mActionDelegate != null;
        assert mTabModelSelector != null;
        assert mSelectionDelegate != null;

        List<Tab> tabs = editorSupportsActionOnRelatedTabs() ? getTabsAndRelatedTabsFromSelection()
                                                             : getTabsFromSelection();
        if (shouldNotifyObserversOfAction()) {
            for (ActionObserver obs : mObsevers) {
                obs.preProcessSelectedTabs(tabs);
            }
        }
        // When hiding by action it is expected that syncRecyclerViewPosition() is called before the
        // action occurs. This is because an action may remove tabs so it needs to sync position
        // before the removal of items occurs to ensure the positions match correctly for
        // animations.
        if (shouldHideEditorAfterAction()) {
            mActionDelegate.syncRecyclerViewPosition();
        }
        if (!performAction(tabs)) {
            return false;
        };
        if (shouldHideEditorAfterAction()) {
            mActionDelegate.hideByAction();
            RecordUserAction.record("TabMultiSelectV2.ClosedAutomatically");
        }
        return true;
    }

    /**
     * Called by {@link TabModelSelectionEditorMediator} to supply additional dependencies.
     * @param tabModelSelector that this action should act on.
     * @param selectionDelegate to get selected tab IDs from.
     * @param actionDelegate to control the TabSelectionEditor.
     * @param editorSupportsActionOnRelatedTabs whether the TabSelectionEditor supports actions on
     *                                          related tabs.
     */
    void configure(@NonNull TabModelSelector tabModelSelector,
            @NonNull SelectionDelegate<Integer> selectionDelegate,
            @NonNull ActionDelegate actionDelegate, boolean editorSupportsActionOnRelatedTabs) {
        mTabModelSelector = tabModelSelector;
        mSelectionDelegate = selectionDelegate;
        mActionDelegate = actionDelegate;
        mEditorSupportsActionOnRelatedTabs = editorSupportsActionOnRelatedTabs;
    }

    PropertyModel getPropertyModel() {
        return mModel;
    }

    protected @NonNull TabModelSelector getTabModelSelector() {
        assert mTabModelSelector != null;
        return mTabModelSelector;
    }

    protected @NonNull ActionDelegate getActionDelegate() {
        assert mActionDelegate != null;
        return mActionDelegate;
    }

    protected void setEnabledAndItemCount(boolean enabled, int itemCount) {
        mModel.set(TabSelectionEditorActionProperties.ENABLED, enabled);
        mModel.set(TabSelectionEditorActionProperties.ITEM_COUNT, itemCount);
    }

    private List<Tab> getTabsFromSelection() {
        List<Tab> selectedTabs = new ArrayList<>();
        for (int tabId : mSelectionDelegate.getSelectedItems()) {
            Tab tab = TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId);
            if (tab == null) continue;

            selectedTabs.add(tab);
        }
        return selectedTabs;
    }

    protected List<Tab> getTabsAndRelatedTabsFromSelection() {
        if (!(mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                            instanceof TabGroupModelFilter)) {
            return getTabsFromSelection();
        }
        TabGroupModelFilter filter =
                (TabGroupModelFilter) mTabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter();

        List<Tab> tabs = new ArrayList<>();
        for (int tabId : mSelectionDelegate.getSelectedItems()) {
            tabs.addAll(filter.getRelatedTabList(tabId));
        }
        return tabs;
    }

    public static int getTabCountIncludingRelatedTabs(
            TabModelSelector tabModelSelector, List<Integer> tabIds) {
        if (!(tabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter()
                            instanceof TabGroupModelFilter)) {
            return tabIds.size();
        }
        TabGroupModelFilter filter =
                (TabGroupModelFilter) tabModelSelector.getTabModelFilterProvider()
                        .getCurrentTabModelFilter();

        int tabCount = 0;
        for (int tabId : tabIds) {
            Tab tab = TabModelUtils.getTabById(tabModelSelector.getCurrentModel(), tabId);
            tabCount += filter.getRelatedTabCountForRootId(filter.getRootId(tab));
        }
        return tabCount;
    }
}
