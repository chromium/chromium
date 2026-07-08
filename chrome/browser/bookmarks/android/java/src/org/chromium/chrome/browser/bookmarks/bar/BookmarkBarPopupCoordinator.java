// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Point;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.TextView;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.bookmarks.BookmarkItem;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.UiUtils;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
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
    private final MonotonicObservableSupplier<Profile> mProfileSupplier;
    private final Supplier<Pair<Integer, Integer>> mControlsHeightSupplier;
    private final BrowserControlsRectProvider mBrowserControlsRectProvider;

    private @Nullable AnchoredPopupWindow mAnchoredPopupWindow;
    private @Nullable ModelList mActiveBookmarkItems;
    private ListObservable.@Nullable ListObserver<Void> mActiveSizeUpdaterObserver;

    public BookmarkBarPopupCoordinator(
            Activity activity,
            View bookmarkBarView,
            MonotonicObservableSupplier<Profile> profileSupplier,
            Supplier<Pair<Integer, Integer>> controlsHeightSupplier) {
        mActivity = activity;
        mBookmarkBarView = bookmarkBarView;
        mProfileSupplier = profileSupplier;
        mControlsHeightSupplier = controlsHeightSupplier;
        mBrowserControlsRectProvider = new BrowserControlsRectProvider(activity);
    }

    /** Shows the right-click context menu for a bookmark item. */
    public void showBookmarkItemContextMenu(View anchorView, BookmarkItem item) {
        // TODO(crbug.com/465996578): Implement context menu for bookmark items.
    }

    /** Shows the context menu for the Bookmarks Bar empty space. */
    public void showBookmarkBarEmptySpaceContextMenu(View anchorView, Point offset) {
        // TODO(crbug.com/465996578): Implement context menu for bookmarks bar background.
    }

    /**
     * Shows a popup window listing the bookmarks and subfolders inside a bookmarks bar folder.
     *
     * @param anchorView The {@link View} used as the positioning anchor for the popup. The popup
     *     will be aligned relative to this view.
     * @param folderMenuModel A {@link ModelList} containing the property models of the bookmark
     *     items to display. {@link BookmarkBarMediator} is expected to construct this model list
     *     and populate it with the direct children of the folder before invoking this method.
     */
    public void showFolderItemsPopup(View anchorView, ModelList folderMenuModel) {
        showPopup(folderMenuModel, anchorView, null);
    }

    /** Dismisses the active popup window. */
    public void dismiss() {
        if (mAnchoredPopupWindow != null) {
            mAnchoredPopupWindow.dismiss();
        }
    }

    private void showPopup(ModelList bookmarkItems, View anchorView, @Nullable Point offset) {
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

        View popupContentView = popupListMenu.getContentView();
        popupContentView.setBackground(null);
        ListMenuUtils.clipContentViewOutline(popupContentView, R.attr.popupBgCornerRadius);

        // Set up empty view if the menu has no items.
        setupEmptyView(popupContentView);

        Pair<Integer, Integer> initialHeights = mControlsHeightSupplier.get();
        mBrowserControlsRectProvider.updateRectAndNotify(
                initialHeights.first, initialHeights.second);

        final Profile profile = mProfileSupplier.get();
        boolean isIncognito = profile != null && profile.isOffTheRecord();

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

        mAnchoredPopupWindow =
                new AnchoredPopupWindow(
                        mActivity,
                        mBookmarkBarView,
                        getMenuBackground(mActivity, isIncognito),
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

        mAnchoredPopupWindow.show();
        configurePopupWindowSize(popupListMenu);
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

    private Drawable getMenuBackground(Context context, boolean isIncognito) {
        final @DrawableRes int bgDrawableId =
                isIncognito ? R.drawable.menu_bg_tinted_on_dark_bg : R.drawable.menu_bg_tinted;

        if (!isIncognito) {
            ColorStateList menuBgColor =
                    ColorStateList.valueOf(SemanticColorUtils.getMenuBgColor(context));
            return UiUtils.getTintedDrawable(context, bgDrawableId, menuBgColor);
        }
        return AppCompatResources.getDrawable(context, bgDrawableId);
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
        int desiredWidth = Math.min(measuredDimensions[0], finalWidth);
        int desiredHeight = measuredDimensions[1];

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
