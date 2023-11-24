// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.content.res.ColorStateList;
import android.graphics.Color;
import android.graphics.PorterDuff;
import android.text.TextWatcher;
import android.util.AttributeSet;
import android.view.Gravity;
import android.view.View;
import android.view.ViewGroup;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.LinearLayout;

import androidx.annotation.ColorInt;
import androidx.annotation.ColorRes;
import androidx.core.content.ContextCompat;
import androidx.core.graphics.drawable.DrawableCompat;
import androidx.core.widget.ImageViewCompat;

import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.styles.SemanticColorUtils;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.widget.ChromeImageView;

/**
 * Represents a generic toolbar used in the bottom strip/grid component.
 * {@link TabGridPanelToolbarCoordinator}
 */
public class TabGroupUiToolbarView extends FrameLayout {
    private ChromeImageView mRightButton;
    private ChromeImageView mLeftButton;
    private ChromeImageView mMenuButton;
    private ChromeImageView mFadingEdgeStart;
    private ChromeImageView mFadingEdgeEnd;
    private ViewGroup mContainerView;
    private EditText mTitleTextView;
    private LinearLayout mMainContent;

    public TabGroupUiToolbarView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mLeftButton = findViewById(R.id.toolbar_left_button);
        mRightButton = findViewById(R.id.toolbar_right_button);
        mMenuButton = findViewById(R.id.toolbar_menu_button);
        mFadingEdgeStart = findViewById(R.id.tab_strip_fading_edge_start);
        mFadingEdgeEnd = findViewById(R.id.tab_strip_fading_edge_end);
        mContainerView = (ViewGroup) findViewById(R.id.toolbar_container_view);
        mTitleTextView = (EditText) findViewById(R.id.title);
        mMainContent = findViewById(R.id.main_content);
    }

    void setLeftButtonOnClickListener(OnClickListener listener) {
        mLeftButton.setOnClickListener(listener);
    }

    void setRightButtonOnClickListener(OnClickListener listener) {
        mRightButton.setOnClickListener(listener);
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
            // TODO(crbug.com/1116644) Figure out why a call to show keyboard without delay still
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

    ViewGroup getViewContainer() {
        return mContainerView;
    }

    void setMainContentVisibility(boolean isVisible) {
        if (mContainerView == null) {
            throw new IllegalStateException("Current Toolbar doesn't have a container view");
        }

        for (int i = 0; i < ((ViewGroup) mContainerView).getChildCount(); i++) {
            View child = ((ViewGroup) mContainerView).getChildAt(i);
            child.setVisibility(isVisible ? View.VISIBLE : View.INVISIBLE);
        }
    }

    void setTitle(String title) {
        if (mTitleTextView == null) {
            throw new IllegalStateException("Current Toolbar doesn't have a title text view");
        }
        mTitleTextView.setText(title);
    }

    void setIsIncognito(boolean isIncognito) {
        @ColorInt
        int primaryColor =
                isIncognito
                        ? getResources().getColor(R.color.dialog_bg_color_dark_baseline)
                        : SemanticColorUtils.getDialogBgColor(getContext());
        setPrimaryColor(primaryColor);

        @ColorRes
        int tintListRes =
                isIncognito
                        ? R.color.default_icon_color_light_tint_list
                        : R.color.default_icon_color_tint_list;
        ColorStateList tintList = ContextCompat.getColorStateList(getContext(), tintListRes);
        setTint(tintList);
    }

    void setPrimaryColor(int color) {
        mMainContent.setBackgroundColor(color);
        if (mFadingEdgeStart == null || mFadingEdgeEnd == null) return;
        mFadingEdgeStart.setColorFilter(color, PorterDuff.Mode.SRC_IN);
        mFadingEdgeEnd.setColorFilter(color, PorterDuff.Mode.SRC_IN);
    }

    void setTint(ColorStateList tint) {
        ImageViewCompat.setImageTintList(mLeftButton, tint);
        ImageViewCompat.setImageTintList(mRightButton, tint);
        if (mTitleTextView != null) mTitleTextView.setTextColor(tint);
        if (mMenuButton != null) {
            ImageViewCompat.setImageTintList(mMenuButton, tint);
        }
    }

    void setBackgroundColorTint(int color) {
        DrawableCompat.setTint(getBackground(), color);
    }

    /** Setup the toolbar layout for TabGridDialog. */
    void setupDialogToolbarLayout() {
        Context context = getContext();
        mLeftButton.setImageResource(R.drawable.ic_arrow_back_24dp);
        int topicMargin =
                (int) context.getResources().getDimension(R.dimen.tab_group_toolbar_topic_margin);
        MarginLayoutParams params = (MarginLayoutParams) mTitleTextView.getLayoutParams();
        params.setMarginStart(topicMargin);
        mTitleTextView.setGravity(Gravity.START | Gravity.CENTER_VERTICAL);
        mTitleTextView.setTextAppearance(R.style.TextAppearance_Headline_Primary);
    }

    /** Hide the title widgets related to tab group continuation features. */
    void hideTitleWidget() {
        mTitleTextView.setFocusable(false);
        mTitleTextView.setBackgroundColor(Color.TRANSPARENT);
    }

    /** Hide the menu button related to tab group continuation and selection editor features. */
    void hideMenuButton() {
        mMainContent.removeView(mMenuButton);
    }

    /** Setup the drawable in the left button. */
    void setLeftButtonDrawableId(int drawableId) {
        mLeftButton.setImageResource(drawableId);
    }

    /** Set the content description of the left button. */
    void setLeftButtonContentDescription(String string) {
        mLeftButton.setContentDescription(string);
    }

    /** Set the content description of the right button. */
    void setRightButtonContentDescription(String string) {
        mRightButton.setContentDescription(string);
    }
}
