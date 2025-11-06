// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.contextmenu;

import static org.chromium.build.NullUtil.assertNonNull;
import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.app.Activity;
import android.graphics.Rect;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewStub;
import android.view.Window;
import android.widget.FrameLayout;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.app.AlertDialog;

import org.chromium.base.Callback;
import org.chromium.base.CallbackUtils;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
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
import org.chromium.content_public.browser.Visibility;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.edge_to_edge.EdgeToEdgeStateProvider;
import org.chromium.ui.hierarchicalmenu.FlyoutController;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController.AccessibilityListObserver;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.ModelListAdapter;
import org.chromium.ui.widget.AnchoredPopupWindow;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.ArrayList;
import java.util.List;
import java.util.Set;

/**
 * The main coordinator for the context menu, responsible for creating the context menu in general
 * and the header component.
 */
@NullMarked
public class ContextMenuCoordinator implements ContextMenuUi, FlyoutHandler<ContextMenuDialog> {

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ContextMenuItemType.HEADER, ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON})
    public @interface ContextMenuItemType {
        // These should come after the ListItemTypes in
        // //ui/android/java/src/org/chromium/ui/listmenu/ListItemType.java
        int HEADER = 6;
        int CONTEXT_MENU_ITEM_WITH_ICON_BUTTON = 7;
    }

    private final Activity mActivity;
    private WindowAndroid mWindowAndroid;
    private WebContents mWebContents;
    private WebContentsObserver mWebContentsObserver;
    private @Nullable ContextMenuChipController mChipController;
    private ContextMenuHeaderCoordinator mHeaderCoordinator;
    private ContextMenuParams mParams;

    private final List<ContextMenuListView> mListViews;
    private final float mTopContentOffsetPx;

    private final HierarchicalMenuController mHierarchicalMenuController;

    private Runnable mOnMenuClosed;
    private final ContextMenuNativeDelegate mNativeDelegate;
    private final boolean mIsCustomItemPresent;
    private boolean mUsePopupWindow;

    /**
     * Constructor that also sets the content offset.
     *
     * @param activity The {@link Activity} for the application.
     * @param topContentOffsetPx content offset from the top.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} to retrieve the thumbnail from
     *     native.
     */
    ContextMenuCoordinator(
            Activity activity, float topContentOffsetPx, ContextMenuNativeDelegate nativeDelegate) {
        this(activity, topContentOffsetPx, nativeDelegate, /* isCustomItemPresent= */ false);
    }

    /**
     * @param activity The {@link Activity} for the application.
     * @param topContentOffsetPx content offset from the top.
     * @param nativeDelegate The {@link ContextMenuNativeDelegate} to retrieve the thumbnail from
     *     native.
     * @param isCustomItemPresent Whether a custom item is present in the context menu.
     */
    ContextMenuCoordinator(
            Activity activity,
            float topContentOffsetPx,
            ContextMenuNativeDelegate nativeDelegate,
            boolean isCustomItemPresent) {
        mActivity = activity;
        mTopContentOffsetPx = topContentOffsetPx;
        mNativeDelegate = nativeDelegate;
        mIsCustomItemPresent = isCustomItemPresent;
        mListViews = new ArrayList<>();
        mHierarchicalMenuController = ListMenuUtils.createHierarchicalMenuController(mActivity);
    }

    @Override
    public void displayMenu(
            final WindowAndroid window,
            WebContents webContents,
            ContextMenuParams params,
            List<ModelList> items,
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
        dismissDialogs();
    }

    // Calculate true top content offset to be used to compute the AnchorRect used by
    // AnchoredPopupWindow, with origin below the system decoration which may or may not be merged
    // with the tabstrip.
    private static float topContentOffset(float offset, WindowAndroid windowAndroid) {
        // If edge-to-edge mode is disabled, the input offset i.e. height of tabstrip plus toolbar
        // is correct.
        if (!EdgeToEdgeStateProvider.isEdgeToEdgeEnabledForWindow(windowAndroid)) return offset;

        // Otherwise, the system decoration is tabstrip, so the input offset should only be height
        // of toolbar.
        // Compute the height of system decoration to get height of tabstrip, and subtract it from
        // the input offset.
        Window window = windowAndroid.getWindow();
        if (window == null) return offset;
        View view = window.getDecorView();
        // The rect of the window without system decoration, see
        // https://developer.android.com/reference/android/view/View#getWindowVisibleDisplayFrame(android.graphics.Rect)
        Rect windowVisibleRect = new Rect();
        view.getWindowVisibleDisplayFrame(windowVisibleRect);
        // The coordinates of the window root (with system decoration), see
        // https://developer.android.com/reference/android/view/View#getLocationOnScreen(int[])
        int[] windowRootCoordinates = new int[2];
        view.getLocationOnScreen(windowRootCoordinates);
        // Difference of the two top-left y-coordinates is the height of the system decoration.
        float systemDecorHeight = windowVisibleRect.top - windowRootCoordinates[1];

        return offset - systemDecorHeight;
    }

    /**
     * Displays the context menu, potentially with a chip at the bottom.
     *
     * <p>This method handles the setup and display of the context menu, including: - Determining
     * whether to use a popup window or a full-screen dialog. - Adjusting the layout for features
     * like "interesttarget", which may reserve screen space. - Inflating and populating the menu
     * with items. - Optionally displaying a chip (e.g., for Google Lens) if a {@link ChipDelegate}
     * is provided and conditions are met. - Setting up listeners for menu events (shown, closed,
     * item clicks). - Observing WebContents for events that should dismiss the menu (navigation,
     * visibility change).
     *
     * @param window The {@link WindowAndroid} for the current activity.
     * @param webContents The {@link WebContents} where the context menu was triggered.
     * @param params The {@link ContextMenuParams} containing details about the context menu
     *     trigger.
     * @param items A list of {@link ModelList} representing the menu items, grouped into sections.
     *     Each {@link ModelList} in {@param items} will be separated from the other {@link
     *     ModelList}s by a horizontal divider.
     * @param onItemClicked A {@link Callback} invoked when a menu item is clicked, passing the
     *     item's ID.
     * @param onMenuShown A {@link Runnable} executed when the menu becomes visible.
     * @param onMenuClosed A {@link Runnable} executed when the menu is dismissed.
     * @param chipDelegate An optional {@link ChipDelegate} to manage the display and interaction of
     *     a chip. If null, no chip will be shown.
     */
    @Initializer
    void displayMenuWithChip(
            final WindowAndroid window,
            WebContents webContents,
            ContextMenuParams params,
            List<ModelList> items,
            Callback<Integer> onItemClicked,
            final Runnable onMenuShown,
            final Runnable onMenuClosed,
            @Nullable ChipDelegate chipDelegate) {
        mParams = params;
        mWindowAndroid = window;
        mOnMenuClosed = onMenuClosed;

        final boolean isDragDropEnabled = ContextMenuUtils.isDragDropEnabled(mActivity);
        mUsePopupWindow = isDragDropEnabled || ContextMenuUtils.isMouseOrHighlightPopup(params);

        final View layout =
                LayoutInflater.from(mActivity)
                        .inflate(R.layout.context_menu_fullscreen_container, null);

        // Calculate the Rect used to display the context menu dialog.
        Rect contextMenuRect =
                ContextMenuUtils.getContextMenuAnchorRect(
                        mActivity,
                        assertNonNull(window.getWindow()),
                        webContents,
                        params,
                        topContentOffset(mTopContentOffsetPx, window),
                        mUsePopupWindow,
                        layout);
        boolean shouldRemoveScrim = ContextMenuUtils.isPopupSupported(mActivity);

        int dialogTopMarginPx = ContextMenuDialog.NO_CUSTOM_MARGIN;
        int dialogBottomMarginPx = ContextMenuDialog.NO_CUSTOM_MARGIN;

        // Only display a chip if an image was selected and the menu isn't a popup.
        if (params.isImage()
                && chipDelegate != null
                && chipDelegate.isChipSupported()
                && !mUsePopupWindow) {
            View chipAnchorView = layout.findViewById(R.id.context_menu_chip_anchor_point);
            mChipController =
                    new ContextMenuChipController(mActivity, chipAnchorView, () -> dismiss());
            chipDelegate.getChipRenderParams(
                    (chipRenderParams) -> {
                        FlyoutController<ContextMenuDialog> controller =
                                mHierarchicalMenuController.getFlyoutController();
                        assert controller != null;

                        if (chipDelegate.isValidChipRenderParams(chipRenderParams)
                                && controller.getMainPopup().isShowing()) {
                            assert chipRenderParams != null;
                            assumeNonNull(mChipController).showChip(chipRenderParams);
                        }
                    });
            dialogBottomMarginPx = mChipController.getVerticalPxNeededForChip();
            // Allow dialog to get close to the top of the screen.
            dialogTopMarginPx = dialogBottomMarginPx / 2;
        }

        final View menu =
                mUsePopupWindow
                        ? LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null)
                        : ((ViewStub) layout.findViewById(R.id.context_menu_stub)).inflate();
        Integer popupMargin =
                params.getOpenedFromHighlight()
                        ? mActivity
                                .getResources()
                                .getDimensionPixelSize(R.dimen.context_menu_small_lateral_margin)
                        : null;
        Integer desiredPopupContentWidth = null;
        if (isDragDropEnabled) {
            desiredPopupContentWidth =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.context_menu_popup_max_width);
        } else if (params.getOpenedFromHighlight()) {
            desiredPopupContentWidth =
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.context_menu_small_width);
        }

        // When drag and drop is enabled, context menu will be dismissed by web content when drag
        // moves beyond certain threshold. ContentView will need to receive drag events dispatched
        // from ContextMenuDialog in order to calculate the movement.
        View dragDispatchingTargetView =
                isDragDropEnabled
                        ? assumeNonNull(webContents.getViewAndroidDelegate()).getContainerView()
                        : null;

        ContextMenuDialog dialog =
                createContextMenuDialog(
                        mActivity,
                        layout,
                        menu,
                        mUsePopupWindow,
                        /* isFlyout= */ false,
                        shouldRemoveScrim,
                        dialogTopMarginPx,
                        dialogBottomMarginPx,
                        popupMargin,
                        desiredPopupContentWidth,
                        dragDispatchingTargetView,
                        contextMenuRect,
                        /* onDismissCallback= */ null);
        dialog.setOnShowListener(dialogInterface -> onMenuShown.run());
        dialog.setOnDismissListener(
                (dialogInterface) -> {
                    mOnMenuClosed.run();
                    dismissDialogs();
                });

        mWebContents = webContents;
        mHeaderCoordinator =
                new ContextMenuHeaderCoordinator(
                        mActivity,
                        params,
                        Profile.fromWebContents(mWebContents),
                        mNativeDelegate,
                        mIsCustomItemPresent);
        ContextMenuMediator mediator =
                new ContextMenuMediator(
                        mActivity, mHeaderCoordinator, onItemClicked, this::dismiss);

        // The Integer here specifies the {@link ListItemType}.
        ModelList listItems =
                mediator.updateAndGetModelList(
                        items,
                        // Resource header is shown for link-type resources so that users can
                        // preview the page before initiating any actions. This is not needed for
                        // actions performed on the current page.
                        /* hasHeader= */ !params.getOpenedFromHighlight() && !params.isPage(),
                        mHierarchicalMenuController);

        ModelListAdapter adapter = createAdapter(listItems);

        ContextMenuListView listView = menu.findViewById(R.id.context_menu_list_view);
        listView.setAdapter(adapter);

        listView.setItemsCanFocus(true);
        // Set the fading edge for context menu. This is guarded by drag and drop feature flag, but
        // ideally this could be enabled for all forms of context menu.
        if (isDragDropEnabled) {
            listView.setVerticalFadingEdgeEnabled(true);
            listView.setFadingEdgeLength(
                    mActivity
                            .getResources()
                            .getDimensionPixelSize(R.dimen.context_menu_fading_edge_size));
        }
        mListViews.add(listView);

        listItems.addObserver(
                mHierarchicalMenuController
                .new AccessibilityListObserver(
                        listView,
                        /* headerView= */ null,
                        listView,
                        /* headerModelList= */ null,
                        listItems));

        mWebContentsObserver =
                new WebContentsObserver(mWebContents) {
                    @Override
                    public void navigationEntryCommitted(LoadCommittedDetails details) {
                        dismissDialogs();
                    }

                    @Override
                    public void onVisibilityChanged(@Visibility int visibility) {
                        if (visibility != Visibility.VISIBLE) dismissDialogs();
                    }
                };

        dialog.show();

        mHierarchicalMenuController.setupFlyoutController(
                /* flyoutHandler= */ this,
                dialog,
                /* drillDownOverrideValue= */ mUsePopupWindow ? null : true);
    }

    @Override
    public Rect getPopupRect(ContextMenuDialog popupWindow) {
        return popupWindow.getDialogRect();
    }

    @Override
    public void dismissPopup(ContextMenuDialog popupWindow) {
        popupWindow.dismiss();
    }

    @Override
    public ContextMenuDialog createAndShowFlyoutPopup(
            ListItem item, View view, Runnable dismissRunnable) {
        assert view != null;
        assert mUsePopupWindow;

        final View menu = LayoutInflater.from(mActivity).inflate(R.layout.context_menu, null);
        ModelList listItems = ListMenuUtils.getModelListSubtree(item);
        ModelListAdapter adapter = createAdapter(listItems);

        ContextMenuListView listView = menu.findViewById(R.id.context_menu_list_view);
        listView.setAdapter(adapter);
        listView.setItemsCanFocus(true);
        listView.setIsFlyout(true);
        mListViews.add(listView);

        ContextMenuDialog dialog =
                createContextMenuDialog(
                        mActivity,
                        new FrameLayout(mActivity),
                        menu,
                        mUsePopupWindow,
                        /* isFlyout= */ true,
                        /* shouldRemoveScrim= */ true,
                        ContextMenuDialog.NO_CUSTOM_MARGIN,
                        ContextMenuDialog.NO_CUSTOM_MARGIN,
                        /* popupMargin= */ null,
                        /* desiredPopupContentWidth= */ null,
                        /* dragDispatchingTargetView= */ null,
                        calculateFlyoutAnchorRect(mActivity, mWindowAndroid, view),
                        () -> {
                            dismissRunnable.run();
                        });

        dialog.show();
        return dialog;
    }

    @Override
    public void setWindowFocus(ContextMenuDialog popup, boolean hasFocus) {
        popup.setWindowFocusForFlyoutMenus(hasFocus);
    }

    @Override
    public void afterFlyoutPopupsRemoved(int removeFromIndex) {
        mListViews.subList(removeFromIndex, mListViews.size()).clear();
    }

    private static Rect calculateFlyoutAnchorRect(
            Activity activity, WindowAndroid windowAndroid, View itemView) {
        Rect anchorRect =
                FlyoutController.calculateFlyoutAnchorRect(
                        itemView, activity.getWindow().getDecorView());
        anchorRect.offset(0, (int) topContentOffset(0, windowAndroid));

        return anchorRect;
    }

    /**
     * Returns the fully complete dialog based off the params, the itemGroups, and related Chrome
     * feature flags.
     *
     * @param activity Used to inflate the dialog.
     * @param layout The inflated context menu layout that will house the context menu.
     * @param menuView The inflated view that contains the list view.
     * @param isPopup Whether the context menu is being shown in a {@link AnchoredPopupWindow}.
     * @param isPopup Whether the window is a flyout popup.
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
            boolean isFlyout,
            boolean shouldRemoveScrim,
            int topMarginPx,
            int bottomMarginPx,
            @Nullable Integer popupMargin,
            @Nullable Integer desiredPopupContentWidth,
            @Nullable View dragDispatchingTargetView,
            Rect rect,
            @Nullable Runnable onDismissCallback) {
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
                        isFlyout,
                        shouldRemoveScrim,
                        popupMargin,
                        desiredPopupContentWidth,
                        dragDispatchingTargetView,
                        rect,
                        EdgeToEdgeUtils.isEdgeToEdgeEverywhereEnabled(),
                        onDismissCallback);
        dialog.setContentView(layout);

        return dialog;
    }

    private void dismissDialogs() {
        if (mWebContentsObserver != null) {
            mWebContentsObserver.observe(null);
        }
        if (mChipController != null) {
            mChipController.dismissChipIfShowing();
        }

        if (mHierarchicalMenuController.getFlyoutController() != null) {
            mHierarchicalMenuController.destroyFlyoutController();
        }
    }

    Callback<ChipRenderParams> getChipRenderParamsCallbackForTesting(ChipDelegate chipDelegate) {
        return (chipRenderParams) -> {
            FlyoutController<ContextMenuDialog> controller =
                    mHierarchicalMenuController.getFlyoutController();
            assert controller != null;

            if (chipDelegate.isValidChipRenderParams(chipRenderParams)
                    && controller.getMainPopup().isShowing()) {
                assumeNonNull(mChipController).showChip(chipRenderParams);
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
        assumeNonNull(mChipController).showChip(chipRenderParamsForTesting);
    }

    void simulateTranslateImageClassificationForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        ChipRenderParams chipRenderParamsForTesting = new ChipRenderParams();
        chipRenderParamsForTesting.titleResourceId =
                R.string.contextmenu_translate_image_with_google_lens;
        chipRenderParamsForTesting.onClickCallback = CallbackUtils.emptyRunnable();
        assumeNonNull(mChipController).showChip(chipRenderParamsForTesting);
    }

    ChipRenderParams simulateImageClassificationForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        ChipRenderParams chipRenderParamsForTesting = new ChipRenderParams();
        return chipRenderParamsForTesting;
    }

    // Public only to allow references from ContextMenuUtils.java
    public void clickChipForTesting() {
        assumeNonNull(mChipController).clickChipForTesting(); // IN-TEST
    }

    // Public only to allow references from ContextMenuUtils.java
    public @Nullable AnchoredPopupWindow getCurrentPopupWindowForTesting() {
        // Don't need to initialize controller because that should be triggered by
        // forcing feature flags.
        return assumeNonNull(mChipController).getCurrentPopupWindowForTesting(); // IN-TEST
    }

    public void clickListItemForTesting(int id) {
        ListItem item = findItem(id);
        assert item != null;
        item.model.get(CLICK_LISTENER).onClick(null);
    }

    @VisibleForTesting
    ListItem getItem(int index) {
        return getItem(0, index);
    }

    @VisibleForTesting
    ListItem getItem(int popupLevel, int index) {
        assert popupLevel < mListViews.size();
        return (ListItem) mListViews.get(popupLevel).getAdapter().getItem(index);
    }

    @VisibleForTesting
    public int getCount() {
        return getCount(0);
    }

    @VisibleForTesting
    public int getCount(int popupLevel) {
        assert popupLevel < mListViews.size();
        return mListViews.get(popupLevel).getAdapter().getCount();
    }

    @VisibleForTesting
    public @Nullable ListItem findItem(int id) {
        for (int i = 0; i < mListViews.size(); i++) {
            for (int j = 0; j < getCount(i); j++) {
                final ListItem item = getItem(i, j);
                // If the item is a title/divider, its model does not have MENU_ID as key.
                if (item.model.getAllSetProperties().contains(MENU_ITEM_ID)
                        && item.model.get(MENU_ITEM_ID) == id) {
                    return item;
                }
            }
        }
        return null;
    }

    @VisibleForTesting
    /* package */ static ModelListAdapter createAdapter(ModelList listItems) {
        ModelListAdapter adapter =
                ListMenuUtils.createAdapter(
                        listItems, Set.of(ContextMenuItemType.HEADER), /* delegate= */ null);
        // Register types / view binders not covered by the default adapter
        adapter.registerType(
                ContextMenuItemType.HEADER,
                new LayoutViewBuilder(R.layout.context_menu_header),
                ContextMenuHeaderViewBinder::bind);
        adapter.registerType(
                ContextMenuItemType.CONTEXT_MENU_ITEM_WITH_ICON_BUTTON,
                new LayoutViewBuilder(R.layout.context_menu_row),
                ContextMenuItemViewBinder::bind);
        return adapter;
    }

    public HierarchicalMenuController getHierarchicalMenuControllerForTest() {
        return mHierarchicalMenuController;
    }

    public ContextMenuHeaderCoordinator getHeaderCoordinatorForTest() {
        return mHeaderCoordinator;
    }

    public List<ContextMenuListView> getListViewsForTest() {
        return mListViews;
    }

    public WebContentsObserver getWebContentsObserverForTesting() {
        return mWebContentsObserver;
    }

    @VisibleForTesting
    public ContextMenuParams getParams() {
        return mParams;
    }
}
