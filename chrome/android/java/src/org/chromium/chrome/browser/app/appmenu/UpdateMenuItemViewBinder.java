// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.appmenu;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.text.TextUtils;
import android.view.View;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.graphics.drawable.DrawableCompat;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.toolbar.menu_button.MenuItemState;
import org.chromium.chrome.browser.ui.appmenu.AppMenuItemProperties;
import org.chromium.chrome.browser.ui.appmenu.AppMenuUtil;
import org.chromium.chrome.browser.ui.appmenu.CustomViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;

/** A custom binder used to bind the update menu item. */
class UpdateMenuItemViewBinder implements CustomViewBinder {
    private static final int UPDATE_ITEM_VIEW_TYPE = 0;
    private final MenuItemState mItemState;

    UpdateMenuItemViewBinder(Profile profile) {
        super();
        mItemState = UpdateMenuItemHelper.getInstance(profile).getUiState().itemState;
    }

    @Override
    public int getViewTypeCount() {
        return 1;
    }

    @Override
    public int getItemViewType(int id) {
        return id == R.id.update_menu_id ? UPDATE_ITEM_VIEW_TYPE : CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public int getLayoutId(int viewType) {
        if (viewType == UPDATE_ITEM_VIEW_TYPE) {
            return R.layout.update_menu_item;
        }
        return CustomViewBinder.NOT_HANDLED;
    }

    @Override
    public void bind(PropertyModel model, View view, PropertyKey key) {
        AppMenuUtil.bindStandardItemEnterAnimation(model, view, key);

        if (key == AppMenuItemProperties.MENU_ITEM_ID) {
            int id = model.get(AppMenuItemProperties.MENU_ITEM_ID);
            assert id == R.id.update_menu_id;
            view.setId(id);

            if (mItemState != null) {
                TextView summary = view.findViewById(R.id.menu_item_summary);
                if (!TextUtils.isEmpty(mItemState.summary)) {
                    summary.setText(mItemState.summary);
                    summary.setVisibility(View.VISIBLE);
                } else {
                    summary.setText("");
                    summary.setVisibility(View.GONE);
                }
            }
        } else if (key == AppMenuItemProperties.TITLE) {
            TextView text = view.findViewById(R.id.menu_item_text);
            if (mItemState == null) {
                text.setText(model.get(AppMenuItemProperties.TITLE));
            } else {
                text.setText(mItemState.title);
                text.setTextColor(
                        AppCompatResources.getColorStateList(
                                view.getContext(), mItemState.titleColorId));
            }
        } else if (key == AppMenuItemProperties.TITLE_CONDENSED) {
            TextView text = view.findViewById(R.id.menu_item_text);
            if (mItemState == null) {
                CharSequence titleCondensed = model.get(AppMenuItemProperties.TITLE_CONDENSED);
                text.setContentDescription(titleCondensed);
            } else {
                text.setContentDescription(view.getResources().getString(mItemState.title));
            }
        } else if (key == AppMenuItemProperties.ICON) {
            ImageView image = view.findViewById(R.id.menu_item_icon);

            if (mItemState == null) {
                Drawable icon = model.get(AppMenuItemProperties.ICON);
                image.setImageDrawable(icon);
                image.setVisibility(View.VISIBLE);
                return;
            }

            image.setImageResource(mItemState.icon);
            if (mItemState.iconTintId != 0) {
                DrawableCompat.setTint(
                        image.getDrawable(), view.getContext().getColor(mItemState.iconTintId));
            }
        } else if (key == AppMenuItemProperties.ENABLED) {
            view.findViewById(R.id.menu_item_text)
                    .setEnabled(model.get(AppMenuItemProperties.ENABLED));
            if (mItemState != null) view.setEnabled(mItemState.enabled);
        } else if (key == AppMenuItemProperties.CLICK_HANDLER) {
            view.setOnClickListener(
                    v -> model.get(AppMenuItemProperties.CLICK_HANDLER).onItemClick(model));
        }
    }

    @Override
    public boolean supportsEnterAnimation(int id) {
        return true;
    }

    @Override
    public int getPixelHeight(Context context) {
        int textSize =
                context.getResources()
                        .getDimensionPixelSize(R.dimen.overflow_menu_update_min_height);
        int paddingSize =
                context.getResources().getDimensionPixelSize(R.dimen.overflow_menu_update_padding);
        int iconSize =
                AppCompatResources.getDrawable(context, R.drawable.menu_update)
                        .getIntrinsicHeight();

        return Math.max(textSize, iconSize) + paddingSize * 2 /* top padding and bottom padding */;
    }
}
