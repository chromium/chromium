// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemProperties.MENU_ID;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemWithIconButtonProperties.BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.contextmenu.RevampedContextMenuItemWithIconButtonProperties.BUTTON_MENU_ID;

import android.app.Activity;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver;
import org.chromium.chrome.browser.performance_hints.PerformanceHintsObserver.PerformanceClass;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.ContextMenuDialog;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.MenuSourceType;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.widget.AnchoredPopupWindow;

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
            ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON})
    public @interface ListItemType {
        int DIVIDER = 0;
        int HEADER = 1;
        int CONTEXT_MENU_ITEM = 2;
        int CONTEXT_MENU_ITEM_WITH_ICON_BUTTON = 3;
    }

    private static final int INVALID_ITEM_ID = -1;

    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private RevampedContextMenuChipController mChipController;
    private RevampedContextMenuHeaderCoordinator mHeaderCoordinator;

    private RevampedContextMenuListView mListView;
    private float mTopContentOffsetPx;
    private ContextMenuDialog mDialog;
    private Runnable mOnMenuClosed;
    private ContextMenuNativeDelegate mNativeDelegate;

    /**
     * Constructor that also sets the content offset.
     *
     * @param topContentOffsetPx content offset from the top.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} to retrieve the thumbnail from
     *         native.
     */
    RevampedContextMenuCoordinator(
            float topContentOffsetPx, ContextMenuNativeDelegate nativeDelegate) {
        mTopContentOffsetPx = topContentOffsetPx;
        mNativeDelegate = nativeDelegate;
    }

    @Override
    public void displayMenu(final WindowAndroid window, WebContents webContents,
            ContextMenuParams params, List<Pair<Integer, ModelList>> items,
            Callback<Integer> onItemClicked, final Runnable onMenuShown,
            final Runnable onMenuClosed) {
        displayMenuWithChip(window, webContents, params, items, onItemClicked, onMenuShown,
                onMenuClosed, /* chipDelegate=*/null);
    }

    @Override
    public void dismiss() {
        dismissDialog();
    }

    // Shows the menu with chip.
    void displayMenuWithChip(final WindowAndroid window, WebContents webContents,
            ContextMenuParams params, List<Pair<Integer, ModelList>> items,
            Callback<Integer> onItemClicked, final Runnable onMenuShown,
            final Runnable onMenuClosed, @Nullable ChipDelegate chipDelegate) {
        mOnMenuClosed = onMenuClosed;
        final boolean isPopup = params.getSourceType() == MenuSourceType.MENU_SOURCE_MOUSE;
        Activity activity = window.getActivity().get();
        final float density = activity.getResources().getDisplayMetrics().density;
        final float touchPointXPx = params.getTriggeringTouchXDp() * density;
        final float touchPointYPx = params.getTriggeringTouchYDp() * density;
        int dialogTopMarginPx = ContextMenuDialog.NO_CUSTOM_MARGIN;
        int dialogBottomMarginPx = ContextMenuDialog.NO_CUSTOM_MARGIN;

        final View layout = LayoutInflater.from(activity).inflate(
                R.layout.context_menu_fullscreen_container, null);

        // Only display a chip if an image was selected and the menu isn't a popup.
        if (params.isImage() && chipDelegate != null && chipDelegate.isChipSupported()
                && !isPopup) {
            View chipAnchorView = layout.findViewById(R.id.context_menu_chip_anchor_point);
            mChipController = new RevampedContextMenuChipController(
                    activity, chipAnchorView, () -> dismiss());
            chipDelegate.getChipRenderParams((chipRenderParams) -> {
                if (chipDelegate.isValidChipRenderParams(chipRenderParams) && mDialog.isShowing()) {
                    mChipController.showChip(chipRenderParams);
                }
            });
            dialogBottomMarginPx = mChipController.getVerticalPxNeededForChip();
            // Allow dialog to get close to the top of the screen.
            dialogTopMarginPx = dialogBottomMarginPx / 2;
        }

        final View menu = isPopup
                ? LayoutInflater.from(activity).inflate(R.layout.context_menu, null)
                : ((ViewStub) layout.findViewById(R.id.context_menu_stub)).inflate();
        mDialog = createContextMenuDialog(activity, layout, menu, isPopup, touchPointXPx,
                touchPointYPx, dialogTopMarginPx, dialogBottomMarginPx);
        mDialog.setOnShowListener(dialogInterface -> onMenuShown.run());
        mDialog.setOnDismissListener(dialogInterface -> mOnMenuClosed.run());

        mWebContents = webContents;
        int performanceClass = params.isAnchor()
                ? PerformanceHintsObserver.getPerformanceClassForURL(
                        webContents, params.getLinkUrl())
                : PerformanceClass.PERFORMANCE_UNKNOWN;
        mHeaderCoordinator = new RevampedContextMenuHeaderCoordinator(activity, performanceClass,
                params, Profile.fromWebContents(mWebContents), mNativeDelegate);

        // The Integer here specifies the {@link ListItemType}.
        ModelList listItems = getItemList(activity, items, onItemClicked);

        ModelListAdapter adapter = new ModelListAdapter(listItems) {
            @Override
            public boolean areAllItemsEnabled() {
                return false;
            }

            @Override
            public boolean isEnabled(int position) {
                return getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM
                        || getItemViewType(position)
                        == ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON;
            }

            @Override
            public long getItemId(int position) {
                if (getItemViewType(position) == ListItemType.CONTEXT_MENU_ITEM
                        || getItemViewType(position)
                                == ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON) {
                    return ((ListItem) getItem(position)).model.get(MENU_ID);
                }
                return INVALID_ITEM_ID;
            }
        };

        mListView = menu.findViewById(R.id.context_menu_list_view);
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
                ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
                new LayoutViewBuilder(R.layout.revamped_context_menu_share_row),
                RevampedContextMenuItemWithIconButtonViewBinder::bind);
        // clang-format on

        mListView.setOnItemClickListener((p, v, pos, id) -> {
            assert id != INVALID_ITEM_ID;

            clickItem((int) id, activity, onItemClicked);
        });

        mWebContentsObserver = new WebContentsObserver(mWebContents) {
            @Override
            public void navigationEntryCommitted(LoadCommittedDetails details) {
                dismissDialog();
            }
        };

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
     * @param layout The inflated context menu layout that will house the context menu.
     * @param view The inflated view that contains the list view.
     * @param isPopup Whether the context menu is being shown in a {@link AnchoredPopupWindow}.
     * @param touchPointXPx The x-coordinate of the touch that triggered the context menu.
     * @param touchPointYPx The y-coordinate of the touch that triggered the context menu.
     * @param topMarginPx An explicit top margin for the dialog, or -1 to use default
     *                    defined in XML.
     * @param bottomMarginPx An explicit bottom margin for the dialog, or -1 to use default
     *                       defined in XML.
     * @return Returns a final dialog that does not have a background can be displayed using
     *         {@link AlertDialog#show()}.
     */
    private ContextMenuDialog createContextMenuDialog(Activity activity, View layout, View view,
            boolean isPopup, float touchPointXPx, float touchPointYPx, int topMarginPx,
            int bottomMarginPx) {
        // TODO(sinansahin): Refactor ContextMenuDialog as well.
        final ContextMenuDialog dialog = new ContextMenuDialog(activity,
                R.style.Theme_Chromium_AlertDialog, touchPointXPx, touchPointYPx,
                mTopContentOffsetPx, topMarginPx, bottomMarginPx, layout, view, isPopup);
        dialog.setContentView(layout);

        return dialog;
    }

    @VisibleForTesting
    ModelList getItemList(Activity activity, List<Pair<Integer, ModelList>> items,
            Callback<Integer> onItemClicked) {
        ModelList itemList = new ModelList();

        // Start with the header
        itemList.add(new ListItem(ListItemType.HEADER, mHeaderCoordinator.getModel()));

        for (Pair<Integer, ModelList> group : items) {
            // Add a divider
            itemList.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
            // Add the items in the group
            itemList.addAll(group.second);
        }

        for (ListItem item : itemList) {
            if (item.type == ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON) {
                item.model.set(BUTTON_CLICK_LISTENER,
                        (v) -> clickItem(item.model.get(BUTTON_MENU_ID), activity, onItemClicked));
            }
        }

        return itemList;
    }

    private void dismissDialog() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.destroy();
        }
        if (mChipController != null) {
            mChipController.dismissChipIfShowing();
        }
        mDialog.dismiss();
    }

    @VisibleForTesting
    Callback<ChipRenderParams> getChipRenderParamsCallbackForTesting(ChipDelegate chipDelegate) {
        return (chipRenderParams) -> {
            if (chipDelegate.isValidChipRenderParams(chipRenderParams) && mDialog.isShowing()) {
                mChipController.showChip(chipRenderParams);
            }
        };
    }

    @VisibleForTesting
    void initializeHeaderCoordinatorForTesting(Activity activity, ContextMenuParams params,
            Profile profile, ContextMenuNativeDelegate nativeDelegate) {
        mHeaderCoordinator = new RevampedContextMenuHeaderCoordinator(
                activity, PerformanceClass.PERFORMANCE_UNKNOWN, params, profile, nativeDelegate);
    }

    @VisibleForTesting
    void simulateShoppyImageClassificationForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        mChipController.setFakeLensQueryResultForTesting(); // IN-TEST
        ChipRenderParams chipRenderParamsForTesting = new ChipRenderParams();
        chipRenderParamsForTesting.titleResourceId =
                R.string.contextmenu_shop_image_with_google_lens;
        chipRenderParamsForTesting.onClickCallback = () -> {};
        mChipController.showChip(chipRenderParamsForTesting);
    }

    @VisibleForTesting
    void simulateTranslateImageClassificationForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        mChipController.setFakeLensQueryResultForTesting(); // IN-TEST
        ChipRenderParams chipRenderParamsForTesting = new ChipRenderParams();
        chipRenderParamsForTesting.titleResourceId =
                R.string.contextmenu_translate_image_with_google_lens;
        chipRenderParamsForTesting.onClickCallback = () -> {};
        mChipController.showChip(chipRenderParamsForTesting);
    }

    @VisibleForTesting
    ChipRenderParams simulateImageClassificationForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        mChipController.setFakeLensQueryResultForTesting(); // IN-TEST
        ChipRenderParams chipRenderParamsForTesting = new ChipRenderParams();
        return chipRenderParamsForTesting;
    }

    // Public only to allow references from RevampedContextMenuUtils.java
    public void clickChipForTesting() {
        mChipController.clickChipForTesting(); // IN-TEST
    }

    // Public only to allow references from RevampedContextMenuUtils.java
    public AnchoredPopupWindow getCurrentPopupWindowForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        return mChipController.getCurrentPopupWindowForTesting(); // IN-TEST
    }

    public void clickListItemForTesting(int id) {
        mListView.performItemClick(null, -1, id);
    }

    @VisibleForTesting
    ListItem getItem(int index) {
        return (ListItem) mListView.getAdapter().getItem(index);
    }

    @VisibleForTesting
    public int getCount() {
        return mListView.getAdapter().getCount();
    }

    @VisibleForTesting
    public ListItem findItem(int id) {
        for (int i = 0; i < getCount(); i++) {
            final ListItem item = getItem(i);
            // If the item is a title/divider, its model does not have MENU_ID as key.
            if (item.model.getAllSetProperties().contains(MENU_ID)
                    && item.model.get(MENU_ID) == id) {
                return item;
            }
        }
        return null;
    }
}
