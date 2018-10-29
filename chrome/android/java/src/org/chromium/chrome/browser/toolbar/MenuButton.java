// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.toolbar;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.drawable.Drawable;
import android.support.v7.widget.AppCompatImageButton;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;
import android.widget.ImageView;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.omaha.UpdateMenuItemHelper;

/**
 * The overflow menu button.
 */
class MenuButton extends FrameLayout {
    /** The {@link android.support.v7.widget.AppCompatImageButton} for the menu button. */
    private AppCompatImageButton mMenuImageButton;

    /** The view for the update badge. */
    private ImageView mUpdateBadgeView;
    private boolean mUseLightDrawables;

    public MenuButton(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();
        mMenuImageButton = findViewById(R.id.menu_button);
        mUpdateBadgeView = findViewById(R.id.menu_badge);
    }

    /**
     * @param onTouchListener An {@link OnTouchListener} that is triggered when the menu button is
     *                        clicked.
     */
    void setTouchListener(OnTouchListener onTouchListener) {
        mMenuImageButton.setOnTouchListener(onTouchListener);
    }

    @Override
    public void setAccessibilityDelegate(AccessibilityDelegate delegate) {
        mMenuImageButton.setAccessibilityDelegate(delegate);
    }

    /**
     * Sets the update badge to visible if the update state requires it.
     *
     * Also updates the visuals to the correct type for the case where {@link
     * #setUseLightDrawables(boolean)} was invoked before the update state was known. This is the
     * case on startup when the bottom toolbar is in use.
     *
     * @param visible Whether the update badge should be visible. Always sets visibility to GONE
     *                if the update type does not require a badge.
     * TODO(crbug.com/865801): Clean this up when MenuButton and UpdateMenuItemHelper is MVCed.
     */
    void setUpdateBadgeVisibilityIfValidState(boolean visible) {
        switch (UpdateMenuItemHelper.getInstance().getUpdateType()) {
            case UpdateMenuItemHelper.UpdateType.UPDATE_AVAILABLE:
            // Intentional fall through.
            case UpdateMenuItemHelper.UpdateType.UNSUPPORTED_OS_VERSION:
                mUpdateBadgeView.setVisibility(visible ? View.VISIBLE : View.GONE);
                updateImageResources();
                break;
            case UpdateMenuItemHelper.UpdateType.NONE:
            // Intentional fall through.
            case UpdateMenuItemHelper.UpdateType.UNKNOWN:
            // Intentional fall through.
            default:
                mUpdateBadgeView.setVisibility(View.GONE);
                break;
        }
    }

    /**
     * Sets the visual type of update badge to use (if any).
     * @param useLightDrawables Whether the light drawable should be used.
     */
    void setUseLightDrawables(boolean useLightDrawables) {
        mUseLightDrawables = useLightDrawables;
        updateImageResources();
    }

    void updateImageResources() {
        Drawable drawable;
        if (mUseLightDrawables) {
            drawable = UpdateMenuItemHelper.getInstance().getLightBadgeDrawable(
                    mUpdateBadgeView.getResources());
        } else {
            drawable = UpdateMenuItemHelper.getInstance().getDarkBadgeDrawable(
                    mUpdateBadgeView.getResources());
        }
        if (drawable == null) return;
        mUpdateBadgeView.setImageDrawable(drawable);
    }

    /**
     * @return Whether the update badge is showing.
     */
    boolean isShowingAppMenuUpdateBadge() {
        return mUpdateBadgeView.getVisibility() == View.VISIBLE;
    }

    void updateContentDescription() {
        if (isShowingAppMenuUpdateBadge()) {
            switch (UpdateMenuItemHelper.getInstance().getUpdateType()) {
                case UpdateMenuItemHelper.UpdateType.UPDATE_AVAILABLE:
                    setContentDescription(getResources().getString(
                            R.string.accessibility_toolbar_btn_menu_update));
                    break;
                case UpdateMenuItemHelper.UpdateType.UNSUPPORTED_OS_VERSION:
                    setContentDescription(getResources().getString(
                            R.string.accessibility_toolbar_btn_menu_os_version_unsupported));
                    break;
                case UpdateMenuItemHelper.UpdateType.NONE:
                // Intentional fall through.
                case UpdateMenuItemHelper.UpdateType.UNKNOWN:
                // Intentional fall through.
                default:
                    break;
            }
        } else {
            setContentDescription(
                    getResources().getString(R.string.accessibility_toolbar_btn_menu));
        }
    }

    AppCompatImageButton getImageButton() {
        return mMenuImageButton;
    }

    /**
     * @param tintList The {@link ColorStateList} that will tint the menu button (the badge is not
     *                 tinted).
     */
    void setTint(ColorStateList tintList) {
        ApiCompatibilityUtils.setImageTintList(mMenuImageButton, tintList);
    }
}
