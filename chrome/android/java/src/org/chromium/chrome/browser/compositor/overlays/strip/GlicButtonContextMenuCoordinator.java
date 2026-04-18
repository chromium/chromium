// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.overlays.strip;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.ui.listmenu.ListMenuItemProperties.MENU_ITEM_ID;

import android.app.Activity;
import android.content.Context;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.MathUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.glic.GlicUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.components.browser_ui.widget.ListItemBuilder;
import org.chromium.ui.listmenu.BasicListMenu;
import org.chromium.ui.listmenu.ListMenu.Delegate;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.widget.AnchoredPopupWindow;
import org.chromium.ui.widget.AnchoredPopupWindow.HorizontalOrientation;
import org.chromium.ui.widget.RectProvider;

/** Coordinator for the Glic button context menu. */
@NullMarked
public class GlicButtonContextMenuCoordinator {
    private final Context mContext;
    private @Nullable AnchoredPopupWindow mMenuWindow;

    /**
     * @param context The current Android {@link Context}.
     */
    public GlicButtonContextMenuCoordinator(Context context) {
        mContext = context;
    }

    /**
     * Shows the Glic button context menu.
     *
     * @param anchorRectProvider The {@link RectProvider} for the anchor view.
     * @param activity The {@link Activity} in which the menu is shown.
     * @param menuWidth The width of the menu in dp.
     */
    public void showMenu(
            RectProvider anchorViewRectProvider,
            Activity activity,
            Profile profile,
            float menuWidth) {
        ModelList modelList = new ModelList();
        modelList.add(
                new ListItemBuilder()
                        .withTitleRes(R.string.glic_button_cxmenu_unpin)
                        .withMenuId(R.id.unpin_glic)
                        .withIsIncognito(false)
                        .build());

        BasicListMenu listMenu =
                BrowserUiListMenuUtils.getBasicListMenu(
                        mContext, modelList, getListMenuDelegate(profile));
        View contentView = listMenu.getContentView();
        View decorView = activity.getWindow().getDecorView();
        var popupWidthPx =
                MathUtils.clamp(
                        (int) (menuWidth * mContext.getResources().getDisplayMetrics().density),
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tab_strip_context_menu_min_width),
                        mContext.getResources()
                                .getDimensionPixelSize(R.dimen.tab_strip_context_menu_max_width));
        AnchoredPopupWindow.Builder builder =
                new AnchoredPopupWindow.Builder(
                                mContext,
                                decorView,
                                new ColorDrawable(Color.TRANSPARENT),
                                () -> contentView,
                                anchorViewRectProvider)
                        .setFocusable(true)
                        .setOutsideTouchable(true)
                        .setHorizontalOverlapAnchor(true)
                        .setVerticalOverlapAnchor(false)
                        .setPreferredHorizontalOrientation(HorizontalOrientation.LAYOUT_DIRECTION)
                        .setDesiredContentWidth(popupWidthPx)
                        .setElevation(
                                contentView
                                        .getResources()
                                        .getDimension(R.dimen.tab_overflow_menu_elevation))
                        .setAnimateFromAnchor(true);
        mMenuWindow = builder.build();
        mMenuWindow.show();
    }

    @VisibleForTesting
    @Nullable AnchoredPopupWindow getPopupWindow() {
        return mMenuWindow;
    }

    @VisibleForTesting
    Delegate getListMenuDelegate(Profile profile) {
        return (model, view) -> {
            if (model.get(MENU_ITEM_ID) == R.id.unpin_glic) {
                GlicUtils.setButtonPinnedToTabStrip(profile, false);
            }
            assumeNonNull(mMenuWindow).dismiss();
        };
    }

    /** Dismisses the menu. */
    public void dismiss() {
        if (mMenuWindow != null) {
            mMenuWindow.dismiss();
            mMenuWindow = null;
        }
    }

    /** Returns true if the Glic button menu is showing */
    public boolean isShowing() {
        return mMenuWindow != null && mMenuWindow.isShowing();
    }
}
