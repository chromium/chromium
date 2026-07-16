// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.ui.widget.ViewRectUpdater;

import java.util.function.Supplier;

/** Coordinates the display of all popup menus anchored to the Bookmarks Bar. */
@NullMarked
public class BookmarkBarPopupCoordinator {
    private final Activity mActivity;
    private final View mBookmarkBarView;
    private final Supplier<Pair<Integer, Integer>> mControlsHeightSupplier;
    private final BrowserControlsRectProvider mBrowserControlsRectProvider;

    private @Nullable AnchoredPopupWindow mAnchoredPopupWindow;
    private @Nullable ModelList mActiveBookmarkItems;
    private ListObservable.@Nullable ListObserver<Void> mActiveSizeUpdaterObserver;

    public BookmarkBarPopupCoordinator(
            Activity activity,
            View bookmarkBarView,
            Supplier<Pair<Integer, Integer>> controlsHeightSupplier) {
        mActivity = activity;
        mBookmarkBarView = bookmarkBarView;
        mControlsHeightSupplier = controlsHeightSupplier;
        mBrowserControlsRectProvider = new BrowserControlsRectProvider(activity);
    }

    /**
     * Shows a popup window listing the bookmarks and subfolders inside a bookmarks bar folder.
     *
     * @param anchorView The {@link View} used as the positioning anchor for the popup. The popup
     *     will be aligned relative to this view.
     * @param folderMenuModel A {@link ModelList} containing the property models of the bookmark
     *     items to display. {@link BookmarkBarMediator} is expected to construct this model list
     *     and populate it with the direct children of the folder before invoking this method.
     * @param isIncognito Whether the current profile session is incognito.
     */
    public void showFolderItemsPopup(
            View anchorView, ModelList folderMenuModel, boolean isIncognito) {
        showPopup(folderMenuModel, anchorView, null, isIncognito);
    }

    /** Dismisses the active popup window. */
    public void dismiss() {
        if (mAnchoredPopupWindow != null) {
            mAnchoredPopupWindow.dismiss();
        }
    }

    public void showPopup(
            ModelList bookmarkItems, View anchorView, @Nullable Point offset, boolean isIncognito) {
        dismiss();

        BasicListMenu popupListMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        mActivity,
                        bookmarkItems,
                        (model, view) -> {
                            View.OnClickListener clickListener =
                                    model.get(ListMenuItemProperties.CLICK_LISTENER);
                            if (clickListener != null) {
                                clickListener.onClick(view);
                            }
                        });

        popupListMenu.setupCallbacks(
                this::dismiss, ListMenuUtils.createHierarchicalMenuController(mActivity));
        // popupContentView (list_menu_layout) is the inner container housing the ListView.
        // AnchoredPopupWindow is the outer popup window container wrapping popupContentView.
        // We keep the default background (default_popup_menu_bg) on popupContentView and set
        // AnchoredPopupWindow's background to transparent for two reasons:
        // 1. Setting default_popup_menu_bg on both containers applies the 6dp shadow inset padding
        //    twice, causing a 6dp gap where list item selection highlights won't go all the way to
        //    the horizontal edges of the visible menu.
        // 2. If we strip the background from popupContentView and put default_popup_menu_bg on
        //    AnchoredPopupWindow, clipToOutline="true" on popupContentView has no outline provider
        //    and fails to clip list item highlights to the rounded corners of the popup.
        View popupContentView = popupListMenu.getContentView();
        ListMenuUtils.clipContentViewOutline(popupContentView, R.attr.popupBgCornerRadius);
        if (popupContentView.getBackground() instanceof GradientDrawable backgroundDrawable) {
            int bgColor =
                    isIncognito
                            ? mActivity.getColor(R.color.dialog_bg_color_dark_baseline)
                            : SemanticColorUtils.getMenuBgColor(mActivity);
            backgroundDrawable.setColor(bgColor);
        }

        // Set up empty view if the menu has no items.
        setupEmptyView(popupContentView);

        Pair<Integer, Integer> initialHeights = mControlsHeightSupplier.get();
        mBrowserControlsRectProvider.updateRectAndNotify(
                initialHeights.first, initialHeights.second);

        ViewRectProvider rectProvider =
                new ViewRectProvider(
                        anchorView,
                        (view, rect, onRectChanged) -> {
                            var updater = new ViewRectUpdater(view, rect, onRectChanged);
                            updater.setIncludePadding(true);
                            return updater;
                        });
        if (offset != null) {
            int left = offset.x;
            int top = offset.y;
            int right = anchorView.getWidth() - offset.x;
            int bottom = anchorView.getHeight() - offset.y;
            rectProvider.setInsetPx(left, top, right, bottom);
        }

        // Pass a transparent background to AnchoredPopupWindow to avoid double backgrounds and
        // double padding.
        mAnchoredPopupWindow =
                new AnchoredPopupWindow(
                        mActivity,
                        mBookmarkBarView,
                        new ColorDrawable(Color.TRANSPARENT),
                        popupListMenu::getContentView,
                        rectProvider,
                        mBrowserControlsRectProvider);

        mAnchoredPopupWindow.setFocusable(true);
        mAnchoredPopupWindow.setPreferredVerticalOrientation(
                AnchoredPopupWindow.VerticalOrientation.BELOW);
        mAnchoredPopupWindow.setHorizontalOverlapAnchor(true);
        mAnchoredPopupWindow.setPreferredHorizontalOrientation(
                AnchoredPopupWindow.HorizontalOrientation.LAYOUT_DIRECTION);
        mAnchoredPopupWindow.setElevation(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bookmarks_bar_popup_elevation));

        mAnchoredPopupWindow.addOnDismissListener(this::onPopupWindowDismissed);

        final ListObservable.ListObserver<Void> sizeUpdaterObserver =
                new ListObservable.ListObserver<>() {
                    private void updatePopupSize() {
                        popupContentView.post(
                                () -> {
                                    if (mAnchoredPopupWindow != null
                                            && mAnchoredPopupWindow.isShowing()) {
                                        configurePopupWindowSize(popupListMenu);
                                    }
                                });
                    }

                    @Override
                    public void onItemRangeChanged(
                            ListObservable<Void> source,
                            int index,
                            int count,
                            @Nullable Void payload) {
                        updatePopupSize();
                    }

                    @Override
                    public void onItemRangeInserted(ListObservable source, int index, int count) {
                        updatePopupSize();
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable source, int index, int count) {
                        updatePopupSize();
                    }
                };

        mActiveBookmarkItems = bookmarkItems;
        mActiveSizeUpdaterObserver = sizeUpdaterObserver;
        bookmarkItems.addObserver(sizeUpdaterObserver);

        // We must call configurePopupWindowSize before show() because AnchoredPopupWindow's
        // transparent background removes the 20dp outer background padding. When show() runs first
        // on an empty folder popup (~30dp unconfigured height), the height falls below the 50dp min
        // touchable target requirement in AnchoredPopupWindow#hasMinimalSize(), causing show() to
        // abort. Configuring the desired dimensions (182dp min height) prior to show() ensures the
        // initial minimal size check passes cleanly on transparent popups.
        configurePopupWindowSize(popupListMenu);
        mAnchoredPopupWindow.show();
    }

    private void onPopupWindowDismissed() {
        mAnchoredPopupWindow = null;
        // Remove the observer to prevent memory leaks since mActiveBookmarkItems
        // outlives the popup window.
        if (mActiveBookmarkItems != null && mActiveSizeUpdaterObserver != null) {
            mActiveBookmarkItems.removeObserver(mActiveSizeUpdaterObserver);
            mActiveBookmarkItems = null;
            mActiveSizeUpdaterObserver = null;
        }
    }

    @VisibleForTesting
    void configurePopupWindowSize(BasicListMenu popupListMenu) {
        if (mAnchoredPopupWindow == null) {
            return;
        }

        Resources resources = mActivity.getResources();
        DisplayMetrics displayMetrics = resources.getDisplayMetrics();

        int maxWidthPx = resources.getDimensionPixelSize(R.dimen.bookmarks_bar_popup_max_width);
        int finalWidth = Math.min(maxWidthPx, displayMetrics.widthPixels);

        if (popupListMenu.getContentAdapter() == null
                || popupListMenu.getContentAdapter().getCount() == 0) {
            mAnchoredPopupWindow.setDesiredContentSize(
                    finalWidth,
                    resources.getDimensionPixelSize(R.dimen.bookmarks_bar_popup_min_height));
            return;
        }

        int[] measuredDimensions = popupListMenu.getMenuDimensions();
        int minInteractSizePx =
                AttrUtils.getDimensionPixelSize(mActivity, R.attr.minInteractTargetSize);
        if (minInteractSizePx == -1) {
            minInteractSizePx = resources.getDimensionPixelSize(R.dimen.min_touch_target_size);
        }
        int marginPx = (int) Math.ceil(displayMetrics.density);
        int minTouchableSizePx = minInteractSizePx + 2 * marginPx;

        int desiredWidth =
                Math.max(Math.min(measuredDimensions[0], finalWidth), minTouchableSizePx);
        int desiredHeight = Math.max(measuredDimensions[1], minTouchableSizePx);

        if (mBrowserControlsRectProvider.getRect() != null) {
            int availableHeight = mBrowserControlsRectProvider.getRect().height();

            ListView menuList = popupListMenu.getContentView().findViewById(R.id.menu_list);
            boolean needsScrollbar = desiredHeight > availableHeight;
            menuList.setVerticalScrollBarEnabled(needsScrollbar);
            menuList.setScrollbarFadingEnabled(needsScrollbar);
        }

        mAnchoredPopupWindow.setDesiredContentSize(desiredWidth, desiredHeight);
    }

    public void onBrowserControlsChanged(int topControlsHeight, int bottomControlsHeight) {
        mBrowserControlsRectProvider.updateRectAndNotify(topControlsHeight, bottomControlsHeight);
    }

    @VisibleForTesting
    void setupEmptyView(View popupContentView) {
        ListView menuList = popupContentView.findViewById(R.id.menu_list);
        if (menuList == null) return;

        ViewGroup contentParent = (ViewGroup) popupContentView;

        TextView emptyView = contentParent.findViewById(R.id.bookmarks_bar_empty_view);
        if (emptyView == null) {
            emptyView = new TextView(mActivity);
            emptyView.setId(R.id.bookmarks_bar_empty_view);
            emptyView.setText(R.string.bookmarks_bar_empty_message);
            emptyView.setGravity(Gravity.CENTER);

            emptyView.setLayoutParams(
                    new LinearLayout.LayoutParams(
                            LinearLayout.LayoutParams.MATCH_PARENT,
                            LinearLayout.LayoutParams.MATCH_PARENT));

            contentParent.addView(emptyView);
        }

        menuList.setEmptyView(emptyView);
    }

    void setAnchoredPopupWindowForTesting(AnchoredPopupWindow anchoredPopupWindow) {
        mAnchoredPopupWindow = anchoredPopupWindow;
    }

    private static class BrowserControlsRectProvider extends RectProvider {
        private final Activity mActivity;

        BrowserControlsRectProvider(Activity activity) {
            mActivity = activity;
        }

        public void updateRectAndNotify(int topControlsHeight, int bottomControlsHeight) {
            DisplayMetrics displayMetrics = mActivity.getResources().getDisplayMetrics();
            mRect.set(0, 0, displayMetrics.widthPixels, displayMetrics.heightPixels);
            mRect.top += topControlsHeight;
            mRect.bottom -= bottomControlsHeight;

            notifyRectChanged();
        }
    }
}
