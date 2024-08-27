// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.res.ColorStateList;
import android.content.res.Resources;
import android.graphics.Rect;
import android.graphics.drawable.GradientDrawable;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.TouchDelegate;
import android.view.View;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageView;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.annotation.Nullable;
import androidx.annotation.StringRes;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.tab_groups.TabGroupColorId;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.widget.ButtonCompat;
import org.chromium.ui.widget.ChromeImageView;

/** Toolbar used in the tab grid dialog see {@link TabGridDialogCoordinator}. */
public class TabGridDialogToolbarView extends FrameLayout {
    private ChromeImageView mNewTabButton;
    private ChromeImageView mBackButton;
    private ChromeImageView mMenuButton;
    private EditText mTitleTextView;
    private LinearLayout mMainContent;
    private FrameLayout mColorIconContainer;
    private ImageView mColorIcon;
    private @Nullable FrameLayout mShareButtonContainer;
    private @Nullable ButtonCompat mShareButton;
    private @Nullable FrameLayout mImageTilesContainer;

    public TabGridDialogToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mBackButton = findViewById(R.id.toolbar_back_button);
        mNewTabButton = findViewById(R.id.toolbar_new_tab_button);
        mMenuButton = findViewById(R.id.toolbar_menu_button);
        mTitleTextView = (EditText) findViewById(R.id.title);
        mMainContent = findViewById(R.id.main_content);
        mColorIconContainer = findViewById(R.id.tab_group_color_icon_container);
        mColorIcon = findViewById(R.id.tab_group_color_icon);
        mShareButtonContainer = findViewById(R.id.share_button_container);
        mShareButton = findViewById(R.id.share_button);
        mImageTilesContainer = findViewById(R.id.image_tiles_container);
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent motionEvent) {
        super.onTouchEvent(motionEvent);
        // Prevent touch events from "falling through" to views below.
        return true;
    }

    void setBackButtonOnClickListener(OnClickListener listener) {
        mBackButton.setOnClickListener(listener);
    }

    void setNewTabButtonOnClickListener(OnClickListener listener) {
        mNewTabButton.setOnClickListener(listener);
    }

    void setMenuButtonOnClickListener(OnClickListener listener) {
        mMenuButton.setOnClickListener(listener);
    }

    void setTitleTextOnChangedListener(TextWatcher textWatcher) {
        mTitleTextView.addTextChangedListener(textWatcher);
    }

    void setTitleTextOnFocusChangeListener(OnFocusChangeListener listener) {
        mTitleTextView.setOnFocusChangeListener(listener);
    }

    void setTitleCursorVisibility(boolean isVisible) {
        mTitleTextView.setCursorVisible(isVisible);
    }

    void updateTitleTextFocus(boolean shouldFocus) {
        if (mTitleTextView.isFocused() == shouldFocus) return;
        if (shouldFocus) {
            mTitleTextView.requestFocus();
        } else {
            clearTitleTextFocus();
        }
    }

    void updateKeyboardVisibility(boolean shouldShow) {
        // This is equal to the animation duration of toolbar menu hiding.
        int showKeyboardDelay = 150;
        if (shouldShow) {
            // TODO(crbug.com/40144823) Figure out why a call to show keyboard without delay still
            // won't work when the window gets focus in onWindowFocusChanged call.
            // Wait until the current window has focus to show the keyboard. This is to deal with
            // the case where the keyboard showing is caused by toolbar menu. In this case, we need
            // to wait for the menu window to hide and current window to gain focus so that we can
            // show the keyboard.
            KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
            postDelayed(
                    new Runnable() {
                        @Override
                        public void run() {
                            assert hasWindowFocus();
                            delegate.showKeyboard(mTitleTextView);
                        }
                    },
                    showKeyboardDelay);
        } else {
            hideKeyboard();
        }
    }

    void clearTitleTextFocus() {
        mTitleTextView.clearFocus();
    }

    void hideKeyboard() {
        KeyboardVisibilityDelegate delegate = KeyboardVisibilityDelegate.getInstance();
        delegate.hideKeyboard(this);
    }

    void setTitle(String title) {
        mTitleTextView.setText(title);
    }

    void setIsIncognito(boolean isIncognito) {
        @ColorRes
        int tintListRes =
                isIncognito
                        ? R.color.default_icon_color_light_tint_list
                        : R.color.default_icon_color_tint_list;
        ColorStateList tintList = ContextCompat.getColorStateList(getContext(), tintListRes);
        setTint(tintList);
    }

    void setContentBackgroundColor(int color) {
        mMainContent.setBackgroundColor(color);
    }

    void setTint(ColorStateList tint) {
        ImageViewCompat.setImageTintList(mBackButton, tint);
        ImageViewCompat.setImageTintList(mNewTabButton, tint);
        if (mTitleTextView != null) mTitleTextView.setTextColor(tint);
        if (mMenuButton != null) {
            ImageViewCompat.setImageTintList(mMenuButton, tint);
        }
    }

    void setBackgroundColorTint(int color) {
        DrawableCompat.setTint(getBackground(), color);
    }

    /** Setup the drawable in the left button. */
    void setBackButtonDrawableId(int drawableId) {
        mBackButton.setImageResource(drawableId);
    }

    /** Set the content description of the left button. */
    void setBackButtonContentDescription(String string) {
        mBackButton.setContentDescription(string);
    }

    /** Set the content description of the right button. */
    void setNewTabButtonContentDescription(String string) {
        mNewTabButton.setContentDescription(string);
    }

    void setImageTilesVisibility(boolean isVisible) {
        if (mImageTilesContainer == null) return;

        mImageTilesContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
    }

    void setShareButtonVisibility(boolean isVisible) {
        if (mShareButtonContainer == null || mShareButton == null) return;
        mShareButtonContainer.setVisibility(isVisible ? View.VISIBLE : View.GONE);
        if (isVisible) {
            // Post this so that getHitRect() returns the correct size as by
            // default the view is GONE.
            mShareButtonContainer.post(
                    () -> {
                        Rect rect = new Rect();
                        mShareButton.getHitRect(rect);
                        Resources res = mShareButton.getContext().getResources();
                        int delta =
                                res.getDimensionPixelSize(R.dimen.min_touch_target_size)
                                        - rect.height();
                        if (delta > 0) {
                            int halfDelta = Math.round(delta / 2.0f);
                            rect.top -= halfDelta;
                            rect.bottom += halfDelta;
                        }
                        mShareButtonContainer.setTouchDelegate(
                                new TouchDelegate(rect, mShareButton));
                    });
        }
    }

    void setShareButtonClickListener(OnClickListener listener) {
        if (mShareButton == null) return;
        mShareButton.setOnClickListener(listener);
    }

    void setImageTilesClickListener(OnClickListener listener) {
        if (mImageTilesContainer == null) return;
        mImageTilesContainer.setOnClickListener(listener);
    }

    /** Set the color icon of type {@link TabGroupColorId} on the tab group card view. */
    void setColorIconColor(@TabGroupColorId int colorId, boolean isIncognito) {
        if (ChromeFeatureList.sTabGroupParityAndroid.isEnabled()) {
            mColorIconContainer.setVisibility(View.VISIBLE);

            final @ColorInt int color =
                    ColorPickerUtils.getTabGroupColorPickerItemColor(
                            getContext(), colorId, isIncognito);

            GradientDrawable gradientDrawable = (GradientDrawable) mColorIcon.getBackground();
            gradientDrawable.setColor(color);

            // Set accessibility content for the color icon.
            Resources res = getContext().getResources();
            final @StringRes int colorDescRes =
                    ColorPickerUtils.getTabGroupColorPickerItemColorAccessibilityString(colorId);
            String colorDesc = res.getString(colorDescRes);
            String contentDescription =
                    res.getString(
                            R.string.accessibility_tab_group_color_icon_description, colorDesc);
            mColorIconContainer.setContentDescription(contentDescription);
        } else {
            mColorIconContainer.setVisibility(View.GONE);
        }
    }

    void setColorIconOnClickListener(OnClickListener listener) {
        mColorIconContainer.setOnClickListener(listener);
    }
}
