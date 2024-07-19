// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import static org.chromium.android_webview.contextmenu.AwContextMenuItemProperties.MENU_ID;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.widget.ListView;

import androidx.activity.ComponentDialog;
import androidx.annotation.IntDef;

import org.chromium.android_webview.R;
import org.chromium.base.Callback;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUi;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/** The main coordinator for the context menu, responsible for creating the context menu */
public class AwContextMenuCoordinator implements ContextMenuUi {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.DIVIDER, ListItemType.HEADER, ListItemType.CONTEXT_MENU_ITEM})
    public @interface ListItemType {
        int DIVIDER = 0;
        int HEADER = 1;
        int CONTEXT_MENU_ITEM = 2;
    }

    private static final int INVALID_ITEM_ID = -1;

    private ComponentDialog mDialog;
    private ListView mListView;

    AwContextMenuCoordinator() {}

    @Override
    public void dismiss() {
        mDialog.dismiss();
    }

    @Override
    public void displayMenu(
            WindowAndroid window,
            WebContents webContents,
            ContextMenuParams params,
            List<Pair<Integer, ModelList>> items,
            Callback<Integer> onItemClicked,
            Runnable onMenuShown,
            Runnable onMenuClosed) {
        Context context = window.getContext().get();

        View layout =
                LayoutInflater.from(context)
                        .inflate(R.layout.aw_context_menu_fullscreen_container, null);

        View menu = ((ViewStub) layout.findViewById(R.id.aw_context_menu_stub)).inflate();

        mDialog = createComponentDialog(context, layout);
        mDialog.setOnShowListener(dialogInterface -> onMenuShown.run());
        mDialog.setOnDismissListener(dialogInterface -> onMenuClosed.run());

        AwContextMenuHeaderCoordinator headerCoordinator =
                new AwContextMenuHeaderCoordinator(params);

        ListItem headerItem = new ListItem(ListItemType.HEADER, headerCoordinator.getModel());

        ModelList listItems = getItemList(headerItem, items);

        ModelListAdapter adapter =
                new ModelListAdapter(listItems) {
                    @Override
                    public boolean areAllItemsEnabled() {
                        return false;
                    }

                    @Override
                    public boolean isEnabled(int position) {
                        return getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM;
                    }

                    @Override
                    public long getItemId(int position) {
                        if (getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM) {
                            return ((ListItem) getItem(position)).model.get(MENU_ID);
                        }
                        return INVALID_ITEM_ID;
                    }
                };

        mListView = menu.findViewById(R.id.context_menu_list_view);
        mListView.setAdapter(adapter);

        adapter.registerType(
                ListItemType.HEADER,
                new LayoutViewBuilder(R.layout.aw_context_menu_header),
                AwContextMenuHeaderViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM,
                new LayoutViewBuilder(R.layout.aw_context_menu_row),
                AwContextMenuItemViewBinder::bind);

        mListView.setOnItemClickListener(
                (parent, view, position, id) -> {
                    assert id != INVALID_ITEM_ID;

                    clickItem((int) id, window.getActivity().get(), onItemClicked);
                });

        mDialog.show();
    }

    /**
     * Execute an action for the selected item and close the menu.
     *
     * @param id The id of the item.
     * @param activity The current activity.
     * @param onItemClicked The callback to take action with the given id.
     */
    private void clickItem(int id, Activity activity, Callback<Integer> onItemClicked) {
        if (activity.isFinishing() || activity.isDestroyed()) return;

        onItemClicked.onResult(id);
        dismiss();
    }

    static ComponentDialog createComponentDialog(Context context, View layout) {
        ComponentDialog dialog = new ComponentDialog(context);

        dialog.getWindow()
                .getDecorView()
                .setBackgroundDrawable(new ColorDrawable(Color.TRANSPARENT));
        dialog.setContentView(layout);

        return dialog;
    }

    private static ModelList getItemList(
            ListItem headerItem, List<Pair<Integer, ModelList>> items) {
        ModelList itemList = new ModelList();
        itemList.add(headerItem);

        for (Pair<Integer, ModelList> group : items) {
            itemList.addAll(group.second);
        }

        return itemList;
    }
}
