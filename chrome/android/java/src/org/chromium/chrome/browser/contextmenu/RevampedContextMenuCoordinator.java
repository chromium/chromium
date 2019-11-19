// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.support.v7.app.AlertDialog;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.share.ShareParams;
import org.chromium.chrome.browser.ui.widget.ContextMenuDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;

/**
 * The main coordinator for the Revamped Context Menu, responsible for creating the context menu in
 * general and the header component.
 */
public class RevampedContextMenuCoordinator implements ContextMenuUi {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ListItemType.DIVIDER, ListItemType.HEADER, ListItemType.CONTEXT_MENU_ITEM,
            ListItemType.CONTEXT_MENU_SHARE_ITEM})
    public @interface ListItemType {
        int DIVIDER = 0;
        int HEADER = 1;
        int CONTEXT_MENU_ITEM = 2;
        int CONTEXT_MENU_SHARE_ITEM = 3;
    }

    private static final int INVALID_ITEM_ID = -1;

    private RevampedContextMenuHeaderCoordinator mHeaderCoordinator;

    private RevampedContextMenuListView mListView;
    private float mTopContentOffsetPx;
    private ContextMenuDialog mDialog;
    private Runnable mOnShareImageDirectly;
    private Callback<Boolean> mOnMenuClosed;

    /**
     * Constructor that also sets the content offset.
     *
     * @param topContentOffsetPx content offset from the top.
     * @param onShareImageDirectly ContextMenuHelper method to be used to share the image directly.
     */
    RevampedContextMenuCoordinator(float topContentOffsetPx, Runnable onShareImageDirectly) {
        mTopContentOffsetPx = topContentOffsetPx;
        mOnShareImageDirectly = onShareImageDirectly;
    }

    @Override
    public void displayMenu(final WindowAndroid window, ContextMenuParams params,
            List<Pair<Integer, List<ContextMenuItem>>> items, Callback<Integer> onItemClicked,
            final Runnable onMenuShown, final Callback<Boolean> onMenuClosed) {
        mOnMenuClosed = onMenuClosed;
        Activity activity = window.getActivity().get();
        final float density = activity.getResources().getDisplayMetrics().density;
        final float touchPointXPx = params.getTriggeringTouchXDp() * density;
        final float touchPointYPx = params.getTriggeringTouchYDp() * density;

        final View view =
                LayoutInflater.from(activity).inflate(R.layout.revamped_context_menu, null);
        mDialog = createContextMenuDialog(activity, view, touchPointXPx, touchPointYPx);
        mDialog.setOnShowListener(dialogInterface -> onMenuShown.run());
        mDialog.setOnDismissListener(dialogInterface -> mOnMenuClosed.onResult(false));

        mHeaderCoordinator = new RevampedContextMenuHeaderCoordinator(activity, params);

        // The Integer here specifies the {@link ListItemType}.
        ModelList listItems = getItemList(window, items, params);

        ModelListAdapter adapter = new ModelListAdapter(listItems) {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }

            @Override
            public boolean isEnabled(int position) {
                return getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM
                        || getItemViewType(position) == ListItemType.CONTEXT_MENU_SHARE_ITEM;
            }

            @Override
            public long getItemId(int position) {
                if (getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM
                        || getItemViewType(position) == ListItemType.CONTEXT_MENU_SHARE_ITEM) {
                    return ((ListItem) getItem(position))
                            .model.get(RevampedContextMenuItemProperties.MENU_ID);
                }
                return INVALID_ITEM_ID;
            }
        };

        mListView = view.findViewById(R.id.context_menu_list_view);
        mListView.setAdapter(adapter);

        // Note: clang-format does a bad job formatting lambdas so we turn it off here.
        // clang-format off
        adapter.registerType(
                ListItemType.HEADER,
                mHeaderCoordinator::getView,
                RevampedContextMenuHeaderViewBinder::bind);
        adapter.registerType(
                ListItemType.DIVIDER,
                () -> LayoutInflater.from(mListView.getContext())
                        .inflate(R.layout.context_menu_divider, null),
                (m, v, p) -> {});
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM,
                () -> LayoutInflater.from(mListView.getContext())
                        .inflate(R.layout.revamped_context_menu_row, null),
                RevampedContextMenuItemViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_SHARE_ITEM,
                () -> LayoutInflater.from(mListView.getContext())
                        .inflate(R.layout.revamped_context_menu_share_row, null),
                RevampedContextMenuShareItemViewBinder::bind);
        // clang-format on

        mListView.setOnItemClickListener((p, v, pos, id) -> {
            assert id != INVALID_ITEM_ID;

            onItemClicked.onResult((int) id);
            mDialog.dismiss();
        });

        mDialog.show();
    }

    /**
     * Returns the fully complete dialog based off the params and the itemGroups.
     *
     * @param activity Used to inflate the dialog.
     * @param view The inflated view, including the scrim, that contains the list view.
     * @param touchPointYPx The x-coordinate of the touch that triggered the context menu.
     * @param touchPointXPx The y-coordinate of the touch that triggered the context menu.
     * @return Returns a final dialog that does not have a background can be displayed using
     *         {@link AlertDialog#show()}.
     */
    private ContextMenuDialog createContextMenuDialog(
            Activity activity, View view, float touchPointXPx, float touchPointYPx) {
        View frame = view.findViewById(R.id.context_menu_frame);
        // TODO(sinansahin): Refactor ContextMenuDialog as well.
        final ContextMenuDialog dialog =
                new ContextMenuDialog(activity, R.style.Theme_Chromium_AlertDialog, touchPointXPx,
                        touchPointYPx, mTopContentOffsetPx, frame);
        dialog.setContentView(view);

        return dialog;
    }

    @VisibleForTesting
    ModelList getItemList(WindowAndroid window, List<Pair<Integer, List<ContextMenuItem>>> items,
            ContextMenuParams params) {
        Activity activity = window.getActivity().get();
        ModelList itemList = new ModelList();

        // TODO(sinansahin): We should be able to remove this conversion once we can get the items
        // in the desired format.
        itemList.add(new ListItem(ListItemType.HEADER, mHeaderCoordinator.getModel()));

        for (Pair<Integer, List<ContextMenuItem>> group : items) {
            // Add a divider
            itemList.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));

            for (ContextMenuItem item : group.second) {
                PropertyModel itemModel;
                if (item instanceof ShareContextMenuItem) {
                    final ShareContextMenuItem shareItem = ((ShareContextMenuItem) item);
                    final Pair<Drawable, CharSequence> shareInfo = shareItem.getShareInfo();
                    itemModel =
                            new PropertyModel
                                    .Builder(RevampedContextMenuShareItemProperties.ALL_KEYS)
                                    .with(RevampedContextMenuShareItemProperties.MENU_ID,
                                            item.getMenuId())
                                    .with(RevampedContextMenuItemProperties.TEXT,
                                            item.getTitle(activity))
                                    .with(RevampedContextMenuShareItemProperties.IMAGE,
                                            shareInfo.first)
                                    .with(RevampedContextMenuShareItemProperties.CONTENT_DESC,
                                            shareInfo.second)
                                    .with(RevampedContextMenuShareItemProperties.CLICK_LISTENER,
                                            getShareItemClickListener(window, shareItem, params))
                                    .build();
                    itemList.add(new ListItem(ListItemType.CONTEXT_MENU_SHARE_ITEM, itemModel));
                } else {
                    itemModel =
                            new PropertyModel.Builder(RevampedContextMenuItemProperties.ALL_KEYS)
                                    .with(RevampedContextMenuItemProperties.MENU_ID,
                                            item.getMenuId())
                                    .with(RevampedContextMenuItemProperties.TEXT,
                                            item.getTitle(activity))
                                    .build();
                    itemList.add(new ListItem(ListItemType.CONTEXT_MENU_ITEM, itemModel));
                }
            }
        }

        return itemList;
    }

    private View.OnClickListener getShareItemClickListener(
            WindowAndroid window, ShareContextMenuItem item, ContextMenuParams params) {
        return (v) -> {
            ChromeContextMenuPopulator.ContextMenuUma.record(params,
                    item.isShareLink()
                            ? ChromeContextMenuPopulator.ContextMenuUma.Action.DIRECT_SHARE_LINK
                            : ChromeContextMenuPopulator.ContextMenuUma.Action.DIRECT_SHARE_IMAGE);
            mDialog.setOnDismissListener(dialogInterface -> mOnMenuClosed.onResult(true));
            mDialog.dismiss();
            if (item.isShareLink()) {
                final ShareParams shareParams =
                        new ShareParams.Builder(window, params.getUrl(), params.getUrl())
                                .setShareDirectly(true)
                                .setSaveLastUsed(false)
                                .build();
                ShareHelper.shareDirectly(shareParams);
            } else {
                mOnShareImageDirectly.run();
            }
        };
    }

    Callback<Bitmap> getOnImageThumbnailRetrievedReference() {
        return mHeaderCoordinator.getOnImageThumbnailRetrievedReference();
    }

    @VisibleForTesting
    void initializeHeaderCoordinatorForTesting(Activity activity, ContextMenuParams params) {
        mHeaderCoordinator = new RevampedContextMenuHeaderCoordinator(activity, params);
    }

    public void clickListItemForTesting(int id) {
        mListView.performItemClick(null, -1, id);
    }
}
