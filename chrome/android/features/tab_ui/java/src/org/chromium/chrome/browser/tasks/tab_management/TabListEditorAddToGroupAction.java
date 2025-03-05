// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.app.Activity;
import android.graphics.drawable.Drawable;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.Token;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilterObserver;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupListBottomSheetCoordinator.TabGroupCreationCallback;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;

import java.util.List;

/** Action to add one or more tabs to a tab group for the {@link TabListEditorMenu}. */
@NullMarked
public class TabListEditorAddToGroupAction extends TabListEditorAction {
    private final Activity mActivity;
    private final TabGroupCreationDialogManager mTabGroupCreationDialogManager;
    private final TabGroupListBottomSheetCoordinatorFactory mFactory;
    private final TabGroupModelFilterObserver mFilterObserver =
            new TabGroupModelFilterObserver() {
                @Override
                public void willCloseTabGroup(Token tabGroupId, boolean isHiding) {
                    updateText();
                }

                @Override
                public void didCreateGroup(
                        List<Tab> tabs,
                        List<Integer> tabOriginalIndex,
                        List<Integer> tabOriginalRootId,
                        List<Token> tabOriginalTabGroupId,
                        String destinationGroupTitle,
                        int destinationGroupColorId,
                        boolean destinationGroupTitleCollapsed) {
                    updateText();
                }
            };

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void tabClosureUndone(Tab tab) {
                    if (tab.getTabGroupId() != null) {
                        updateText();
                    }
                }
            };

    /**
     * Create an action for adding one or more tabs to a tab group.
     *
     * @param activity The current activity.
     * @param tabGroupCreationDialogManager The manager for showing a dialog on group creation.
     * @param showMode Whether to show an action view.
     * @param buttonType The type of the action view.
     * @param iconPosition The position of the icon in the action view.
     */
    public static TabListEditorAction createAction(
            Activity activity,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition) {
        Drawable drawable = AppCompatResources.getDrawable(activity, R.drawable.ic_widgets);
        return new TabListEditorAddToGroupAction(
                activity,
                tabGroupCreationDialogManager,
                showMode,
                buttonType,
                iconPosition,
                drawable,
                TabGroupListBottomSheetCoordinator::new);
    }

    @VisibleForTesting
    /* package */ TabListEditorAddToGroupAction(
            Activity activity,
            TabGroupCreationDialogManager tabGroupCreationDialogManager,
            @ShowMode int showMode,
            @ButtonType int buttonType,
            @IconPosition int iconPosition,
            Drawable drawable,
            TabGroupListBottomSheetCoordinatorFactory factory) {
        super(
                R.id.tab_list_editor_add_tab_to_group_menu_item,
                showMode,
                buttonType,
                iconPosition,
                R.plurals.add_tab_to_group_menu_item,
                R.plurals.accessibility_add_tab_to_group_menu_item,
                drawable);
        mActivity = activity;
        mTabGroupCreationDialogManager = tabGroupCreationDialogManager;
        mFactory = factory;
        setDestroyable(this::destroy);
    }

    @Override
    public void onSelectionStateChange(List<Integer> tabIds) {
        int numTabs =
                editorSupportsActionOnRelatedTabs()
                        ? getTabCountIncludingRelatedTabs(getTabGroupModelFilter(), tabIds)
                        : tabIds.size();
        setEnabledAndItemCount(!tabIds.isEmpty(), numTabs);
    }

    @Override
    public boolean performAction(List<Tab> tabs) {
        assert !tabs.isEmpty() : "Add tab to group action should not be enabled for no tabs.";
        BottomSheetController controller = getActionDelegate().getBottomSheetController();
        TabGroupModelFilter filter = getTabGroupModelFilter();
        Tab destinationTab = tabs.get(0);
        Profile profile = destinationTab.getProfile();

        if (hasTabGroups()) {
            showBottomSheet(tabs, filter, profile, controller);
        } else {
            createNewTabGroup(tabs, filter, destinationTab);
        }

        return true;
    }

    @Override
    public boolean shouldHideEditorAfterAction() {
        return true;
    }

    @Override
    void configure(
            @NonNull Supplier<TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            @NonNull SelectionDelegate<Integer> selectionDelegate,
            @NonNull ActionDelegate actionDelegate,
            boolean editorSupportsActionOnRelatedTabs) {
        super.configure(
                currentTabGroupModelFilterSupplier,
                selectionDelegate,
                actionDelegate,
                editorSupportsActionOnRelatedTabs);
        TabGroupModelFilter filter = getTabGroupModelFilter();
        filter.addTabGroupObserver(mFilterObserver);
        filter.getTabModel().addObserver(mTabModelObserver);
        updateText();
    }

    private void showBottomSheet(
            List<Tab> tabs,
            TabGroupModelFilter filter,
            Profile profile,
            BottomSheetController controller) {
        TabGroupCreationCallback groupCreationCallback =
                tabGroupId -> {
                    int rootId = filter.getRootIdFromTabGroupId(tabGroupId);
                    mTabGroupCreationDialogManager.showDialog(rootId, filter);
                };

        TabGroupListBottomSheetCoordinator coordinator =
                mFactory.create(
                        mActivity, profile, groupCreationCallback, filter, controller, true);
        coordinator.showBottomSheet(tabs);
    }

    private void createNewTabGroup(List<Tab> tabs, TabGroupModelFilter filter, Tab destinationTab) {
        filter.mergeListOfTabsToGroup(tabs, destinationTab, true);
        mTabGroupCreationDialogManager.showDialog(destinationTab.getRootId(), filter);
    }

    private void destroy() {
        TabGroupModelFilter filter = getTabGroupModelFilter();
        filter.removeTabGroupObserver(mFilterObserver);
        filter.getTabModel().removeObserver(mTabModelObserver);
    }

    private boolean hasTabGroups() {
        return getTabGroupModelFilter().getTabGroupCount() != 0;
    }

    private void updateText() {
        if (hasTabGroups()) {
            setActionText(
                    R.plurals.add_tab_to_group_menu_item,
                    R.plurals.accessibility_add_tab_to_group_menu_item);
        } else {
            setActionText(
                    R.plurals.add_tab_to_new_group_menu_item,
                    R.plurals.accessibility_add_tab_to_new_group_menu_item);
        }
    }
}
