// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.modaldialog;

import android.content.Context;
import android.support.annotation.IntDef;
import android.support.annotation.NonNull;
import android.support.annotation.StringRes;
import android.text.TextUtils;
import android.view.ContextThemeWrapper;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.LinearLayout.LayoutParams;
import android.widget.TextView;

import org.chromium.base.ContextUtils;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.widget.FadingEdgeScrollView;
import org.chromium.ui.UiUtils;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Generic builder for app modal or tab modal alert dialogs.
 */
public class ModalDialogView implements View.OnClickListener {
    /**
     * Interface that controls the actions on the modal dialog.
     */
    public interface Controller {
        /**
         * Handle click event of the buttons on the dialog.
         * @param buttonType The type of the button.
         */
        void onClick(@ButtonType int buttonType);

        /**
         * Handle dismiss event when the dialog is dismissed by actions on the dialog. Note that it
         * can be dangerous to the {@code dismissalCause} for business logic other than metrics
         * recording, unless the dismissal cause is fully controlled by the client (e.g. button
         * clicked), because the dismissal cause can be different values depending on modal dialog
         * type and mode of presentation (e.g. it could be unknown on VR but a specific value on
         * non-VR).
         * @param dismissalCause The reason of the dialog being dismissed.
         * @see DialogDismissalCause
         */
        void onDismiss(@DialogDismissalCause int dismissalCause);
    }

    /** Parameters that can be used to create a new ModalDialogView. */
    public static class Params {
        /** Optional: The String to show as the dialog title. */
        public String title;

        /** Optional: The String to show as descriptive text. */
        public String message;

        /**
         * Optional: The customized View to show in the dialog. Note that the message and the
         * custom view cannot be set together.
         */
        public View customView;

        /** Optional: Resource ID of the String to show on the positive button. */
        public @StringRes int positiveButtonTextId;

        /** Optional: Resource ID of the String to show on the negative button. */
        public @StringRes int negativeButtonTextId;

        /**
         * Optional: The String to show on the positive button. Note that String
         * must be null if positiveButtonTextId is not zero.
         */
        public String positiveButtonText;

        /**
         * Optional: The String to show on the negative button.  Note that String
         * must be null if negativeButtonTextId is not zero
         */
        public String negativeButtonText;

        /**
         * Optional: If true the dialog gets cancelled when the user touches outside of the dialog.
         */
        public boolean cancelOnTouchOutside;

        /**
         * Optional: If true, the dialog title is scrollable with the message. Note that the
         * {@link #customView} will have height WRAP_CONTENT if this is set to true.
         */
        public boolean titleScrollable;
    }

    @IntDef({ButtonType.POSITIVE, ButtonType.NEGATIVE})
    @Retention(RetentionPolicy.SOURCE)
    public @interface ButtonType {
        int POSITIVE = 0;
        int NEGATIVE = 1;
    }

    private final Controller mController;
    private final Params mParams;

    private final View mDialogView;
    private final TextView mTitleView;
    private final TextView mMessageView;
    private final ViewGroup mCustomView;
    private final Button mPositiveButton;
    private final Button mNegativeButton;

    /**
     * @return The {@link Context} with the modal dialog theme set.
     */
    public static Context getContext() {
        return new ContextThemeWrapper(
                ContextUtils.getApplicationContext(), R.style.ModalDialogTheme);
    }

    /**
     * Constructor for initializing controller and views.
     * @param controller The controller for this dialog.
     */
    public ModalDialogView(@NonNull Controller controller, @NonNull Params params) {
        mController = controller;
        mParams = params;

        mDialogView = LayoutInflater.from(getContext()).inflate(R.layout.modal_dialog_view, null);
        mTitleView = mDialogView.findViewById(
                mParams.titleScrollable ? R.id.scrollable_title : R.id.title);
        mMessageView = mDialogView.findViewById(R.id.message);
        mCustomView = mDialogView.findViewById(R.id.custom);
        mPositiveButton = mDialogView.findViewById(R.id.positive_button);
        mNegativeButton = mDialogView.findViewById(R.id.negative_button);
    }

    @Override
    public void onClick(View view) {
        if (view == mPositiveButton) {
            mController.onClick(ButtonType.POSITIVE);
        } else if (view == mNegativeButton) {
            mController.onClick(ButtonType.NEGATIVE);
        }
    }

    /**
     * Prepare the contents before showing the dialog.
     */
    protected void prepareBeforeShow() {
        FadingEdgeScrollView scrollView = mDialogView.findViewById(R.id.modal_dialog_scroll_view);

        if (!TextUtils.isEmpty(mParams.title)) {
            mTitleView.setText(mParams.title);
            mTitleView.setVisibility(View.VISIBLE);
        }

        if (TextUtils.isEmpty(mParams.message)) {
            if (mParams.titleScrollable && mTitleView.getVisibility() != View.GONE) {
                mMessageView.setVisibility(View.GONE);
            } else {
                scrollView.setVisibility(View.GONE);
            }
        } else {
            assert mParams.titleScrollable || mParams.customView == null;
            mMessageView.setText(mParams.message);
        }

        if (mParams.customView != null) {
            UiUtils.removeViewFromParent(mParams.customView);
            mCustomView.addView(mParams.customView);
        } else {
            mCustomView.setVisibility(View.GONE);
        }

        assert(mParams.positiveButtonTextId == 0 || mParams.positiveButtonText == null);
        if (mParams.positiveButtonTextId != 0) {
            mPositiveButton.setText(mParams.positiveButtonTextId);
            mPositiveButton.setOnClickListener(this);
        } else if (mParams.positiveButtonText != null) {
            mPositiveButton.setText(mParams.positiveButtonText);
            mPositiveButton.setOnClickListener(this);
        } else {
            mPositiveButton.setVisibility(View.GONE);
        }

        assert(mParams.negativeButtonTextId == 0 || mParams.negativeButtonText == null);
        if (mParams.negativeButtonTextId != 0) {
            mNegativeButton.setText(mParams.negativeButtonTextId);
            mNegativeButton.setOnClickListener(this);
        } else if (mParams.negativeButtonText != null) {
            mNegativeButton.setText(mParams.negativeButtonText);
            mNegativeButton.setOnClickListener(this);
        } else {
            mNegativeButton.setVisibility(View.GONE);
        }

        if (mParams.titleScrollable) {
            LayoutParams layoutParams = (LayoutParams) mCustomView.getLayoutParams();
            layoutParams.height = LayoutParams.WRAP_CONTENT;
            layoutParams.weight = 0;
            mCustomView.setLayoutParams(layoutParams);
        } else {
            scrollView.setEdgeVisibility(
                    FadingEdgeScrollView.EdgeType.NONE, FadingEdgeScrollView.EdgeType.NONE);
        }
    }

    /**
     * @return The content view of this dialog.
     */
    public View getView() {
        return mDialogView;
    }

    /**
     * @return The button that was added to the dialog using {@link Params}.
     * @param button indicates which button should be returned.
     */
    public Button getButton(@ButtonType int button) {
        if (button == ButtonType.POSITIVE) {
            return mPositiveButton;
        } else if (button == ButtonType.NEGATIVE) {
            return mNegativeButton;
        }
        assert false;
        return null;
    }

    /**
     * @return The controller that controls the actions on the dialogs.
     */
    public Controller getController() {
        return mController;
    }

    /**
     * @return The content description of the dialog view.
     */
    public String getContentDescription() {
        return mParams.title;
    }

    /**
     * TODO(huayinz): Should we consider adding a model change processor now that the params are
     * mutable
     *
     * @param title Updates the title string to the new title.
     */
    public void setTitle(String title) {
        mTitleView.setText(title);
    }

    /**
     * @return Returns true if the dialog is dismissed when the user touches outside of the dialog.
     */
    public boolean getCancelOnTouchOutside() {
        return mParams.cancelOnTouchOutside;
    }
}
