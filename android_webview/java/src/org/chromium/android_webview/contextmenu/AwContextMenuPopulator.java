// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.contextmenu;

import static org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems.COPY_LINK_ADDRESS;
import static org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems.COPY_LINK_TEXT;
import static org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems.OPEN_LINK;
import static org.chromium.android_webview.contextmenu.AwContextMenuItemProperties.ICON_DRAWABLE;
import static org.chromium.android_webview.contextmenu.AwContextMenuItemProperties.MENU_ID;
import static org.chromium.android_webview.contextmenu.AwContextMenuItemProperties.TEXT;

import android.app.Activity;
import android.content.Context;

import androidx.annotation.DrawableRes;
import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.android_webview.AwSettings.HyperlinkContextMenuItems;
import org.chromium.android_webview.R;
import org.chromium.android_webview.contextmenu.AwContextMenuCoordinator.ListItemType;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.embedder_support.contextmenu.ChipDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuItemDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuPopulator;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.ArrayList;
import java.util.List;

/** A {@link ContextMenuPopulator} used for showing the WebView context menu. */
@NullMarked
public class AwContextMenuPopulator implements ContextMenuPopulator {
    private final Context mContext;
    private final ContextMenuItemDelegate mItemDelegate;
    private final ContextMenuParams mParams;
    private final boolean mUsePopupWindow;
    private final @HyperlinkContextMenuItems int mMenuItems;

    /**
     * Builds a {@link AwContextMenuPopulator}.
     *
     * @param context The {@link Context} used to retrieve the strings.
     * @param activity The {@link Activity} used to create the {@link ContextMenuItemDelegate}.
     * @param webContents The {@link WebContents} that this context menu belongs to.
     * @param params The {@link ContextMenuParams} to populate the menu items.
     */
    public AwContextMenuPopulator(
            Context context,
            Activity activity,
            WebContents webContents,
            ContextMenuParams params,
            boolean usePopupWindow,
            @HyperlinkContextMenuItems int menuItems) {
        mContext = context;
        mParams = params;
        mUsePopupWindow = usePopupWindow;

        mItemDelegate = new AwContextMenuItemDelegate(activity, webContents);
        mMenuItems = menuItems;
    }

    @Override
    public List<ModelList> buildContextMenu() {
        List<ModelList> groupedItems = new ArrayList<>();

        ModelList items = new ModelList();

        if ((mMenuItems & COPY_LINK_ADDRESS) != 0) {
            items.add(
                    createListItem(
                            R.id.contextmenu_copy_link_address,
                            R.string.context_menu_copy_link_address,
                            mUsePopupWindow ? R.drawable.ic_link : 0));
        }
        if ((mMenuItems & COPY_LINK_TEXT) != 0) {
            items.add(
                    createListItem(
                            R.id.contextmenu_copy_link_text,
                            R.string.context_menu_copy_link_text,
                            mUsePopupWindow ? R.drawable.ic_content_copy : 0));
        }
        if ((mMenuItems & OPEN_LINK) != 0) {
            items.add(
                    createListItem(
                            R.id.contextmenu_open_link_id,
                            R.string.context_menu_open_link,
                            mUsePopupWindow ? R.drawable.ic_open_in_new : 0));
        }

        if (!items.isEmpty()) {
            groupedItems.add(items);
        }

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
        } else if (itemId == R.id.contextmenu_open_link_id) {
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

    private ListItem createListItem(
            int menuId, @StringRes int stringId, @DrawableRes int drawableId) {
        PropertyModel.Builder builder =
                new PropertyModel.Builder(AwContextMenuItemProperties.ALL_KEYS)
                        .with(MENU_ID, menuId)
                        .with(TEXT, mContext.getString(stringId));

        if (drawableId != 0) {
            builder.with(ICON_DRAWABLE, ContextCompat.getDrawable(mContext, drawableId));
        }

        return new ListItem(ListItemType.CONTEXT_MENU_ITEM, builder.build());
    }
}
