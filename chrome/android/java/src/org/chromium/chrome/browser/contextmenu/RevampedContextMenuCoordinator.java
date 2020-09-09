// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import android.app.Activity;
import android.graphics.Bitmap;
import android.graphics.drawable.Drawable;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.components.browser_ui.share.ShareParams;
import org.chromium.components.browser_ui.widget.ContextMenuDialog;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LayoutViewBuilder;
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

    private WebContents mWebContents;
    private RevampedContextMenuChipController mChipController;
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
    public void displayMenu(final WindowAndroid window, WebContents webContents,
            ContextMenuParams params, List<Pair<Integer, List<ContextMenuItem>>> items,
            Callback<Integer> onItemClicked, final Runnable onMenuShown,
            final Callback<Boolean> onMenuClosed) {
        displayMenuWithLensChip(window, webContents, params, items, onItemClicked, onMenuShown,
                onMenuClosed, /* lensAsyncManager=*/null);
    }

    // Shows the Context Menu in Chrome with the lens chip (if supported).
    void displayMenuWithLensChip(final WindowAndroid window, WebContents webContents,
            ContextMenuParams params, List<Pair<Integer, List<ContextMenuItem>>> items,
            Callback<Integer> onItemClicked, final Runnable onMenuShown,
            final Callback<Boolean> onMenuClosed, @Nullable LensAsyncManager lensAsyncManager) {
        mOnMenuClosed = onMenuClosed;
        final boolean lensShoppingFeatureEnabled = lensAsyncManager != null;
        Activity activity = window.getActivity().get();
        final float density = activity.getResources().getDisplayMetrics().density;
        final float touchPointXPx = params.getTriggeringTouchXDp() * density;
        final float touchPointYPx = params.getTriggeringTouchYDp() * density;
        int dialogTopMarginPx = ContextMenuDialog.NO_CUSTOM_MARGIN;
        int dialogBottomMarginPx = ContextMenuDialog.NO_CUSTOM_MARGIN;
        final View view =
                LayoutInflater.from(activity).inflate(R.layout.revamped_context_menu, null);

        // Only display a chip if an image was selected.
        if (params.isImage() && lensShoppingFeatureEnabled) {
            View chipAnchorView = view.findViewById(R.id.context_menu_chip_anchor_point);
            mChipController = new RevampedContextMenuChipController(
                    activity, chipAnchorView, lensAsyncManager, () -> {
                        // A chip selection should trigger the lens shopping action.
                        clickItem((int) R.id.contextmenu_shop_image_with_google_lens, activity,
                                onItemClicked);
                    });
            dialogBottomMarginPx = mChipController.getVerticalPxNeededForChip();
            // Allow dialog to get close to the top of the screen.
            dialogTopMarginPx = dialogBottomMarginPx / 2;
        }

        mDialog = createContextMenuDialog(activity, view, touchPointXPx, touchPointYPx,
                dialogTopMarginPx, dialogBottomMarginPx);
        mDialog.setOnShowListener(dialogInterface -> onMenuShown.run());
        mDialog.setOnDismissListener(dialogInterface -> mOnMenuClosed.onResult(false));

        mWebContents = webContents;
        int performanceClass = params.isAnchor()
                ? PerformanceHintsObserver.getPerformanceClassForURL(
                        webContents, params.getLinkUrl())
                : PerformanceClass.PERFORMANCE_UNKNOWN;
        mHeaderCoordinator = new RevampedContextMenuHeaderCoordinator(
                activity, performanceClass, params, Profile.fromWebContents(mWebContents));

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
                new LayoutViewBuilder(R.layout.revamped_context_menu_header),
                RevampedContextMenuHeaderViewBinder::bind);
        adapter.registerType(
                ListItemType.DIVIDER,
                new LayoutViewBuilder(R.layout.app_menu_divider),
                (m, v, p) -> {});
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM,
                new LayoutViewBuilder(R.layout.revamped_context_menu_row),
                RevampedContextMenuItemViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_SHARE_ITEM,
                new LayoutViewBuilder(R.layout.revamped_context_menu_share_row),
                RevampedContextMenuShareItemViewBinder::bind);
        // clang-format on

        mListView.setOnItemClickListener((p, v, pos, id) -> {
            assert id != INVALID_ITEM_ID;

            clickItem((int) id, activity, onItemClicked);
        });

        mDialog.show();
    }

    /**
     * Execute an action for the selected item and close the menu.
     * @param id The id of the item.
     * @param activity The current activity.
     * @param onItemClicked The callback to take action with the given id.
     */
    private void clickItem(int id, Activity activity, Callback<Integer> onItemClicked) {
        // Do not start any action when the activity is on the way to destruction.
        // See https://crbug.com/990987
        if (activity.isFinishing() || activity.isDestroyed()) return;
        onItemClicked.onResult((int) id);
        dismissDialog();
    }

    /**
     * Returns the fully complete dialog based off the params and the itemGroups.
     *
     * @param activity Used to inflate the dialog.
     * @param view The inflated view, including the scrim, that contains the list view.
     * @param touchPointYPx The x-coordinate of the touch that triggered the context menu.
     * @param touchPointXPx The y-coordinate of the touch that triggered the context menu.
     * @param topMarginPx An explicit top margin for the dialog, or -1 to use default
     *                    defined in XML.
     * @param bottomMarginPx An explicit bottom margin for the dialog, or -1 to use default
     *                       defined in XML.
     * @return Returns a final dialog that does not have a background can be displayed using
     *         {@link AlertDialog#show()}.
     */
    private ContextMenuDialog createContextMenuDialog(Activity activity, View view,
            float touchPointXPx, float touchPointYPx, int topMarginPx, int bottomMarginPx) {
        View frame = view.findViewById(R.id.context_menu_frame);
        // TODO(sinansahin): Refactor ContextMenuDialog as well.
        final ContextMenuDialog dialog =
                new ContextMenuDialog(activity, R.style.Theme_Chromium_AlertDialog, touchPointXPx,
                        touchPointYPx, mTopContentOffsetPx, topMarginPx, bottomMarginPx, frame);
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
            ChromeContextMenuPopulator.ContextMenuUma.record(mWebContents, params,
                    item.isShareLink()
                            ? ChromeContextMenuPopulator.ContextMenuUma.Action.DIRECT_SHARE_LINK
                            : ChromeContextMenuPopulator.ContextMenuUma.Action.DIRECT_SHARE_IMAGE);
            mDialog.setOnDismissListener(dialogInterface -> mOnMenuClosed.onResult(true));
            dismissDialog();
            if (item.isShareLink()) {
                final ShareParams shareParams =
                        new ShareParams.Builder(window, params.getUrl(), params.getUrl()).build();
                ShareHelper.shareWithLastUsedComponent(shareParams);
            } else {
                mOnShareImageDirectly.run();
            }
        };
    }

    private void dismissDialog() {
        if (mChipController != null) {
            mChipController.dismissLensChipIfShowing();
        }
        mDialog.dismiss();
    }

    Callback<Bitmap> getOnImageThumbnailRetrievedReference() {
        return mHeaderCoordinator.getOnImageThumbnailRetrievedReference();
    }

    @VisibleForTesting
    void initializeHeaderCoordinatorForTesting(
            Activity activity, ContextMenuParams params, Profile profile) {
        mHeaderCoordinator = new RevampedContextMenuHeaderCoordinator(
                activity, PerformanceClass.PERFORMANCE_UNKNOWN, params, profile);
    }

    public void clickListItemForTesting(int id) {
        mListView.performItemClick(null, -1, id);
    }
}
