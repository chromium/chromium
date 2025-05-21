// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import androidx.annotation.StyleRes;
import androidx.appcompat.content.res.AppCompatResources;
import androidx.core.widget.TextViewCompat;

import org.chromium.base.Callback;
import org.chromium.build.annotations.MonotonicNonNull;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ButtonType;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.IconPosition;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorAction.ShowMode;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.util.motion.MotionEventInfo;
import org.chromium.components.browser_ui.widget.BrowserUiListMenuUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;

import java.util.List;

/**
 * Holds the {@code mActionView} and {@link ListItem} for an item in the {@link TabListEditorMenu}.
 */
@NullMarked
public class TabListEditorMenuItem {

    /** Runs when a menu item is clicked. */
    public interface OnClickRunnable {

        /**
         * Called when a menu item is clicked.
         *
         * @param triggeringMotion the {@link MotionEventInfo} that triggered the click; it is
         *     {@code null} if {@link android.view.MotionEvent} wasn't available when the click was
         *     detected, such as in {@link android.view.View.OnClickListener}.
         */
        void run(@Nullable MotionEventInfo triggeringMotion);
    }

    private final Context mContext;

    private final ListItem mListItem;
    private @Nullable Button mActionView;
    private boolean mShowText;
    private boolean mShowIcon;
    private boolean mEnabled;
    private boolean mShouldDismissMenu;
    private boolean mActionViewShowing;
    private @Nullable ColorStateList mIconTint;

    private @MonotonicNonNull OnClickRunnable mOnClickRunnable;
    private @Nullable Callback<List<TabListEditorItemSelectionId>> mOnSelectionStateChange;

    /**
     * @param context for loading resources.
     */
    TabListEditorMenuItem(Context context, ListItem listItem) {
        mContext = context;
        mListItem = listItem;
    }

    /**
     * Initializes the {@code mActionView} if applicable.
     * @param showMode whether to show the action view.
     * @param buttonType the button layout of the action view.
     */
    public void initActionView(@ShowMode int showMode, @ButtonType int buttonType) {
        mShowText = buttonType == ButtonType.TEXT || buttonType == ButtonType.ICON_AND_TEXT;
        mShowIcon = buttonType == ButtonType.ICON || buttonType == ButtonType.ICON_AND_TEXT;

        if ((!mShowText && !mShowIcon) || showMode == ShowMode.MENU_ONLY) return;

        mActionView =
                (Button)
                        LayoutInflater.from(mContext)
                                .inflate(R.layout.tab_list_editor_action_view, null);
        mActionView.setId(mListItem.model.get(TabListEditorActionProperties.MENU_ITEM_ID));
        if (mShowIcon && !mShowText) {
            mActionView.setCompoundDrawablePadding(0);
        }
    }

    public @Nullable View getActionView() {
        return mActionView;
    }

    public ListItem getListItem() {
        return mListItem;
    }

    /**
     * Set the title in the menu and ActionView.
     * @param titleResourceId Resource ID of the title.
     * @param itemCount current item count. -1 means the title is not plural.
     */
    public void setTitle(int titleResourceId, int itemCount) {
        String title;
        if (itemCount >= 0) {
            title =
                    mContext.getResources()
                            .getQuantityString(titleResourceId, itemCount, itemCount);
        } else {
            title = mContext.getString(titleResourceId);
        }
        mListItem.model.set(TabListEditorActionProperties.TITLE, title);
        if (mActionView != null) {
            if (mShowText) {
                mActionView.setText(title);
            } else {
                mActionView.setText("");
                mActionView.setMinWidth(0);
                mActionView.setMinimumWidth(0);
            }
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
            contentDescription =
                    mContext.getResources()
                            .getQuantityString(contentDescriptionResourceId, itemCount, itemCount);
        }
        mListItem.model.set(TabListEditorActionProperties.CONTENT_DESCRIPTION, contentDescription);
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
        mListItem.model.set(TabListEditorActionProperties.ICON, icon);
        if (mActionView != null && mShowIcon) {
            TextViewCompat.setCompoundDrawablesRelativeWithIntrinsicBounds(
                    mActionView,
                    iconPosition == IconPosition.START ? icon : null,
                    null,
                    iconPosition == IconPosition.END ? icon : null,
                    null);
        }
    }

    public void setEnabled(boolean enabled) {
        mEnabled = enabled;
        mListItem.model.set(TabListEditorActionProperties.ENABLED, enabled);
        if (mActionView != null) {
            mActionView.setEnabled(enabled);
        }
    }

    public void setTextAppearance(@StyleRes int textAppearanceId) {
        if (mActionView != null) {
            mActionView.setTextAppearance(textAppearanceId);
        }
    }

    public void setTextTint(ColorStateList colorStateList) {
        // mListItem uses the default text tint.
        if (mActionView != null) {
            mActionView.setTextColor(colorStateList);
        }
    }

    public void setIconTint(@Nullable ColorStateList colorStateList) {
        // mListItem uses the default icon tint whenever shown. Cache the tint to restore it when
        // the action view shown state is toggled.
        mListItem.model.set(
                TabListEditorActionProperties.ICON_TINT,
                AppCompatResources.getColorStateList(
                        mContext, BrowserUiListMenuUtils.getDefaultIconTintColorStateListId()));
        mIconTint = colorStateList;
        if (mActionView != null && mActionViewShowing) {
            TextViewCompat.setCompoundDrawableTintList(mActionView, colorStateList);
        }
    }

    public void setActionViewShowing(boolean actionViewShowing) {
        mActionViewShowing = actionViewShowing;
        if (mActionViewShowing) {
            // Ensure the drawable has the correct tint.
            setIconTint(mIconTint);
        }
    }

    public void setOnClickListener(OnClickRunnable runnable) {
        mOnClickRunnable = runnable;
        if (mActionView != null) {
            // TODO(crbug.com/419085605): Also attach an OnPeripheralClickListener and use it to
            // pass triggeringMotion.
            mActionView.setOnClickListener(v -> onClick(/* triggeringMotion= */ null));
        }
    }

    public void setShouldDismissMenu(boolean shouldDismissMenu) {
        mShouldDismissMenu = shouldDismissMenu;
    }

    public boolean shouldDismissMenu() {
        return mShouldDismissMenu;
    }

    public void setOnSelectionStateChange(Callback<List<TabListEditorItemSelectionId>> callback) {
        mOnSelectionStateChange = callback;
    }

    /**
     * Handler for click events on the menu item or action view.
     *
     * @param triggeringMotion see {@link OnClickRunnable#run(MotionEventInfo)}.
     */
    public boolean onClick(@Nullable MotionEventInfo triggeringMotion) {
        if (!mEnabled) return false;

        assumeNonNull(mOnClickRunnable).run(triggeringMotion);

        return true;
    }

    /** Updates the {@link TabListEditorAction} with the currently selected tabs. */
    public void onSelectionStateChange(List<TabListEditorItemSelectionId> itemIds) {
        assumeNonNull(mOnSelectionStateChange).onResult(itemIds);
    }
}
