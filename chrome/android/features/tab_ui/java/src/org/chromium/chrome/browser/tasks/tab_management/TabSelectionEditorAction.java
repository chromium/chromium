// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.graphics.drawable.Drawable;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;

import org.chromium.base.ContextUtils;
import org.chromium.base.ObserverList;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
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
         * Called at the start of {@link TabSelectionEditorAction#performAction()} before an action
         * is executed.
         * @param tabs The list of tabs that will be acted on.
         */
        void preProcessSelectedTabs(List<Tab> tabs);
    }

    private ObserverList<ActionObserver> mObsevers = new ObserverList<>();
    private PropertyModel mModel;
    protected TabModelSelector mTabModelSelector;
    private SelectionDelegate<Integer> mSelectionDelegate;

    public TabSelectionEditorAction(int menuItemId, @ShowMode int showMode,
            @ButtonType int buttonType, @IconPosition int iconPosition, int titleResourceId,
            @Nullable Integer contentDescriptionResourceId, @Nullable Drawable icon) {
        assert showMode >= ShowMode.MENU_ONLY && showMode < ShowMode.NUM_ENTRIES;
        assert buttonType >= ButtonType.TEXT && buttonType < ButtonType.NUM_ENTRIES;
        assert iconPosition >= IconPosition.START && iconPosition < IconPosition.NUM_ENTRIES;

        mModel =
                new PropertyModel.Builder(TabSelectionEditorActionProperties.ALL_KEYS)
                        .with(TabSelectionEditorActionProperties.MENU_ITEM_ID, menuItemId)
                        .with(TabSelectionEditorActionProperties.SHOW_MODE, showMode)
                        .with(TabSelectionEditorActionProperties.BUTTON_TYPE, buttonType)
                        .with(TabSelectionEditorActionProperties.ICON_POSITION, iconPosition)
                        .with(TabSelectionEditorActionProperties.TITLE_RESOURCE_ID, titleResourceId)
                        .with(TabSelectionEditorActionProperties.CONTENT_DESCRIPTION_RESOURCE_ID,
                                contentDescriptionResourceId)
                        .with(TabSelectionEditorActionProperties.ICON, icon)
                        .with(TabSelectionEditorActionProperties.ENABLED, false)
                        .with(TabSelectionEditorActionProperties.ITEM_COUNT, 0)
                        .with(TabSelectionEditorActionProperties.ON_CLICK_LISTENER,
                                this::performAction)
                        .with(TabSelectionEditorActionProperties.ON_SELECTION_STATE_CHANGED,
                                this::onSelectionStateChanged)
                        .build();

        if (contentDescriptionResourceId == null) return;

        final String expectedResourceourceTypeName = "plurals";
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
     * Actions should override this to decide if an action should be enabled and
     * to provide the enabled state and count to the PropertyModel.
     * @param tabIds the list of selected tab ids.
     * @return Whether the action should be enabled.
     */
    public abstract void onSelectionStateChanged(List<Integer> tabIds);

    /**
     * Processes the selected tabs from the selection list. Override this and call
     * {@code super.performAction()} as the first action in the method if taking a standard action.
     * If the action only changes the list of selected tabs do not call the super method.
     * @return whether an action was taken.
     */
    public boolean performAction() {
        List<Tab> tabs = getTabsFromSelection();

        for (ActionObserver obs : mObsevers) {
            obs.preProcessSelectedTabs(tabs);
        }
        return true;
    }

    /**
     * Called by {@link TabModelSelectionEditorMediator} to supply additional dependencies.
     * TODO(ckitagawa): Supply a delegate for additional dependencies on TabSelectionEditorMediator.
     */
    void configure(
            TabModelSelector tabModelSelector, SelectionDelegate<Integer> selectionDelegate) {
        mTabModelSelector = tabModelSelector;
        mSelectionDelegate = selectionDelegate;
    }

    PropertyModel getPropertyModel() {
        return mModel;
    }

    protected void setEnabledAndItemCount(boolean enabled, int itemCount) {
        mModel.set(TabSelectionEditorActionProperties.ENABLED, enabled);
        mModel.set(TabSelectionEditorActionProperties.ITEM_COUNT, itemCount);
    }

    protected List<Tab> getTabsFromSelection() {
        assert mTabModelSelector != null;
        assert mSelectionDelegate != null;

        List<Tab> selectedTabs = new ArrayList<>();
        for (int tabId : mSelectionDelegate.getSelectedItems()) {
            Tab tab = TabModelUtils.getTabById(mTabModelSelector.getCurrentModel(), tabId);
            if (tab == null) continue;

            selectedTabs.add(tab);
        }
        return selectedTabs;
    }
}
