// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.ui.listmenu.ListMenuItemProperties.CLICK_LISTENER;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;
import static org.chromium.ui.listmenu.ListMenuUtils.createAdapter;

import android.app.Activity;
import android.content.ComponentCallbacks;
import android.content.Context;
import android.content.res.Configuration;
import android.content.res.Resources;
import android.database.DataSetObserver;
import android.graphics.drawable.Drawable;
import android.graphics.drawable.GradientDrawable;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.LayoutRes;
import androidx.annotation.StyleRes;

import org.chromium.base.Callback;
import org.chromium.base.lifetime.LifetimeAssert;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.TabOverflowMenuCoordinator.OnItemClickedCallback;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.list_view.TouchTrackingListView;
import org.chromium.ui.UiUtils;
import org.chromium.ui.listmenu.ListMenuItemAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.util.AttrUtils;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.FlyoutPopupSpecCalculator;
import org.chromium.ui.widget.RectProvider;

import java.util.Set;

/**
 * A coordinator for the overflow menu for tabs and tab groups. This applies to both the
 * TabGridDialog toolbar and tab group cards on GTS. It is responsible for creating a list of menu
 * items, setting up the menu, and displaying the menu.
 *
 * @param <T> The type of the ID of the overflow menu's origin. For individual tabs, this is a tab
 *     ID. For tab groups, it's the tab group ID.
 */
@NullMarked
public class TabOverflowMenuHolder<T> {
    private final Context mContext;
    private final View mContentView;
    private final ComponentCallbacks mComponentCallbacks;
    private final ModelList mModelList;
    private final @Nullable LifetimeAssert mLifetimeAssert = LifetimeAssert.create(this);
    private AnchoredPopupWindow mMenuWindow;

    TabOverflowMenuHolder(
            RectProvider anchorViewRectProvider,
            boolean horizontalOverlapAnchor,
            boolean verticalOverlapAnchor,
            @StyleRes int animStyle,
            @HorizontalOrientation int horizontalOrientation,
            @LayoutRes int menuLayout,
            Drawable menuBackground,
            ModelList modelList,
            OnItemClickedCallback<T> onItemClickedCallback,
            T id,
            @Nullable String collaborationId,
            int popupMaxWidthPx,
            @Nullable Callback<TabOverflowMenuHolder<T>> onDismiss,
            Activity activity,
            boolean isFlyout) {
        mModelList = modelList;
        mContext = new ContextThemeWrapper(activity, R.style.OverflowMenuThemeOverlay);
        mComponentCallbacks =
                new ComponentCallbacks() {
                    @Override
                    public void onConfigurationChanged(Configuration newConfig) {
                        if (mMenuWindow == null || !mMenuWindow.isShowing()) return;
                        mMenuWindow.dismiss();
                    }

                    @Override
                    public void onLowMemory() {}
                };
        mContext.registerComponentCallbacks(mComponentCallbacks);

        mContentView = LayoutInflater.from(mContext).inflate(menuLayout, null);
        clipContentViewOutline();

        TouchTrackingListView touchTrackingListView =
                mContentView.findViewById(R.id.tab_group_action_menu_list);
        ListMenuItemAdapter adapter =
                createAdapter(
                        modelList,
                        Set.of(),
                        (model, view) -> {
                            // Because ListMenuItemAdapter always uses the delegate if there is
                            // one, we need to manually call click listeners.
                            if (model.containsKey(CLICK_LISTENER)
                                    && model.get(CLICK_LISTENER) != null) {
                                model.get(CLICK_LISTENER).onClick(mContentView);
                                return;
                            }
                            onItemClickedCallback.onClick(
                                    model.get(MENU_ITEM_ID),
                                    id,
                                    collaborationId,
                                    /* listViewTouchTracker= */ touchTrackingListView);
                            mMenuWindow.dismiss();
                        });
        touchTrackingListView.setItemsCanFocus(true);
        touchTrackingListView.setAdapter(adapter);

        View decorView = activity.getWindow().getDecorView();

        AnchoredPopupWindow.Builder builder =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                decorView,
                                menuBackground,
                                () -> mContentView,
                                anchorViewRectProvider)
                        .setFocusable(true)
                        .setOutsideTouchable(true)
                        .setHorizontalOverlapAnchor(horizontalOverlapAnchor)
                        .setVerticalOverlapAnchor(verticalOverlapAnchor)
                        .setPreferredHorizontalOrientation(horizontalOrientation)
                        .setMaxWidth(popupMaxWidthPx)
                        .setAllowNonTouchableSize(true)
                        .setElevation(
                                mContentView
                                        .getResources()
                                        .getDimension(R.dimen.tab_overflow_menu_elevation));

        if (isFlyout) {
            builder.setAnimationStyle(R.style.PopupWindowAnimFade);
            builder.setSpecCalculator(new FlyoutPopupSpecCalculator());
            builder.setDesiredContentWidth(
                    UiUtils.computeListAdapterContentDimensions(adapter, touchTrackingListView)[0]);
        } else {
            // Override animation style or animate from anchor as default.
            if (animStyle == Resources.ID_NULL) {
                builder.setAnimateFromAnchor(true);
            } else {
                builder.setAnimationStyle(animStyle);
            }
        }

        mMenuWindow = builder.build();

        // Resize if any new elements are added.
        adapter.registerDataSetObserver(
                new DataSetObserver() {
                    @Override
                    public void onChanged() {
                        resize();
                    }
                });

        // When the menu is dismissed, call destroy to unregister the orientation listener.
        mMenuWindow.addOnDismissListener(
                () -> {
                    if (onDismiss != null) {
                        onDismiss.onResult(this);
                    }
                    destroy();
                });
    }

    AnchoredPopupWindow getMenuWindow() {
        return mMenuWindow;
    }

    ModelList getModelList() {
        return mModelList;
    }

    View getContentView() {
        return mContentView;
    }

    void show() {
        mMenuWindow.show();
    }

    void resize() {
        mMenuWindow.onRectChanged();
    }

    void dismiss() {
        mMenuWindow.dismiss();
    }

    void destroy() {
        mContext.unregisterComponentCallbacks(mComponentCallbacks);
        // If mLifetimeAssert is GC'ed before this is called, it will throw an exception
        // with a stack trace showing the stack during LifetimeAssert.create().
        LifetimeAssert.destroy(mLifetimeAssert);
    }

    private void clipContentViewOutline() {
        GradientDrawable outlineDrawable = new GradientDrawable();
        outlineDrawable.setShape(GradientDrawable.RECTANGLE);
        outlineDrawable.setCornerRadius(
                AttrUtils.getDimensionPixelSize(
                        mContentView.getContext(), R.attr.popupBgCornerRadius));
        mContentView.setBackground(outlineDrawable);
        mContentView.setClipToOutline(true);
    }
}
