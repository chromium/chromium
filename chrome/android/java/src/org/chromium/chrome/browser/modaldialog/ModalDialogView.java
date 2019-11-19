// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.content.Context;
import android.graphics.drawable.Drawable;
import android.os.Build;
import android.text.TextUtils;
import android.util.AttributeSet;
import android.view.MotionEvent;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ImageView;
import android.widget.TextView;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ui.widget.BoundedLinearLayout;
import org.chromium.chrome.browser.ui.widget.FadingEdgeScrollView;
import org.chromium.ui.UiUtils;
import org.chromium.ui.modaldialog.ModalDialogProperties;

import java.lang.reflect.Field;

/**
 * Generic dialog view for app modal or tab modal alert dialogs.
 */
public class ModalDialogView extends BoundedLinearLayout implements View.OnClickListener {
    private static final String TAG = "ModalDialogView";

    private ModalDialogProperties.Controller mController;

    private FadingEdgeScrollView mScrollView;
    private ViewGroup mTitleContainer;
    private TextView mTitleView;
    private ImageView mTitleIcon;
    private TextView mMessageView;
    private ViewGroup mCustomViewContainer;
    private View mButtonBar;
    private Button mPositiveButton;
    private Button mNegativeButton;
    private Callback<Integer> mOnButtonClickedCallback;
    private boolean mTitleScrollable;
    private boolean mFilterTouchForSecurity;

    /**
     * Constructor for inflating from XML.
     */
    public ModalDialogView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    protected void onFinishInflate() {
        super.onFinishInflate();

        mScrollView = findViewById(R.id.modal_dialog_scroll_view);
        mTitleContainer = findViewById(R.id.title_container);
        mTitleView = mTitleContainer.findViewById(R.id.title);
        mTitleIcon = mTitleContainer.findViewById(R.id.title_icon);
        mMessageView = findViewById(R.id.message);
        mCustomViewContainer = findViewById(R.id.custom);
        mButtonBar = findViewById(R.id.button_bar);
        mPositiveButton = findViewById(R.id.positive_button);
        mNegativeButton = findViewById(R.id.negative_button);

        mPositiveButton.setOnClickListener(this);
        mNegativeButton.setOnClickListener(this);
        updateContentVisibility();
        updateButtonVisibility();

        // If the scroll view can not be scrolled, make the scroll view not focusable so that the
        // focusing behavior for hardware keyboard is less confusing.
        // See https://codereview.chromium.org/2939883002.
        mScrollView.addOnLayoutChangeListener(
                (v, left, top, right, bottom, oldLeft, oldTop, oldRight, oldBottom) -> {
                    boolean isScrollable = v.canScrollVertically(-1) || v.canScrollVertically(1);
                    v.setFocusable(isScrollable);
                });
    }

    // View.OnClickListener implementation.

    @Override
    public void onClick(View view) {
        if (view == mPositiveButton) {
            mOnButtonClickedCallback.onResult(ModalDialogProperties.ButtonType.POSITIVE);
        } else if (view == mNegativeButton) {
            mOnButtonClickedCallback.onResult(ModalDialogProperties.ButtonType.NEGATIVE);
        }
    }

    /**
     * @return The controller that controls the actions on the dialogs.
     */
    public ModalDialogProperties.Controller getController() {
        return mController;
    }

    /**
     * @param controller The {@link ModalDialogProperties.Controller} that handles events on user
     *         actions.
     */
    void setController(ModalDialogProperties.Controller controller) {
        mController = controller;
    }

    /**
     * @param callback The {@link Callback<Integer>} when a button on the dialog button bar is
     *                 clicked. The {@link Integer} indicates the button type.
     */
    void setOnButtonClickedCallback(Callback<Integer> callback) {
        mOnButtonClickedCallback = callback;
    }

    /** @param title The title of the dialog. */
    public void setTitle(CharSequence title) {
        mTitleView.setText(title);
        updateContentVisibility();
    }

    /**
     * @param drawable The icon drawable on the title.
     */
    public void setTitleIcon(Drawable drawable) {
        mTitleIcon.setImageDrawable(drawable);
        updateContentVisibility();
    }

    /** @param titleScrollable Whether the title is scrollable with the message. */
    void setTitleScrollable(boolean titleScrollable) {
        if (mTitleScrollable == titleScrollable) return;

        mTitleScrollable = titleScrollable;
        CharSequence title = mTitleView.getText();
        Drawable icon = mTitleIcon.getDrawable();

        // Hide the previous title container since the scrollable and non-scrollable title container
        // should not be shown at the same time.
        mTitleContainer.setVisibility(View.GONE);

        mTitleContainer = findViewById(
                titleScrollable ? R.id.scrollable_title_container : R.id.title_container);
        mTitleView = mTitleContainer.findViewById(R.id.title);
        mTitleIcon = mTitleContainer.findViewById(R.id.title_icon);
        setTitle(title);
        setTitleIcon(icon);

        LayoutParams layoutParams = (LayoutParams) mCustomViewContainer.getLayoutParams();
        if (titleScrollable) {
            layoutParams.height = LayoutParams.WRAP_CONTENT;
            layoutParams.weight = 0;
            mScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.FADING, FadingEdgeScrollView.EdgeType.FADING);
        } else {
            layoutParams.height = 0;
            layoutParams.weight = 1;
            mScrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.NONE, FadingEdgeScrollView.EdgeType.NONE);
        }
        mCustomViewContainer.setLayoutParams(layoutParams);
    }

    /**
     * @param filterTouchForSecurity Whether button touch events should be filtered when buttons are
     *                               obscured by another visible window.
     */
    void setFilterTouchForSecurity(boolean filterTouchForSecurity) {
        if (mFilterTouchForSecurity == filterTouchForSecurity) return;

        mFilterTouchForSecurity = filterTouchForSecurity;
        if (filterTouchForSecurity) {
            setupFilterTouchForSecurity();
        } else {
            assert false : "Shouldn't remove touch filter after setting it up";
        }
    }

    /** Setup touch filters to block events when buttons are obscured by another window. */
    private void setupFilterTouchForSecurity() {
        Button positiveButton = getButton(ModalDialogProperties.ButtonType.POSITIVE);
        Button negativeButton = getButton(ModalDialogProperties.ButtonType.NEGATIVE);
        View.OnTouchListener onTouchListener = (View v, MotionEvent ev) -> {
            // Filter touch events based MotionEvent.FLAG_WINDOW_IS_PARTIALLY_OBSCURED which is
            // introduced on M+.
            if (Build.VERSION.SDK_INT < Build.VERSION_CODES.M) return false;

            try {
                Field field = MotionEvent.class.getField("FLAG_WINDOW_IS_PARTIALLY_OBSCURED");
                if ((ev.getFlags() & field.getInt(null)) != 0) return true;
            } catch (NoSuchFieldException | IllegalAccessException e) {
                Log.e(TAG, "Reflection failure: " + e);
            }
            return false;
        };

        positiveButton.setFilterTouchesWhenObscured(true);
        positiveButton.setOnTouchListener(onTouchListener);
        negativeButton.setFilterTouchesWhenObscured(true);
        negativeButton.setOnTouchListener(onTouchListener);
    }

    /** @param message The message in the dialog content. */
    void setMessage(String message) {
        mMessageView.setText(message);
        updateContentVisibility();
    }

    /** @param view The customized view in the dialog content. */
    void setCustomView(View view) {
        if (mCustomViewContainer.getChildCount() > 0) mCustomViewContainer.removeAllViews();

        if (view != null) {
            UiUtils.removeViewFromParent(view);
            mCustomViewContainer.addView(view);
            mCustomViewContainer.setVisibility(View.VISIBLE);
        } else {
            mCustomViewContainer.setVisibility(View.GONE);
        }
    }

    /**
     * @param buttonType Indicates which button should be returned.
     */
    private Button getButton(@ModalDialogProperties.ButtonType int buttonType) {
        switch (buttonType) {
            case ModalDialogProperties.ButtonType.POSITIVE:
                return mPositiveButton;
            case ModalDialogProperties.ButtonType.NEGATIVE:
                return mNegativeButton;
            default:
                assert false;
                return null;
        }
    }

    /**
     * Sets button text for the specified button. If {@code buttonText} is empty or null, the
     * specified button will not be visible.
     * @param buttonType The {@link ModalDialogProperties.ButtonType} of the button.
     * @param buttonText The text to be set on the specified button.
     */
    void setButtonText(@ModalDialogProperties.ButtonType int buttonType, String buttonText) {
        getButton(buttonType).setText(buttonText);
        updateButtonVisibility();
    }

    /**
     * Sets content description for the specified button.
     * @param buttonType The {@link ModalDialogProperties.ButtonType} of the button.
     * @param contentDescription The content description to be set for the specified button.
     */
    void setButtonContentDescription(
            @ModalDialogProperties.ButtonType int buttonType, String contentDescription) {
        getButton(buttonType).setContentDescription(contentDescription);
    }

    /**
     * @param buttonType The {@link ModalDialogProperties.ButtonType} of the button.
     * @param enabled Whether the specified button should be enabled.
     */
    void setButtonEnabled(@ModalDialogProperties.ButtonType int buttonType, boolean enabled) {
        getButton(buttonType).setEnabled(enabled);
    }

    private void updateContentVisibility() {
        boolean titleVisible = !TextUtils.isEmpty(mTitleView.getText());
        boolean titleIconVisible = mTitleIcon.getDrawable() != null;
        boolean titleContainerVisible = titleVisible || titleIconVisible;
        boolean messageVisible = !TextUtils.isEmpty(mMessageView.getText());
        boolean scrollViewVisible = (mTitleScrollable && titleContainerVisible) || messageVisible;

        mTitleView.setVisibility(titleVisible ? View.VISIBLE : View.GONE);
        mTitleIcon.setVisibility(titleIconVisible ? View.VISIBLE : View.GONE);
        mTitleContainer.setVisibility(titleContainerVisible ? View.VISIBLE : View.GONE);
        mMessageView.setVisibility(messageVisible ? View.VISIBLE : View.GONE);
        mScrollView.setVisibility(scrollViewVisible ? View.VISIBLE : View.GONE);
    }

    private void updateButtonVisibility() {
        boolean positiveButtonVisible = !TextUtils.isEmpty(mPositiveButton.getText());
        boolean negativeButtonVisible = !TextUtils.isEmpty(mNegativeButton.getText());
        boolean buttonBarVisible = positiveButtonVisible || negativeButtonVisible;

        mPositiveButton.setVisibility(positiveButtonVisible ? View.VISIBLE : View.GONE);
        mNegativeButton.setVisibility(negativeButtonVisible ? View.VISIBLE : View.GONE);
        mButtonBar.setVisibility(buttonBarVisible ? View.VISIBLE : View.GONE);
    }
}
