// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.bookmarks.bar;

import android.app.Activity;
import android.content.Context;
import android.content.res.Resources;
import android.graphics.Color;
import android.graphics.Point;
import android.graphics.PorterDuff;
import android.graphics.Rect;
import android.graphics.drawable.ColorDrawable;
import android.graphics.drawable.Drawable;
import android.util.DisplayMetrics;
import android.util.Pair;
import android.view.ContextThemeWrapper;
import android.view.Gravity;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnTouchListener;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityNodeInfo;
import android.widget.FrameLayout;
import android.widget.LinearLayout;
import android.widget.ListView;
import android.widget.RadioGroup;
import android.widget.TextView;

import androidx.annotation.VisibleForTesting;
import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.bookmarks.R;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.hierarchicalmenu.FlyoutController;
import org.chromium.ui.hierarchicalmenu.FlyoutController.FlyoutHandler;
import org.chromium.ui.hierarchicalmenu.HierarchicalMenuController;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenuItemProperties;
import org.chromium.ui.listmenu.ListMenuUtils;
import org.chromium.ui.modelutil.ListObservable;
import org.chromium.ui.modelutil.ListObservable.ListObserver;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.FlyoutPopupSpecCalculator;
import org.chromium.ui.widget.RectProvider;
import org.chromium.ui.widget.ViewRectProvider;
import org.chromium.ui.widget.ViewRectUpdater;

import java.util.List;
import java.util.function.Supplier;

/**
 * Manages the lifecycle, view binding, and size calculation for a single anchored popup window
 * displayed on the Bookmarks Bar.
 */
@NullMarked
class BookmarkBarPopup implements FlyoutHandler<AnchoredPopupWindow> {
    private final Activity mActivity;
    private final Supplier<Pair<Integer, Integer>> mControlsHeightSupplier;
    private final BrowserControlsRectProvider mBrowserControlsRectProvider;

    private @Nullable AnchoredPopupWindow mPopupWindow;
    private @Nullable View mContentView;
    private @Nullable HierarchicalMenuController<AnchoredPopupWindow> mHierarchicalMenuController;
    private @Nullable View mAnchorView;
    private boolean mIsIncognito;
    private @Nullable ModelList mModelList;
    private @Nullable ListObserver<Void> mSizeObserver;
    private final int[] mLocation = new int[2];

    BookmarkBarPopup(Activity activity, Supplier<Pair<Integer, Integer>> controlsHeightSupplier) {
        mActivity = activity;
        mControlsHeightSupplier = controlsHeightSupplier;
        mBrowserControlsRectProvider = new BrowserControlsRectProvider(activity);
    }

    /**
     * Creates a 2-layer container view structure for the popup menu. The outer FrameLayout holds
     * the 9-patch shadow drawable (popup_bg_shadow), while the inner view draws the rounded
     * background shape (popup_bg_shape) and clips list items to its rounded corners.
     */
    private View createPopupContentView(View menuContentView, boolean isIncognito) {
        FrameLayout outerContainer = new FrameLayout(mActivity);
        outerContainer.setLayoutParams(
                new ViewGroup.LayoutParams(
                        ViewGroup.LayoutParams.WRAP_CONTENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        Drawable shadowDrawable =
                AppCompatResources.getDrawable(mActivity, R.drawable.popup_bg_shadow_16dp);
        if (isIncognito && shadowDrawable != null) {
            shadowDrawable = shadowDrawable.mutate();
            shadowDrawable.setTint(mActivity.getColor(R.color.dialog_bg_color_dark_baseline));
            shadowDrawable.setTintMode(PorterDuff.Mode.MULTIPLY);
        }
        outerContainer.setBackground(shadowDrawable);

        menuContentView.setBackground(
                AppCompatResources.getDrawable(
                        mActivity,
                        isIncognito
                                ? R.drawable.menu_bg_tinted_on_dark_bg
                                : R.drawable.popup_bg_shape_16dp));
        menuContentView.setClipToOutline(true);
        menuContentView.setElevation(0);

        outerContainer.addView(menuContentView);
        return outerContainer;
    }

    void show(
            View anchorView,
            ModelList menuModel,
            boolean isIncognito,
            Runnable dismissAllCallback,
            @Nullable Runnable onDismissListener,
            @Nullable OnTouchListener touchListener,
            @Nullable OnTouchListener touchInterceptor) {
        showImpl(
                anchorView,
                new Point(0, 0),
                menuModel,
                isIncognito,
                dismissAllCallback,
                onDismissListener,
                touchListener,
                touchInterceptor,
                /* anchorToPoint= */ false);
    }

    void showAtOffset(
            View anchorView,
            Point offset,
            ModelList menuModel,
            boolean isIncognito,
            Runnable dismissAllCallback,
            @Nullable Runnable onDismissListener,
            @Nullable OnTouchListener touchListener,
            @Nullable OnTouchListener touchInterceptor) {
        showImpl(
                anchorView,
                offset,
                menuModel,
                isIncognito,
                dismissAllCallback,
                onDismissListener,
                touchListener,
                touchInterceptor,
                /* anchorToPoint= */ true);
    }

    private void showImpl(
            View anchorView,
            Point offset,
            ModelList menuModel,
            boolean isIncognito,
            Runnable dismissAllCallback,
            @Nullable Runnable onDismissListener,
            @Nullable OnTouchListener touchListener,
            @Nullable OnTouchListener touchInterceptor,
            boolean anchorToPoint) {
        dismiss();

        mAnchorView = anchorView;
        mIsIncognito = isIncognito;
        mHierarchicalMenuController = ListMenuUtils.createHierarchicalMenuController(mActivity);

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
        popupListMenu.setupCallbacks(dismissAllCallback, mHierarchicalMenuController);

        final View contentView =
                createPopupContentView(popupListMenu.getContentView(), isIncognito);
        mContentView = contentView;
        if (touchListener != null) {
            contentView.setOnTouchListener(touchListener);
        }
        setupEmptyView(contentView);

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
        baseRectProvider.setInsetPx(
                offset.x - contentView.getPaddingLeft(),
                offset.y,
                anchorToPoint ? anchorView.getWidth() - offset.x : 0,
                (anchorToPoint ? anchorView.getHeight() - offset.y : 0)
                        + contentView.getPaddingTop());

        RectProvider translatedRectProvider =
                new TranslatedRectProvider(baseRectProvider, anchorView, mActivity);

        mPopupWindow =
                new AnchoredPopupWindow(
                        mActivity,
                        mActivity.getWindow().getDecorView(),
                        new ColorDrawable(Color.TRANSPARENT),
                        () -> contentView,
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
        mPopupWindow.setAnimateFromAnchor(true);

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

        mHierarchicalMenuController.setupFlyoutController(
                this,
                mPopupWindow,
                popupListMenu::addOnScrollListener,
                /* drillDownOverrideValue= */ ChromeFeatureList.sFlyoutInBookmarksBar.isEnabled()
                        ? null
                        : true);
        mHierarchicalMenuController.setupBackPressBehaviorForPopupWindow(
                mPopupWindow.getContentView(), this::dismiss);
    }

    void dismiss() {
        if (mHierarchicalMenuController != null
                && mHierarchicalMenuController.getFlyoutController() != null) {
            mHierarchicalMenuController.destroyFlyoutController();
        }
        if (mPopupWindow != null) {
            mPopupWindow.dismiss();
        }
    }

    void onBrowserControlsChanged(int topControlsHeight, int bottomControlsHeight) {
        mBrowserControlsRectProvider.updateRectAndNotify(topControlsHeight, bottomControlsHeight);
    }

    private void cleanup() {
        if (mHierarchicalMenuController != null
                && mHierarchicalMenuController.getFlyoutController() != null) {
            mHierarchicalMenuController.destroyFlyoutController();
        }
        mHierarchicalMenuController = null;
        mAnchorView = null;
        mPopupWindow = null;
        mContentView = null;
        if (mModelList != null && mSizeObserver != null) {
            mModelList.removeObserver(mSizeObserver);
            mModelList = null;
            mSizeObserver = null;
        }
    }

    @Override
    public Rect getPopupRect(AnchoredPopupWindow popupWindow) {
        View contentView = popupWindow.getContentView();
        if (contentView == null) {
            return new Rect();
        }
        return ListMenuUtils.getViewRectRelativeToItsRootView(contentView);
    }

    @Override
    public void dismissPopup(AnchoredPopupWindow popupWindow) {
        popupWindow.dismiss();
    }

    @Override
    public void setWindowFocus(AnchoredPopupWindow popupWindow, boolean hasFocus) {
        popupWindow.setFocusable(hasFocus);
        ViewGroup contentView = (ViewGroup) popupWindow.getContentView();
        if (contentView == null) return;
        HierarchicalMenuController.setWindowFocusForFlyoutMenus(contentView, hasFocus);
    }

    @Override
    public AnchoredPopupWindow createAndShowFlyoutPopup(
            List<ListItem> items,
            View view,
            Runnable dismissRunnable,
            View.OnScrollChangeListener scrollListener) {
        ModelList modelList = new ModelList();
        modelList.addAll(items);

        BasicListMenu menu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        mActivity,
                        modelList,
                        (model, v) -> {
                            OnClickListener clickListener =
                                    model.get(ListMenuItemProperties.CLICK_LISTENER);
                            if (clickListener != null) {
                                clickListener.onClick(v);
                            }
                        });
        menu.addOnScrollListener(scrollListener);

        int radioItemCount = countRadioGroupItems(items);
        if (radioItemCount > 0) {
            setRadioGroupAccessibilityDelegate(menu.getListView(), radioItemCount);
        }

        View contentView = createPopupContentView(menu.getContentView(), mIsIncognito);
        setupEmptyView(contentView);

        int lateralPadding = contentView.getPaddingLeft() + contentView.getPaddingRight();
        View rootView = mAnchorView != null ? mAnchorView.getRootView() : view.getRootView();

        AnchoredPopupWindow popupMenu =
                new AnchoredPopupWindow.Builder(
                                mActivity,
                                rootView,
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> contentView,
                                new RectProvider(
                                        FlyoutController.calculateFlyoutAnchorRect(view, rootView)))
                        .setVerticalOverlapAnchor(true)
                        .setHorizontalOverlapAnchor(false)
                        .setMaxWidth(
                                mActivity
                                        .getResources()
                                        .getDimensionPixelSize(
                                                R.dimen.bookmarks_bar_popup_max_width))
                        .setFocusable(true)
                        .setTouchModal(false)
                        .setAnimateFromAnchor(false)
                        .setAnimationStyle(R.style.PopupWindowAnimFade)
                        .setSpecCalculator(
                                new FlyoutPopupSpecCalculator(
                                        menu.getContentView().getPaddingTop()))
                        .setDesiredContentWidth(menu.getMaxItemWidth() + lateralPadding)
                        .addOnDismissListener(dismissRunnable::run)
                        .build();

        popupMenu.show();
        return popupMenu;
    }

    private void setRadioGroupAccessibilityDelegate(ListView listView, int itemCount) {
        listView.setAccessibilityDelegate(
                new View.AccessibilityDelegate() {
                    @Override
                    public void onInitializeAccessibilityNodeInfo(
                            View host, AccessibilityNodeInfo info) {
                        super.onInitializeAccessibilityNodeInfo(host, info);
                        info.setClassName(RadioGroup.class.getName());
                        info.setCollectionInfo(
                                AccessibilityNodeInfo.CollectionInfo.obtain(
                                        /* rowCount= */ itemCount,
                                        /* columnCount= */ 1,
                                        /* hierarchical= */ false,
                                        AccessibilityNodeInfo.CollectionInfo
                                                .SELECTION_MODE_SINGLE));
                    }
                });
    }

    private static int countRadioGroupItems(List<ListItem> items) {
        int radioCount = 0;
        for (ListItem item : items) {
            if (item.model != null
                    && item.model.containsKey(ListMenuItemProperties.CHECKABLE)
                    && item.model.get(ListMenuItemProperties.CHECKABLE)
                    && item.model.containsKey(ListMenuItemProperties.POSITION)) {
                radioCount++;
            }
        }
        return radioCount;
    }

    @Nullable HierarchicalMenuController<AnchoredPopupWindow>
            getHierarchicalMenuControllerForTesting() {
        return mHierarchicalMenuController;
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

        int horizontalPadding =
                mContentView != null
                        ? mContentView.getPaddingLeft() + mContentView.getPaddingRight()
                        : 0;
        int verticalPadding =
                mContentView != null
                        ? mContentView.getPaddingTop() + mContentView.getPaddingBottom()
                        : 0;

        int contentWidth = popupListMenu.getMaxItemWidth() + horizontalPadding;
        int desiredWidth = Math.max(Math.min(contentWidth, finalWidth), minTouchableSizePx);
        int desiredHeight = Math.max(measuredDimensions[1] + verticalPadding, minTouchableSizePx);

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
