// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.MenuItem;
import android.view.View;
import android.widget.Button;

import androidx.annotation.Nullable;
import androidx.core.view.MenuItemCompat;
import androidx.core.widget.TextViewCompat;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;

import java.util.List;

/**
 * View holder for a {@link MenuItem} with an optional {@code mActionView} to show in a toolbar.
 */
public class TabSelectionEditorMenuItem {
    private Context mContext;
    private MenuItem mMenuItem;

    private Button mActionView;

    private Runnable mOnClickRunnable;
    private Callback<List<Integer>> mOnSelectionStateChange;

    /**
     * @param context for loading resources.
     * @param menuItem the {@link MenuItem} this view owns.
     */
    TabSelectionEditorMenuItem(Context context, MenuItem menuItem) {
        mContext = context;
        mMenuItem = menuItem;
    }

    /**
     * Initializes the {@code mActionView} if applicable.
     * @param showMode whether to show the action view.
     * @param buttonType the button layout of the action view.
     */
    public void initActionView(@ShowMode int showMode, @ButtonType int buttonType) {
        final boolean showText =
                buttonType == ButtonType.TEXT || buttonType == ButtonType.ICON_AND_TEXT;
        final boolean showIcon =
                buttonType == ButtonType.ICON || buttonType == ButtonType.ICON_AND_TEXT;

        if (!showText && !showIcon) {
            // Force menu mode if the button has no content.
            showMode = ShowMode.MENU_ONLY;
        }

        mMenuItem.setShowAsAction(getShowAsAction(showMode));
        if (showMode == ShowMode.MENU_ONLY) return;

        // TODO(ckitagawa): Work with UX on padding/margins/style.
        mActionView = (Button) LayoutInflater.from(mContext).inflate(
                R.layout.tab_selection_editor_action_view, null);
        mActionView.setId(mMenuItem.getItemId());
        // Default visibility is GONE.
        mMenuItem.setActionView(mActionView);
    }

    public void setTitleResourceId(int titleResourceId) {
        mMenuItem.setTitle(titleResourceId);
        if (mActionView != null) {
            mActionView.setText(titleResourceId);
            mActionView.setVisibility(View.VISIBLE);
        }
    }

    /**
     * Builds a content description for a plural number of items. Defaults to the title otherwise.
     * @param contentDescriptionResourceId for the plural string to use or null to use the title.
     * @param itemCount the count of items selected.
     */
    public void setContentDescription(
            @Nullable Integer contentDescriptionResourceId, int itemCount) {
        String contentDescription = null;
        if (contentDescriptionResourceId != null && itemCount > 0) {
            contentDescription = mContext.getResources().getQuantityString(
                    contentDescriptionResourceId, itemCount, itemCount);
        }
        MenuItemCompat.setContentDescription(mMenuItem, contentDescription);
        if (mActionView != null) {
            mActionView.setContentDescription(contentDescription);
        }
    }

    /**
     * Sets the icon for the action view and menu item.
     * @param iconPosition for the action view.
     * @param icon to display in the menu item or action view.
     */
    public void setIcon(@IconPosition int iconPosition, Drawable icon) {
        mMenuItem.setIcon(icon);
        if (mActionView != null) {
            // TODO(ckitagawa): Determine whether to require actions to define bounds or just use
            // the intrinsic ones.
            TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(mActionView,
                    iconPosition == IconPosition.START ? icon : null, null,
                    iconPosition == IconPosition.END ? icon : null, null);
            mActionView.setVisibility(View.VISIBLE);
        }
    }

    public void setEnabled(boolean enabled) {
        mMenuItem.setEnabled(enabled);
        if (mActionView != null) {
            mActionView.setEnabled(enabled);
        }
    }

    public void setTextTint(ColorStateList colorStateList) {
        // MenuItem text color is handled by the theme.
        if (mActionView != null) {
            mActionView.setTextColor(colorStateList);
        }
    }

    public void setIconTint(@Nullable ColorStateList colorStateList) {
        // A null colorStateList is used with TabSelectionEditorActionProperties.SKIP_ICON_TINT
        // = true to signal that a custom tint is used. Ignore null so that this custom tint is
        // not overridden.
        if (colorStateList == null) return;

        MenuItemCompat.setIconTintList(mMenuItem, colorStateList);
        if (mActionView != null) {
            TextViewCompat.setCompoundDrawableTintList(mActionView, colorStateList);
        }
    }

    public void setOnClickListener(Runnable runnable) {
        mOnClickRunnable = runnable;
        if (mActionView != null) {
            mActionView.setOnClickListener(v -> onClick());
        }
    }

    public void setOnSelectionStateChange(Callback<List<Integer>> callback) {
        mOnSelectionStateChange = callback;
    }

    /**
     * Handler for click events on the menu item or action view.
     */
    public void onClick() {
        mOnClickRunnable.run();
    }

    /**
     * Updates the {@link TabSelectionEditorAction} with the currently selected tabs.
     */
    public void onSelectionStateChange(List<Integer> tabIds) {
        mOnSelectionStateChange.onResult(tabIds);
    }

    private static int getShowAsAction(@ShowMode int showMode) {
        switch (showMode) {
            case ShowMode.MENU_ONLY:
                return MenuItem.SHOW_AS_ACTION_NEVER;
            case ShowMode.IF_ROOM:
                return MenuItem.SHOW_AS_ACTION_IF_ROOM;
            default:
                assert false;
                return MenuItem.SHOW_AS_ACTION_NEVER;
        }
    }
}
