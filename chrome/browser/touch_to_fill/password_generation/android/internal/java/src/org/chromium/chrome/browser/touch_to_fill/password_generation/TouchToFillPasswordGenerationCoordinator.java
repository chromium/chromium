// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.password_generation;

import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationProperties.ACCOUNT_EMAIL;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationProperties.GENERATED_PASSWORD;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationProperties.PASSWORD_ACCEPTED_CALLBACK;
import static org.chromium.chrome.browser.touch_to_fill.password_generation.TouchToFillPasswordGenerationProperties.PASSWORD_REJECTED_CALLBACK;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.VisibleForTesting;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Coordinates the password generation bottom sheet functionality. It shows the bottom sheet, fills
 * in the generated password and handles the bottom sheet dismissal.
 */
class TouchToFillPasswordGenerationCoordinator {
    interface Delegate {
        /** Called when the bottom sheet is hidden (both by user and programmatically). */
        void onDismissed();

        /**
         * Called, when user agrees to use the generated password.
         *
         * @param password the password, which was used.
         */
        void onGeneratedPasswordAccepted(String password);

        /** Called when the user rejects the generated password. */
        void onGeneratedPasswordRejected();
    }

    private final WebContents mWebContents;
    private final TouchToFillPasswordGenerationView mTouchToFillPasswordGenerationView;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final Delegate mTouchToFillPasswordGenerationDelegate;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@StateChangeReason int reason) {
            onDismissed(reason);
        }
    };

    public TouchToFillPasswordGenerationCoordinator(BottomSheetController bottomSheetController,
            Context context, WebContents webContents,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            Delegate touchToFillPasswordGenerationDelegate) {
        this(webContents, bottomSheetController, createView(context), keyboardVisibilityDelegate,
                touchToFillPasswordGenerationDelegate);
    }

    private static TouchToFillPasswordGenerationView createView(Context context) {
        return new TouchToFillPasswordGenerationView(context,
                LayoutInflater.from(context).inflate(
                        R.layout.touch_to_fill_password_generation, null));
    }

    @VisibleForTesting
    TouchToFillPasswordGenerationCoordinator(WebContents webContents,
            BottomSheetController bottomSheetController,
            TouchToFillPasswordGenerationView touchToFillPasswordGenerationView,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            Delegate touchToFillPasswordGenerationDelegate) {
        mWebContents = webContents;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mTouchToFillPasswordGenerationDelegate = touchToFillPasswordGenerationDelegate;
        mBottomSheetController = bottomSheetController;
        mTouchToFillPasswordGenerationView = touchToFillPasswordGenerationView;
    }

    /**
     *  Displays the bottom sheet.
     */
    public boolean show(String generatedPassword, String account) {
        PropertyModel model =
                new PropertyModel.Builder(TouchToFillPasswordGenerationProperties.ALL_KEYS)
                        .with(ACCOUNT_EMAIL, account)
                        .with(GENERATED_PASSWORD, generatedPassword)
                        .with(PASSWORD_ACCEPTED_CALLBACK, this::onGeneratedPasswordAccepted)
                        .with(PASSWORD_REJECTED_CALLBACK, this::onGeneratedPasswordRejected)
                        .build();
        setUpModelChangeProcessors(model, mTouchToFillPasswordGenerationView);

        mBottomSheetController.addObserver(mBottomSheetObserver);
        if (mBottomSheetController.requestShowContent(mTouchToFillPasswordGenerationView, true)) {
            return true;
        }
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        return false;
    }

    public void hide() {
        onDismissed(StateChangeReason.NONE);
    }

    private void onDismissed(@StateChangeReason int reason) {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.hideContent(mTouchToFillPasswordGenerationView, true);
        mTouchToFillPasswordGenerationDelegate.onDismissed();
    }

    private void onGeneratedPasswordAccepted(String password) {
        mTouchToFillPasswordGenerationDelegate.onGeneratedPasswordAccepted(password);
        onDismissed(StateChangeReason.INTERACTION_COMPLETE);
    }

    private void onGeneratedPasswordRejected() {
        mTouchToFillPasswordGenerationDelegate.onGeneratedPasswordRejected();
        onDismissed(StateChangeReason.INTERACTION_COMPLETE);
        restoreKeyboardFocus();
    }

    void restoreKeyboardFocus() {
        if (mWebContents.getViewAndroidDelegate() == null) return;
        if (mWebContents.getViewAndroidDelegate().getContainerView() == null) return;

        View webContentView = mWebContents.getViewAndroidDelegate().getContainerView();
        if (webContentView.requestFocus()) {
            mKeyboardVisibilityDelegate.showKeyboard(webContentView);
        }
    }

    /**
     * Connects the given model with the given view using Model Change Processors.
     * @param model A {@link PropertyModel} built with {@link
     *         TouchToFillPasswordGenerationProperties}.
     * @param view A {@link TouchToFillPasswordGenerationView}.
     */
    private static void setUpModelChangeProcessors(
            PropertyModel model, TouchToFillPasswordGenerationView view) {
        PropertyModelChangeProcessor.create(
                model, view, TouchToFillPasswordGenerationViewBinder::bindView);
    }
}
