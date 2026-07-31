// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.GradientDrawable;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
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
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.ui.widget.ViewRectUpdater;

import java.util.function.Supplier;

/**
 * Manages the lifecycle, view binding, and size calculation for a single anchored popup window
 * displayed on the Bookmarks Bar.
 */
@NullMarked
class BookmarkBarPopup {
    private final Activity mActivity;
    private final Supplier<Pair<Integer, Integer>> mControlsHeightSupplier;
    private final BrowserControlsRectProvider mBrowserControlsRectProvider;

    private @Nullable AnchoredPopupWindow mPopupWindow;
    private @Nullable View mContentView;
    private @Nullable ModelList mModelList;
    private @Nullable ListObserver<Void> mSizeObserver;
    private final int[] mLocation = new int[2];

    BookmarkBarPopup(Activity activity, Supplier<Pair<Integer, Integer>> controlsHeightSupplier) {
        mActivity = activity;
        mControlsHeightSupplier = controlsHeightSupplier;
        mBrowserControlsRectProvider = new BrowserControlsRectProvider(activity);
    }

    void show(
            View anchorView,
            @Nullable Point offset,
            ModelList menuModel,
            boolean isIncognito,
            Runnable dismissAllCallback,
            Runnable onDismissListener,
            @Nullable OnTouchListener touchListener,
            @Nullable OnTouchListener touchInterceptor) {
        dismiss();

        Context listContext =
                isIncognito
                        ? new ContextThemeWrapper(
                                mActivity, R.style.ThemeOverlay_BrowserUI_TabbedMode_Incognito)
                        : mActivity;
        BasicListMenu popupListMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        listContext,
                        menuModel,
                        (model, view) -> {
                            OnClickListener clickListener =
                                    model.get(ListMenuItemProperties.CLICK_LISTENER);
                            if (clickListener != null) {
                                clickListener.onClick(view);
                            }
                        });
        popupListMenu.setupCallbacks(
                dismissAllCallback, ListMenuUtils.createHierarchicalMenuController(mActivity));

        mContentView = popupListMenu.getContentView();
        if (touchListener != null) {
            mContentView.setOnTouchListener(touchListener);
        }
        ListMenuUtils.clipContentViewOutline(mContentView, R.attr.popupBgCornerRadius);
        if (mContentView.getBackground() instanceof GradientDrawable bg) {
            int color =
                    isIncognito
                            ? mActivity.getColor(R.color.dialog_bg_color_dark_baseline)
                            : SemanticColorUtils.getMenuBgColor(mActivity);
            bg.setColor(color);
        }
        setupEmptyView(mContentView);

        Pair<Integer, Integer> heights = mControlsHeightSupplier.get();
        mBrowserControlsRectProvider.updateRectAndNotify(heights.first, heights.second);

        // Two RectProviders are used here in a chain:
        // 1. baseRectProvider calculates the raw bounds of the anchor relative to its own window.
        // 2. translatedRectProvider wraps it to shift those bounds into the main Activity's window
        //    space, which is required for AnchoredPopupWindow to position itself correctly.
        ViewRectProvider baseRectProvider =
                new ViewRectProvider(
                        anchorView,
                        (view, rect, onRectChanged) -> {
                            var updater = new ViewRectUpdater(view, rect, onRectChanged);
                            updater.setIncludePadding(true);
                            return updater;
                        });
        if (offset != null) {
            baseRectProvider.setInsetPx(
                    offset.x,
                    offset.y,
                    anchorView.getWidth() - offset.x,
                    anchorView.getHeight() - offset.y);
        }

        RectProvider translatedRectProvider =
                new TranslatedRectProvider(baseRectProvider, anchorView, mActivity);

        mPopupWindow =
                new AnchoredPopupWindow(
                        mActivity,
                        mActivity.getWindow().getDecorView(),
                        new ColorDrawable(Color.TRANSPARENT),
                        popupListMenu::getContentView,
                        translatedRectProvider,
                        mBrowserControlsRectProvider);

        mPopupWindow.setFocusable(true);
        mPopupWindow.setOutsideTouchable(true);
        if (touchInterceptor != null) {
            mPopupWindow.setTouchInterceptor(touchInterceptor);
        }
        mPopupWindow.setPreferredVerticalOrientation(AnchoredPopupWindow.VerticalOrientation.BELOW);
        mPopupWindow.setHorizontalOverlapAnchor(true);
        mPopupWindow.setPreferredHorizontalOrientation(
                AnchoredPopupWindow.HorizontalOrientation.LAYOUT_DIRECTION);
        mPopupWindow.setElevation(
                mActivity
                        .getResources()
                        .getDimensionPixelSize(R.dimen.bookmarks_bar_popup_elevation));

        mModelList = menuModel;
        mSizeObserver =
                new ListObserver<>() {
                    @Override
                    public void onItemRangeChanged(
                            ListObservable<Void> s, int i, int c, @Nullable Void p) {
                        updateSize();
                    }

                    @Override
                    public void onItemRangeInserted(ListObservable s, int i, int c) {
                        updateSize();
                    }

                    @Override
                    public void onItemRangeRemoved(ListObservable s, int i, int c) {
                        updateSize();
                    }

                    private void updateSize() {
                        if (mContentView != null) {
                            mContentView.post(
                                    () -> {
                                        if (mPopupWindow != null && mPopupWindow.isShowing()) {
                                            configurePopupWindowSize(mPopupWindow, popupListMenu);
                                        }
                                    });
                        }
                    }
                };
        menuModel.addObserver(mSizeObserver);

        mPopupWindow.addOnDismissListener(
                () -> {
                    cleanup();
                    if (onDismissListener != null) onDismissListener.run();
                });

        configurePopupWindowSize(mPopupWindow, popupListMenu);
        mPopupWindow.show();
    }

    void dismiss() {
        if (mPopupWindow != null) {
            mPopupWindow.dismiss();
        }
    }

    void onBrowserControlsChanged(int topControlsHeight, int bottomControlsHeight) {
        mBrowserControlsRectProvider.updateRectAndNotify(topControlsHeight, bottomControlsHeight);
    }

    private void cleanup() {
        mPopupWindow = null;
        mContentView = null;
        if (mModelList != null && mSizeObserver != null) {
            mModelList.removeObserver(mSizeObserver);
            mModelList = null;
            mSizeObserver = null;
        }
    }

    boolean isShowing() {
        return mPopupWindow != null && mPopupWindow.isShowing();
    }

    @Nullable AnchoredPopupWindow getPopupWindow() {
        return mPopupWindow;
    }

    void setPopupWindowForTesting(@Nullable AnchoredPopupWindow popupWindow) {
        mPopupWindow = popupWindow;
    }

    boolean containsTouch(float rawX, float rawY) {
        if (mPopupWindow == null || !mPopupWindow.isShowing() || mContentView == null) {
            return false;
        }
        mContentView.getLocationOnScreen(mLocation);
        return rawX >= mLocation[0]
                && rawX <= mLocation[0] + mContentView.getWidth()
                && rawY >= mLocation[1]
                && rawY <= mLocation[1] + mContentView.getHeight();
    }

    @VisibleForTesting
    void configurePopupWindowSize(
            @Nullable AnchoredPopupWindow popupWindow, BasicListMenu popupListMenu) {
        if (popupWindow == null) return;

        Resources resources = mActivity.getResources();
        DisplayMetrics displayMetrics = resources.getDisplayMetrics();
        int maxWidthPx = resources.getDimensionPixelSize(R.dimen.bookmarks_bar_popup_max_width);
        int finalWidth = Math.min(maxWidthPx, displayMetrics.widthPixels);

        if (popupListMenu.getContentAdapter() == null
                || popupListMenu.getContentAdapter().getCount() == 0) {
            popupWindow.setDesiredContentSize(
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
            ListView menuList = popupListMenu.getContentView().findViewById(R.id.menu_list);
            if (menuList != null) {
                menuList.setVerticalScrollBarEnabled(true);
                menuList.setScrollbarFadingEnabled(true);
            }
        }

        popupWindow.setDesiredContentSize(desiredWidth, desiredHeight);
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

    static class BrowserControlsRectProvider extends RectProvider {
        private final Activity mActivity;

        BrowserControlsRectProvider(Activity activity) {
            mActivity = activity;
        }

        void updateRectAndNotify(int topControlsHeight, int bottomControlsHeight) {
            View rootView = mActivity.getWindow().getDecorView();
            mRect.set(0, 0, rootView.getWidth(), rootView.getHeight());
            mRect.top += topControlsHeight;
            mRect.bottom -= bottomControlsHeight;
            notifyRectChanged();
        }
    }

    private static class TranslatedRectProvider extends RectProvider {
        private final Rect mTranslatedRect = new Rect();
        private final int[] mMainRootCoords = new int[2];
        private final int[] mAnchorRootCoords = new int[2];
        private final RectProvider mBaseRectProvider;
        private final View mAnchorView;
        private final Activity mActivity;

        TranslatedRectProvider(RectProvider baseRectProvider, View anchorView, Activity activity) {
            mBaseRectProvider = baseRectProvider;
            mAnchorView = anchorView;
            mActivity = activity;
        }

        @Override
        public void startObserving(Observer observer) {
            super.startObserving(observer);
            mBaseRectProvider.startObserving(
                    new Observer() {
                        @Override
                        public void onRectChanged() {
                            notifyRectChanged();
                        }

                        @Override
                        public void onRectHidden() {
                            notifyRectHidden();
                        }
                    });
        }

        @Override
        public void stopObserving() {
            super.stopObserving();
            mBaseRectProvider.stopObserving();
        }

        @Override
        public Rect getRect() {
            Rect original = mBaseRectProvider.getRect();
            if (original == null) return new Rect();
            mTranslatedRect.set(original);

            // We translate coordinates from the anchor view's root window to the main
            // Activity's window. This ensures the context menu popup is positioned
            // correctly even if it's anchored to an item inside a folder popup.
            mActivity.getWindow().getDecorView().getLocationOnScreen(mMainRootCoords);
            mAnchorView.getRootView().getLocationOnScreen(mAnchorRootCoords);

            mTranslatedRect.offset(
                    mAnchorRootCoords[0] - mMainRootCoords[0],
                    mAnchorRootCoords[1] - mMainRootCoords[1]);
            return mTranslatedRect;
        }
    }
}
