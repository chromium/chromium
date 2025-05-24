// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_CLICK_LISTENER;
import static org.chromium.chrome.browser.contextmenu.ContextMenuItemWithIconButtonProperties.BUTTON_MENU_ID;
import static org.chromium.ui.listmenu.ListMenuItemProperties.ENABLED;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.app.Activity;
import android.graphics.Rect;
import android.util.Pair;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeUtils;
import org.chromium.components.browser_ui.widget.ContextMenuDialog;
import org.chromium.components.embedder_support.contextmenu.ChipDelegate;
import org.chromium.components.embedder_support.contextmenu.ChipRenderParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuNativeDelegate;
import org.chromium.components.embedder_support.contextmenu.ContextMenuParams;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUi;
import org.chromium.components.embedder_support.contextmenu.ContextMenuUtils;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.listmenu.ListMenuItemViewBinder;
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
 * The main coordinator for the context menu, responsible for creating the context menu in general
 * and the header component.
 */
public class ContextMenuCoordinator implements ContextMenuUi {
    @Retention(RetentionPolicy.SOURCE)
    @IntDef({
        ListItemType.DIVIDER,
        ListItemType.HEADER,
        ListItemType.CONTEXT_MENU_ITEM,
        ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
        ListItemType.CONTEXT_MENU_ITEM_WITH_CHECKBOX,
        ListItemType.CONTEXT_MENU_ITEM_WITH_RADIO_BUTTON,
    })
    public @interface ListItemType {
        int DIVIDER = 0;
        int HEADER = 1;
        int CONTEXT_MENU_ITEM = 2;
        int CONTEXT_MENU_ITEM_WITH_ICON_BUTTON = 3;
        int CONTEXT_MENU_ITEM_WITH_CHECKBOX = 4;
        int CONTEXT_MENU_ITEM_WITH_RADIO_BUTTON = 5;
    }

    private static final int INVALID_ITEM_ID = -1;

    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private ContextMenuChipController mChipController;
    private ContextMenuHeaderCoordinator mHeaderCoordinator;

    private ContextMenuListView mListView;
    private final float mTopContentOffsetPx;
    private ContextMenuDialog mDialog;
    private Runnable mOnMenuClosed;
    private final ContextMenuNativeDelegate mNativeDelegate;
    private boolean mIsInterestTargetWithShiftedMenu;

    /**
     * Constructor that also sets the content offset.
     *
     * @param topContentOffsetPx content offset from the top.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} to retrieve the thumbnail from
     *     native.
     */
    ContextMenuCoordinator(float topContentOffsetPx, ContextMenuNativeDelegate nativeDelegate) {
        mTopContentOffsetPx = topContentOffsetPx;
        mNativeDelegate = nativeDelegate;
    }

    @Override
    public void displayMenu(
            final WindowAndroid window,
            WebContents webContents,
            ContextMenuParams params,
            List<Pair<Integer, ModelList>> items,
            Callback<Integer> onItemClicked,
            final Runnable onMenuShown,
            final Runnable onMenuClosed) {
        displayMenuWithChip(
                window,
                webContents,
                params,
                items,
                onItemClicked,
                onMenuShown,
                onMenuClosed,
                /* chipDelegate= */ null);
    }

    @Override
    public void dismiss() {
        dismissDialog();
    }

    // Shows the menu with chip.
    void displayMenuWithChip(
            final WindowAndroid window,
            WebContents webContents,
            ContextMenuParams params,
            List<Pair<Integer, ModelList>> items,
            Callback<Integer> onItemClicked,
            final Runnable onMenuShown,
            final Runnable onMenuClosed,
            @Nullable ChipDelegate chipDelegate) {
        mOnMenuClosed = onMenuClosed;
        Activity activity = window.getActivity().get();

        final boolean isDragDropEnabled = ContextMenuUtils.isDragDropEnabled(activity);
        // There are two experimental modes for the interesttarget feature:
        //  1. the context menu is "shifted", to leave room for the page content, with the available
        //     space communicated back to the site via env() variables.
        //  2. the context menu is shown "as usual", but an item is added to the top of the context
        //     menu, allowing the user to show interest in the link.
        // If mIsInterestTargetWithShiftedMenu is true, we're in case 1. The
        // InterestTargetNodeID being set to 0 indicate this, and that'll happen
        // if the `HTMLInterestTargetContextMenuItemOnly` feature is disabled.
        mIsInterestTargetWithShiftedMenu =
                params.getOpenedFromInterestTarget() && params.getInterestTargetNodeID() == 0;

        final boolean usePopupWindow =
                isDragDropEnabled
                        || ContextMenuUtils.isMouseOrHighlightPopup(params)
                        || mIsInterestTargetWithShiftedMenu;

        final View layout =
                LayoutInflater.from(activity)
                        .inflate(R.layout.context_menu_fullscreen_container, null);

        // Calculate the Rect used to display the context menu dialog.
        Rect contextMenuRect =
                ContextMenuUtils.getContextMenuAnchorRect(
                        activity,
                        window.getWindow(),
                        webContents,
                        params,
                        mTopContentOffsetPx,
                        usePopupWindow,
                        layout);
        boolean shouldRemoveScrim = ContextMenuUtils.isPopupSupported(activity);

        // If this is an interesttarget element, the top (or left) half of the
        // screen should be left open for the site to locate its hovercard.
        // TODO(masonf): Still left to do:
        //  1. For larger screens, simply provide a rectangular area around the
        //     tapped screen location, and let the context menu position itself
        //     relative to that.
        if (mIsInterestTargetWithShiftedMenu) {
            var displayMetrics = activity.getResources().getDisplayMetrics();
            float displayWidth = (float) displayMetrics.widthPixels;
            float displayHeight = (float) displayMetrics.heightPixels;
            float page_scale_factor =
                    RenderCoordinates.fromWebContents(webContents).getPageScaleFactor();
            float device_scale_factor =
                    webContents.getTopLevelNativeWindow().getDisplay().getDipScale();
            float scale_factor = device_scale_factor * page_scale_factor;
            float safeAreaWidth;
            float safeAreaHeight;
            if (displayWidth < displayHeight) {
                // Portrait - leave the top half of the screen available to the
                // site.
                contextMenuRect = new Rect(0, 0, (int) displayWidth, (int) (displayHeight / 2));
                safeAreaWidth = displayWidth / scale_factor;
                safeAreaHeight = ((displayHeight / 2) - mTopContentOffsetPx) / scale_factor;
            } else {
                // Landscape - leave the left half of the screen available to
                // the site.
                // TODO(masonf) Since the context menu is wider than half the
                // width of the screen, the context menu will be simply shown at
                // the top left. Likely the context menu needs to be made
                // narrower in this case.
                contextMenuRect = new Rect(0, 0, (int) (displayWidth / 2), (int) displayHeight);
                safeAreaWidth = displayWidth / 2 / scale_factor;
                safeAreaHeight = (displayHeight - mTopContentOffsetPx) / scale_factor;
            }
            // Remove the darkened "scrim" behind the context menu.
            shouldRemoveScrim = true;

            // Notify Blink of the new still-open "safe area" not covered by the context menu. It is
            // in DIPs, and is adjusted for page zoom.
            Rect safeAreaRect =
                    new Rect(0, 0, Math.round(safeAreaWidth), Math.round(safeAreaHeight));
            webContents.setContextMenuInsets(safeAreaRect);
        }

        int dialogTopMarginPx = ContextMenuDialog.NO_CUSTOM_MARGIN;
        int dialogBottomMarginPx = ContextMenuDialog.NO_CUSTOM_MARGIN;

        // Only display a chip if an image was selected and the menu isn't a popup.
        if (params.isImage()
                && chipDelegate != null
                && chipDelegate.isChipSupported()
                && !usePopupWindow) {
            View chipAnchorView = layout.findViewById(R.id.context_menu_chip_anchor_point);
            mChipController =
                    new ContextMenuChipController(activity, chipAnchorView, () -> dismiss());
            chipDelegate.getChipRenderParams(
                    (chipRenderParams) -> {
                        if (chipDelegate.isValidChipRenderParams(chipRenderParams)
                                && mDialog.isShowing()) {
                            mChipController.showChip(chipRenderParams);
                        }
                    });
            dialogBottomMarginPx = mChipController.getVerticalPxNeededForChip();
            // Allow dialog to get close to the top of the screen.
            dialogTopMarginPx = dialogBottomMarginPx / 2;
        }

        final View menu =
                usePopupWindow
                        ? LayoutInflater.from(activity).inflate(R.layout.context_menu, null)
                        : ((ViewStub) layout.findViewById(R.id.context_menu_stub)).inflate();
        Integer popupMargin =
                params.getOpenedFromHighlight()
                        ? activity.getResources()
                                .getDimensionPixelSize(R.dimen.context_menu_small_lateral_margin)
                        : null;
        Integer desiredPopupContentWidth = null;
        if (isDragDropEnabled) {
            desiredPopupContentWidth =
                    activity.getResources()
                            .getDimensionPixelSize(R.dimen.context_menu_popup_max_width);
        } else if (params.getOpenedFromHighlight()) {
            desiredPopupContentWidth =
                    activity.getResources().getDimensionPixelSize(R.dimen.context_menu_small_width);
        }

        // When drag and drop is enabled, context menu will be dismissed by web content when drag
        // moves beyond certain threshold. ContentView will need to receive drag events dispatched
        // from ContextMenuDialog in order to calculate the movement.
        View dragDispatchingTargetView =
                isDragDropEnabled ? webContents.getViewAndroidDelegate().getContainerView() : null;

        mDialog =
                createContextMenuDialog(
                        activity,
                        layout,
                        menu,
                        usePopupWindow,
                        shouldRemoveScrim,
                        dialogTopMarginPx,
                        dialogBottomMarginPx,
                        popupMargin,
                        desiredPopupContentWidth,
                        dragDispatchingTargetView,
                        contextMenuRect);
        mDialog.setOnShowListener(dialogInterface -> onMenuShown.run());
        mDialog.setOnDismissListener(
                (dialogInterface) -> {
                    mOnMenuClosed.run();
                    if (mIsInterestTargetWithShiftedMenu) {
                        // Remove context menu insets when the menu closes.
                        webContents.setContextMenuInsets(new Rect());
                    }
                });

        mWebContents = webContents;
        mHeaderCoordinator =
                new ContextMenuHeaderCoordinator(
                        activity, params, Profile.fromWebContents(mWebContents), mNativeDelegate);

        // The Integer here specifies the {@link ListItemType}.
        ModelList listItems =
                getItemList(
                        activity,
                        items,
                        onItemClicked,
                        // Resource header is shown for link-type resources so that users can
                        // preview the page before initiating any actions. This is not needed for
                        // actions performed on the current page.
                        /* hasHeader= */ !params.getOpenedFromHighlight() && !params.isPage());

        ModelListAdapter adapter =
                new ModelListAdapter(listItems) {
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
                            return ((ListItem) getItem(position)).model.get(MENU_ITEM_ID);
                        }
                        return INVALID_ITEM_ID;
                    }
                };

        mListView = menu.findViewById(R.id.context_menu_list_view);
        mListView.setAdapter(adapter);

        adapter.registerType(
                ListItemType.HEADER,
                new LayoutViewBuilder(R.layout.context_menu_header),
                ContextMenuHeaderViewBinder::bind);
        adapter.registerType(
                ListItemType.DIVIDER,
                new LayoutViewBuilder(R.layout.list_section_divider),
                (m, v, p) -> {});
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM,
                new LayoutViewBuilder(R.layout.context_menu_row),
                ListMenuItemViewBinder::binder);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
                new LayoutViewBuilder(R.layout.context_menu_share_row),
                ContextMenuItemWithIconButtonViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM_WITH_CHECKBOX,
                new LayoutViewBuilder<>(R.layout.checkbox_layout),
                ContextMenuItemWithCheckboxViewBinder::bind);
        adapter.registerType(
                ListItemType.CONTEXT_MENU_ITEM_WITH_RADIO_BUTTON,
                new LayoutViewBuilder<>(R.layout.radio_button_layout_element),
                ContextMenuItemWithRadioButtonViewBinder::bind);

        mListView.setOnItemClickListener(
                (p, v, pos, id) -> {
                    assert id != INVALID_ITEM_ID;
                    ListItem item = findItem((int) id);
                    clickItem(
                            (int) id,
                            activity,
                            onItemClicked,
                            item == null ? true : item.model.get(ENABLED));
                });
        // Set the fading edge for context menu. This is guarded by drag and drop feature flag, but
        // ideally this could be enabled for all forms of context menu.
        if (isDragDropEnabled) {
            mListView.setVerticalFadingEdgeEnabled(true);
            mListView.setFadingEdgeLength(
                    activity.getResources()
                            .getDimensionPixelSize(R.dimen.context_menu_fading_edge_size));
        }
        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    @Override
                    public void navigationEntryCommitted(LoadCommittedDetails details) {
                        dismissDialog();
                    }

                    @Override
                    public void onVisibilityChanged(@Visibility int visibility) {
                        if (visibility != Visibility.VISIBLE) dismissDialog();
                    }
                };

        mDialog.show();
    }

    /**
     * Execute an action for the selected item and close the menu.
     *
     * @param id The id of the item.
     * @param activity The current activity.
     * @param onItemClicked The callback to take action with the given id.
     * @param enabled Whether the item is enabled.
     */
    private void clickItem(
            int id, Activity activity, Callback<Integer> onItemClicked, boolean enabled) {
        // Do not start any action when the activity is on the way to destruction.
        // See https://crbug.com/990987
        if (activity.isFinishing() || activity.isDestroyed()) return;

        onItemClicked.onResult(id);

        // Dismiss the dialog if the item is enabled.
        if (enabled) {
            dismissDialog();
        }
    }

    /**
     * Returns the fully complete dialog based off the params, the itemGroups, and related Chrome
     * feature flags.
     *
     * @param activity Used to inflate the dialog.
     * @param layout The inflated context menu layout that will house the context menu.
     * @param menuView The inflated view that contains the list view.
     * @param isPopup Whether the context menu is being shown in a {@link AnchoredPopupWindow}.
     * @param topMarginPx An explicit top margin for the dialog, or -1 to use default defined in
     *     XML.
     * @param bottomMarginPx An explicit bottom margin for the dialog, or -1 to use default defined
     *     in XML.
     * @param popupMargin The margin for the popup window.
     * @param desiredPopupContentWidth The desired width for the content of the context menu.
     * @param dragDispatchingTargetView The view presented behind the context menu. If provided,
     *     drag event happened outside of ContextMenu will be dispatched into this View.
     * @param rect Rect location where context menu is triggered. If this menu is a popup, the
     *     coordinates are expected to be screen coordinates.
     * @return Returns a final dialog that does not have a background can be displayed using {@link
     *     AlertDialog#show()}.
     */
    @VisibleForTesting
    static ContextMenuDialog createContextMenuDialog(
            Activity activity,
            View layout,
            View menuView,
            boolean isPopup,
            boolean shouldRemoveScrim,
            int topMarginPx,
            int bottomMarginPx,
            @Nullable Integer popupMargin,
            @Nullable Integer desiredPopupContentWidth,
            @Nullable View dragDispatchingTargetView,
            Rect rect) {
        // TODO(sinansahin): Refactor ContextMenuDialog as well.
        final ContextMenuDialog dialog =
                new ContextMenuDialog(
                        activity,
                        R.style.ThemeOverlay_BrowserUI_AlertDialog,
                        topMarginPx,
                        bottomMarginPx,
                        layout,
                        menuView,
                        isPopup,
                        shouldRemoveScrim,
                        ChromeFeatureList.isEnabled(
                                ChromeFeatureList.CONTEXT_MENU_SYS_UI_MATCHES_ACTIVITY),
                        popupMargin,
                        desiredPopupContentWidth,
                        dragDispatchingTargetView,
                        rect,
                        EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled());
        dialog.setContentView(layout);

        return dialog;
    }

    @VisibleForTesting
    ModelList getItemList(
            Activity activity,
            List<Pair<Integer, ModelList>> items,
            Callback<Integer> onItemClicked,
            boolean hasHeader) {
        ModelList itemList = new ModelList();

        // Start with the header
        if (hasHeader) {
            itemList.add(new ListItem(ListItemType.HEADER, mHeaderCoordinator.getModel()));
        }

        for (Pair<Integer, ModelList> group : items) {
            // Add a divider
            if (itemList.size() > 0) {
                itemList.add(new ListItem(ListItemType.DIVIDER, new PropertyModel()));
            }

            // Add the items in the group
            itemList.addAll(group.second);
        }

        for (ListItem item : itemList) {
            if (item.type == ListItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON) {
                item.model.set(
                        BUTTON_CLICK_LISTENER,
                        (v) ->
                                clickItem(
                                        item.model.get(BUTTON_MENU_ID),
                                        activity,
                                        onItemClicked,
                                        item.model.get(ENABLED)));
            }
        }

        return itemList;
    }

    private void dismissDialog() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
        }
        if (mChipController != null) {
            mChipController.dismissChipIfShowing();
        }
        mDialog.dismiss();
        if (mIsInterestTargetWithShiftedMenu) {
            // Remove context menu insets if the menu is dismissed.
            mWebContents.setContextMenuInsets(new Rect());
        }
    }

    Callback<ChipRenderParams> getChipRenderParamsCallbackForTesting(ChipDelegate chipDelegate) {
        return (chipRenderParams) -> {
            if (chipDelegate.isValidChipRenderParams(chipRenderParams) && mDialog.isShowing()) {
                mChipController.showChip(chipRenderParams);
            }
        };
    }

    void initializeHeaderCoordinatorForTesting(
            Activity activity,
            ContextMenuParams params,
            Profile profile,
            ContextMenuNativeDelegate nativeDelegate) {
        mHeaderCoordinator =
                new ContextMenuHeaderCoordinator(activity, params, profile, nativeDelegate);
    }

    void simulateShoppyImageClassificationForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        ChipRenderParams chipRenderParamsForTesting = new ChipRenderParams();
        chipRenderParamsForTesting.titleResourceId =
                R.string.contextmenu_shop_image_with_google_lens;
        chipRenderParamsForTesting.onClickCallback = CallbackUtils.emptyRunnable();
        mChipController.showChip(chipRenderParamsForTesting);
    }

    void simulateTranslateImageClassificationForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        ChipRenderParams chipRenderParamsForTesting = new ChipRenderParams();
        chipRenderParamsForTesting.titleResourceId =
                R.string.contextmenu_translate_image_with_google_lens;
        chipRenderParamsForTesting.onClickCallback = CallbackUtils.emptyRunnable();
        mChipController.showChip(chipRenderParamsForTesting);
    }

    ChipRenderParams simulateImageClassificationForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        ChipRenderParams chipRenderParamsForTesting = new ChipRenderParams();
        return chipRenderParamsForTesting;
    }

    // Public only to allow references from ContextMenuUtils.java
    public void clickChipForTesting() {
        mChipController.clickChipForTesting(); // IN-TEST
    }

    // Public only to allow references from ContextMenuUtils.java
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
            if (item.model.getAllSetProperties().contains(MENU_ITEM_ID)
                    && item.model.get(MENU_ITEM_ID) == id) {
                return item;
            }
        }
        return null;
    }

    public ContextMenuDialog getDialogForTest() {
        return mDialog;
    }

    public ContextMenuListView getListViewForTest() {
        return mListView;
    }

    public WebContentsObserver getWebContentsObserverForTesting() {
        return mWebContentsObserver;
    }
}
