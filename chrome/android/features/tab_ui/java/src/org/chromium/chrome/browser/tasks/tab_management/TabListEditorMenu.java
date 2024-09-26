// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.AdapterView;
import android.widget.AdapterView.OnItemClickListener;
import android.widget.ListView;

import androidx.annotation.IntDef;

import org.chromium.chrome.browser.tasks.tab_management.TabListEditorActionViewLayout.ActionViewLayoutDelegate;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.selectable_list.SelectionDelegate;
import org.chromium.ui.listmenu.ListMenu;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * A {@link ListMenu} for the {@link TabListEditorToolbar} that helps manage a
 * {@link TabListEditorActionViewLayout} for Action views. The menu contains a list of
 * {@link TabListEditorMenuItem}s which hold optional action views if room is available.
 */
public class TabListEditorMenu
        implements ListMenu,
                OnItemClickListener,
                SelectionDelegate.SelectionObserver<Integer>,
                ActionViewLayoutDelegate {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.MENU_ITEM})
    public static @interface ListItemType {
        int MENU_ITEM = 0;
    }

    private Context mContext;
    // Insertion ordering is important and for performance it is ok as size is very small.
    private Map<Integer, TabListEditorMenuItem> mMenuItems = new LinkedHashMap<>();

    private View mContentView;
    private ListView mListView;
    private TabListEditorActionViewLayout mActionViewLayout;
    private ModelList mModelList;
    private ModelListAdapter mAdapter;

    /**
     * @param context to use for accessing resources.
     * @param actionViewLayout the actionViewLayout to use.
     */
    public TabListEditorMenu(Context context, TabListEditorActionViewLayout actionViewLayout) {
        mContext = context;
        mActionViewLayout = actionViewLayout;

        mModelList = new ModelList();
        mAdapter =
                new ModelListAdapter(mModelList) {
                    @Override
                    public boolean isEnabled(int position) {
                        // For accessibility on Android Q and earlier even if the View for the item
                        // is disabled the list item may behave as though it is enabled. Pass back
                        // the model state for isEnabled() queries. This is also necessary in some
                        // testing frameworks such as Espresso.
                        return mModelList
                                .get(position)
                                .model
                                .get(TabListEditorActionProperties.ENABLED);
                    }
                };
        registerItemTypes();
        mContentView = LayoutInflater.from(mContext).inflate(R.layout.app_menu_layout, null);
        mListView = mContentView.findViewById(R.id.app_menu_list);
        mListView.setAdapter(mAdapter);
        mListView.setDivider(null);
        mListView.setOnItemClickListener(this);

        mActionViewLayout.setListMenuButtonDelegate(() -> this);
        mActionViewLayout.setActionViewLayoutDelegate(this);
    }

    private void registerItemTypes() {
        mAdapter.registerType(
                ListItemType.MENU_ITEM,
                new LayoutViewBuilder(R.layout.list_menu_item),
                TabListEditorMenuAdapter::bindMenuItem);
    }

    private ListItem buildListItem(int menuItemId) {
        // Model values are populated while configuring the TabListEditorMenuItem.
        return new ListItem(
                ListItemType.MENU_ITEM,
                new PropertyModel.Builder(TabListEditorActionProperties.MENU_ITEM_KEYS)
                        .with(TabListEditorActionProperties.MENU_ITEM_ID, menuItemId)
                        .with(
                                TabListEditorActionProperties.TEXT_APPEARANCE_ID,
                                BrowserUiListMenuUtils.getDefaultTextAppearanceStyle())
                        .build());
    }

    /**
     * Create a {@link TabListEditorMenuItem} for this menu.
     * @param menuItemId the ID to use for the new TabListEditorMenuItem.
     */
    public void add(int menuItemId) {
        ListItem listItem = buildListItem(menuItemId);
        mMenuItems.put(menuItemId, new TabListEditorMenuItem(mContext, listItem));
        mModelList.add(listItem);
    }

    /**
     * Signal that the action view and property model for {@link TabSelecetionEditorMenuItem} are
     * initialized.
     * @param menuItemId the ID of the TabListEditorMenuItem that finished initialization.
     */
    public void menuItemInitialized(int menuItemId) {
        final TabListEditorMenuItem menuItem = mMenuItems.get(menuItemId);
        if (menuItem.getActionView() == null) {
            mActionViewLayout.setHasMenuOnlyItems(true);
        } else {
            mActionViewLayout.add(menuItem);
        }
    }

    /**
     * @param menuItemId the id of the item to get.
     * @return a {@link} TabListEditorMenuItem or null if the key isn't present.
     */
    public TabListEditorMenuItem getMenuItem(int menuItemId) {
        return mMenuItems.get(menuItemId);
    }

    /** Clears all items in the menu. */
    public void clear() {
        mMenuItems.clear();
        mModelList.clear();
        mActionViewLayout.clear();
    }

    /**
     * Delegates selection updates to each menu item.
     * @param selectedItems the currently selected items.
     */
    @Override
    public void onSelectionStateChange(List<Integer> selectedItems) {
        for (TabListEditorMenuItem menuItem : mMenuItems.values()) {
            menuItem.onSelectionStateChange(selectedItems);
        }
    }

    /** {@link OnItemClickListener} implementation. */
    @Override
    public void onItemClick(AdapterView<?> parent, View view, int position, long id) {
        TabListEditorMenuItem item =
                mMenuItems.get(
                        ((ListItem) mAdapter.getItem(position))
                                .model.get(TabListEditorActionProperties.MENU_ITEM_ID));

        if (!item.onClick()) return;

        if (item.shouldDismissMenu()) mActionViewLayout.dismissMenu();
    }

    /** {@link ActionViewLayoutDelegate} implementation. */
    @Override
    public void setVisibleActionViews(Set<TabListEditorMenuItem> visibleActions) {
        if (mModelList.size() == visibleActions.size()) {
            boolean unchanged = true;
            for (TabListEditorMenuItem item : visibleActions) {
                if (mModelList.indexOf(item.getListItem()) == -1) {
                    unchanged = false;
                    break;
                }
            }
            if (unchanged) return;
        }

        // Reset the entire list to maintain the correct ordering.
        mModelList.clear();
        for (TabListEditorMenuItem item : mMenuItems.values()) {
            if (visibleActions.contains(item)) {
                item.setActionViewShowing(true);
                continue;
            }

            item.setActionViewShowing(false);
            mModelList.add(item.getListItem());
        }
        // Resize the list which is necessary if elements are removed.
        mListView.invalidateViews();
    }

    /** {@link ListMenu} implementation. */
    @Override
    public View getContentView() {
        return mContentView;
    }

    @Override
    public void addContentViewClickRunnable(Runnable runnable) {}

    @Override
    public int getMaxItemWidth() {
        return mContext.getResources().getDimensionPixelSize(R.dimen.menu_width);
    }
}
