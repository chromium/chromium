// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.password_manager;

import androidx.annotation.IdRes;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.ui.modaldialog.ModalDialogManager;

/** Class containing all data that customizes the contents displayed in the dialog. */
public class PasswordManagerDialogContents {

    private final String mTitle;
    private final String mDetails;
    private final String mPrimaryButtonText;
    private final @Nullable String mSecondaryButtonText;
    private final @IdRes int mIllustrationId;
    private final Callback<Integer> mButtonClickCallback;

    private boolean mPrimaryButtonFilled;
    private @Nullable Runnable mHelpButtonCallback;
    private @ModalDialogManager.ModalDialogType int mDialogType;

    /**
     * Constructor for the dialog contents.
     *
     * @param title               The title of the dialog, to be displayed below the
     *                            image.
     * @param details             The details text to be displayed under the title.
     * @param illustrationId      The resource id of the image displayed above the
     *                            title.
     * @param primaryButtonText   The text of the primary button.
     * @param secondaryButtonText The text of the secondary button or null if there
     *                            shouldn't be a secondary button.
     * @param buttonClickCallback The callback handling the click on the buttons. It
     *                            takes the type of the button as a parameter.
     */
    public PasswordManagerDialogContents(
            String title,
            String details,
            int illustrationId,
            String primaryButtonText,
            @Nullable String secondaryButtonText,
            Callback<Integer> buttonClickCallback) {
        mTitle = title;
        mDetails = details;
        mPrimaryButtonText = primaryButtonText;
        mSecondaryButtonText = secondaryButtonText;
        mIllustrationId = illustrationId;
        mButtonClickCallback = buttonClickCallback;
        mPrimaryButtonFilled = false;
        mHelpButtonCallback = null;
        mDialogType = ModalDialogManager.ModalDialogType.APP;
    }

    /** Sets whether or not the primary button should be displayed as filled. */
    public void setPrimaryButtonFilled(boolean primaryButtonFilled) {
        mPrimaryButtonFilled = primaryButtonFilled;
    }

    /**
     * Sets a callback to be invoked when the help button is clicked. If left null, no help button
     * will be displayed.
     */
    public void setHelpButtonCallback(Runnable helpButtonCallback) {
        mHelpButtonCallback = helpButtonCallback;
    }

    /** Sets type of the modal dialog to be displayed: app or tab modal. */
    public void setDialogType(@ModalDialogManager.ModalDialogType int type) {
        mDialogType = type;
    }

    /** Returns the title of the dialog. It is also used as content description. */
    public String getTitle() {
        return mTitle;
    }

    /** Returns the details to be displayed in the dialog under the title. */
    public String getDetails() {
        return mDetails;
    }

    /** Returns the text displayed in the primary button. */
    public String getPrimaryButtonText() {
        return mPrimaryButtonText;
    }

    /**
     * Returns the text displayed in the secondary button or null if the dialog has only one button.
     */
    public @Nullable String getSecondaryButtonText() {
        return mSecondaryButtonText;
    }

    /** The resource id of the image displayed above the title. */
    public @IdRes int getIllustrationId() {
        return mIllustrationId;
    }

    /**
     * The callback invoked when either of the two dialog buttons is clicked.
     * @return A {@link Callback} taking an {@link Integer} as a parameter which represents which
     *         button was clicked.
     */
    public Callback getButtonClickCallback() {
        return mButtonClickCallback;
    }

    /** Whether the primary button should be displayed as filled or not. */
    public boolean isPrimaryButtonFilled() {
        return mPrimaryButtonFilled;
    }

    /**
     * Returns the callback handling the click on the help icon or null if no help icon should be
     * displayed.
     */
    public @Nullable Runnable getHelpButtonCallback() {
        return mHelpButtonCallback;
    }

    /** The type of the dialog: app modal or tab modal. */
    public @ModalDialogManager.ModalDialogType int getDialogType() {
        return mDialogType;
    }
}
