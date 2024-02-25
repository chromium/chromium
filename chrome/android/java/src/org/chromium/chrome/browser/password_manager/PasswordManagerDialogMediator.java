// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.password_manager;

import static org.chromium.chrome.browser.password_manager.PasswordManagerDialogProperties.ILLUSTRATION_VISIBLE;

import android.content.res.Resources;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.modaldialog.ChromeTabModalPresenter;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogProperties;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator class responsible for the logic of showing the password manager dialog. */
class PasswordManagerDialogMediator implements View.OnLayoutChangeListener {
    private final ModalDialogManager mDialogManager;
    private final View mAndroidContentView;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;

    private PropertyModel.Builder mHostDialogModelBuilder;
    private PropertyModel mHostDialogModel;
    private PropertyModel mModel;
    private Resources mResources;
    private @ModalDialogManager.ModalDialogType int mDialogType;

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

    PasswordManagerDialogMediator(
            PropertyModel.Builder hostDialogModelBuilder,
            ModalDialogManager manager,
            View androidContentView,
            BrowserControlsStateProvider controlsStateProvider) {
        mDialogManager = manager;
        mHostDialogModelBuilder = hostDialogModelBuilder;
        mAndroidContentView = androidContentView;
        mBrowserControlsStateProvider = controlsStateProvider;
        mAndroidContentView.addOnLayoutChangeListener(this);
    }

    void initialize(PropertyModel model, View view, PasswordManagerDialogContents contents) {
        mResources = view.getResources();
        mModel = model;
        mHostDialogModel =
                mHostDialogModelBuilder
                        .with(ModalDialogProperties.CUSTOM_VIEW, view)
                        .with(
                                ModalDialogProperties.CONTROLLER,
                                new DialogClickHandler(contents.getButtonClickCallback()))
                        .with(ModalDialogProperties.CONTENT_DESCRIPTION, contents.getTitle())
                        .with(
                                ModalDialogProperties.POSITIVE_BUTTON_TEXT,
                                contents.getPrimaryButtonText())
                        .with(
                                ModalDialogProperties.NEGATIVE_BUTTON_TEXT,
                                contents.getSecondaryButtonText())
                        .with(
                                ModalDialogProperties.BUTTON_STYLES,
                                contents.isPrimaryButtonFilled()
                                        ? ModalDialogProperties.ButtonStyles
                                                .PRIMARY_FILLED_NEGATIVE_OUTLINE
                                        : ModalDialogProperties.ButtonStyles
                                                .PRIMARY_OUTLINE_NEGATIVE_OUTLINE)
                        .build();
        mDialogType = contents.getDialogType();
    }

    private boolean hasSufficientSpaceForIllustration(int heightPx) {
        // If |mResources| is null, it means that the dialog was not initialized yet.
        if (mResources == null) return false;
        heightPx -=
                ChromeTabModalPresenter.getContainerTopMargin(
                        mResources, mBrowserControlsStateProvider);
        heightPx -= ChromeTabModalPresenter.getContainerBottomMargin(mBrowserControlsStateProvider);
        return heightPx
                >= mResources.getDimensionPixelSize(
                        R.dimen.password_manager_dialog_min_vertical_space_to_show_illustration);
    }

    @Override
    public void onLayoutChange(
            View view,
            int left,
            int top,
            int right,
            int bottom,
            int oldLeft,
            int oldTop,
            int oldRight,
            int oldBottom) {
        // Return if the dialog wasn't initialized
        if (mModel == null) return;
        int oldHeight = oldBottom - oldTop;
        int newHeight = bottom - top;
        if (newHeight == oldHeight) return;
        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    mModel.set(ILLUSTRATION_VISIBLE, hasSufficientSpaceForIllustration(newHeight));
                });
    }

    void showDialog() {
        mModel.set(
                ILLUSTRATION_VISIBLE,
                hasSufficientSpaceForIllustration(mAndroidContentView.getHeight()));
        mHostDialogModel = mHostDialogModelBuilder.build();
        mDialogManager.showDialog(mHostDialogModel, mDialogType);
    }

    void dismissDialog(int dismissalCause) {
        mDialogManager.dismissDialog(mHostDialogModel, dismissalCause);
        mAndroidContentView.removeOnLayoutChangeListener(this);
    }

    public PropertyModel getModelForTesting() {
        return mModel;
    }
}
