// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.DETAILS;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION_VISIBLE;
import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.TITLE;

import android.content.res.Resources;
import android.graphics.Typeface;
import android.text.SpannableString;
import android.text.Spanned;
import android.text.style.StyleSpan;
import android.view.View;

import androidx.annotation.DrawableRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.fullscreen.ChromeFullscreenManager;
import org.chromium.chrome.browser.modaldialog.TabModalPresenter;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class responsible for the logic of showing the password manager dialog (e.g. onboarding
 * dialog).
 */
class PasswordManagerDialogMediator implements View.OnLayoutChangeListener {
    private final PropertyModel mModel;
    private final ModalDialogManager mDialogManager;
    private PropertyModel.Builder mModalDialogBuilder;
    private PropertyModel mDialogModel;
    private final View mAndroidContentView;
    private final Resources mResources;
    private final ChromeFullscreenManager mFullscreenManager;
    private final int mContainerHeightResource;

    private static class DialogClickHandler implements ModalDialogProperties.Controller {
        private Callback<Integer> mCallback;

        DialogClickHandler(Callback<Integer> onClick) {
            mCallback = onClick;
        }

        @Override
        public void onClick(PropertyModel model, int buttonType) {
            switch (buttonType) {
                case ModalDialogProperties.ButtonType.POSITIVE:
                    mCallback.onResult(DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                    break;
                case ModalDialogProperties.ButtonType.NEGATIVE:
                    mCallback.onResult(DialogDismissalCause.NEGATIVE_BUTTON_CLICKED);
                    break;
                default:
                    assert false : "Unexpected button pressed in dialog: " + buttonType;
            }
        }

        @Override
        public void onDismiss(PropertyModel model, @DialogDismissalCause int dismissalCause) {
            mCallback.onResult(dismissalCause);
        }
    }

    PasswordManagerDialogMediator(PropertyModel model, PropertyModel.Builder dialogBuilder,
            ModalDialogManager manager, View androidContentView, Resources resources,
            ChromeFullscreenManager fullscreenManager, int containerHeightResource) {
        mModel = model;
        mDialogManager = manager;
        mModalDialogBuilder = dialogBuilder;
        mAndroidContentView = androidContentView;
        mResources = resources;
        mFullscreenManager = fullscreenManager;
        mContainerHeightResource = containerHeightResource;
        mAndroidContentView.addOnLayoutChangeListener(this);
    }

    void setContents(String title, String details, int boldRangeStart, int boldRangeEnd,
            @DrawableRes int drawableId) {
        mModel.set(ILLUSTRATION, drawableId);
        mModel.set(TITLE, title);
        mModalDialogBuilder.with(ModalDialogProperties.CONTENT_DESCRIPTION, title);
        mModel.set(DETAILS, addBoldSpanToDetails(details, boldRangeStart, boldRangeEnd));
    }

    void setButtons(String positiveButtonText, String negativeButtonText, Callback<Integer> onClick,
            boolean primaryButtonFilled) {
        mModalDialogBuilder.with(ModalDialogProperties.CONTROLLER, new DialogClickHandler(onClick))
                .with(ModalDialogProperties.POSITIVE_BUTTON_TEXT, positiveButtonText)
                .with(ModalDialogProperties.NEGATIVE_BUTTON_TEXT, negativeButtonText)
                .with(ModalDialogProperties.PRIMARY_BUTTON_FILLED, primaryButtonFilled);
    }

    private boolean hasSufficientSpaceForIllustration(int heightPx) {
        heightPx -= TabModalPresenter.getContainerTopMargin(mResources, mContainerHeightResource);
        heightPx -= TabModalPresenter.getContainerBottomMargin(mFullscreenManager);
        return heightPx >= mResources.getDimensionPixelSize(
                       R.dimen.password_manager_dialog_min_vertical_space_to_show_illustration);
    }

    private SpannableString addBoldSpanToDetails(
            String details, int boldRangeStart, int boldRangeEnd) {
        SpannableString spannableDetails = new SpannableString(details);
        StyleSpan boldSpan = new StyleSpan(Typeface.BOLD);
        spannableDetails.setSpan(
                boldSpan, boldRangeStart, boldRangeEnd, Spanned.SPAN_INCLUSIVE_INCLUSIVE);
        return spannableDetails;
    }

    @Override
    public void onLayoutChange(View view, int left, int top, int right, int bottom, int oldLeft,
            int oldTop, int oldRight, int oldBottom) {
        int oldHeight = oldBottom - oldTop;
        int newHeight = bottom - top;
        if (newHeight == oldHeight) return;
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
            mModel.set(ILLUSTRATION_VISIBLE, hasSufficientSpaceForIllustration(newHeight));
        });
    }

    void showDialog(@ModalDialogManager.ModalDialogType int type) {
        mModel.set(ILLUSTRATION_VISIBLE,
                hasSufficientSpaceForIllustration(mAndroidContentView.getHeight()));
        mDialogModel = mModalDialogBuilder.build();
        mDialogManager.showDialog(mDialogModel, type);
    }

    void dismissDialog(int dismissalClause) {
        mDialogManager.dismissDialog(mDialogModel, dismissalClause);
        mAndroidContentView.removeOnLayoutChangeListener(this);
    }

    @VisibleForTesting
    public PropertyModel getModelForTesting() {
        return mModel;
    }
}
