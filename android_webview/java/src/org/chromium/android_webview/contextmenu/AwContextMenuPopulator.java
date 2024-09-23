// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import static org.chromium.android_webview.contextmenu.AwContextMenuItemProperties.MENU_ID;
import static org.chromium.android_webview.contextmenu.AwContextMenuItemProperties.TEXT;

import android.content.Context;
import android.util.Pair;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.android_webview.R;
import org.chromium.android_webview.contextmenu.AwContextMenuCoordinator.ListItemType;
import org.chromium.android_webview.contextmenu.AwContextMenuItem.Item;
import org.chromium.components.embedder_support.contextmenu.ChipDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** A {@link ContextMenuPopulator} used for showing the WebView context menu. */
public class AwContextMenuPopulator implements ContextMenuPopulator {
    private final Context mContext;
    private final ContextMenuItemDelegate mItemDelegate;
    private final ContextMenuParams mParams;

    /**
     * Builds a {@link AwContextMenuPopulator}.
     *
     * @param context The {@link Context} used to retrieve the strings.
     * @param itemDelegate The {@link ContextMenuItemDelegate} that will be notified with actions to
     *     perform when menu items are selected.
     * @param params The {@link ContextMenuParams} to populate the menu items.
     */
    public AwContextMenuPopulator(
            Context context, ContextMenuItemDelegate itemDelegate, ContextMenuParams params) {
        mContext = context;
        mItemDelegate = itemDelegate;
        mParams = params;
    }

    @Override
    public List<Pair<Integer, ModelList>> buildContextMenu() {
        List<Pair<Integer, ModelList>> groupedItems = new ArrayList<>();

        ModelList items = new ModelList();

        items.add(createListItem(Item.COPY_LINK_TEXT));
        items.add(createListItem(Item.COPY_LINK_ADDRESS));
        items.add(createListItem(Item.OPEN_IN_BROWSER));

        groupedItems.add(new Pair<>(R.string.context_menu_copy_link_text, items));

        return groupedItems;
    }

    @Override
    public boolean isIncognito() {
        return mItemDelegate.isIncognito();
    }

    @Override
    public String getPageTitle() {
        return mItemDelegate.getPageTitle();
    }

    @Override
    public boolean onItemSelected(int itemId) {
        if (itemId == R.id.contextmenu_copy_link_address) {
            mItemDelegate.onSaveToClipboard(
                    mParams.getUnfilteredLinkUrl().getSpec(),
                    ContextMenuItemDelegate.ClipboardType.LINK_URL);
        } else if (itemId == R.id.contextmenu_copy_link_text) {
            mItemDelegate.onSaveToClipboard(
                    mParams.getLinkText(), ContextMenuItemDelegate.ClipboardType.LINK_TEXT);
        } else if (itemId == R.id.contextmenu_open_in_browser_id) {
            mItemDelegate.onOpenInDefaultBrowser(mParams.getUrl());
        } else {
            assert false;
        }

        return true;
    }

    @Override
    public void onMenuClosed() {}

    @Override
    public @Nullable ChipDelegate getChipDelegate() {
        return null;
    }

    @VisibleForTesting
    public Integer getMenuId(PropertyModel model) {
        return model.get(MENU_ID);
    }

    private ListItem createListItem(@Item int item) {
        PropertyModel model =
                new PropertyModel.Builder(AwContextMenuItemProperties.ALL_KEYS)
                        .with(MENU_ID, AwContextMenuItem.getMenuId(item))
                        .with(TEXT, AwContextMenuItem.getTitle(mContext, item))
                        .build();
        return new ListItem(ListItemType.CONTEXT_MENU_ITEM, model);
    }
}
