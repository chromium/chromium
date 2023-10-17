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

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.prefs.PrefService;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.KeyboardVisibilityDelegate;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Coordinates the password generation bottom sheet functionality. It shows the bottom sheet, fills
 * in the generated password and handles the bottom sheet dismissal.
 */
class TouchToFillPasswordGenerationCoordinator {
    interface Delegate {
        /** Called when the bottom sheet is hidden (both by the user and programmatically). */
        void onDismissed(boolean passwordAccepted);

        /**
         * Called, when user agrees to use the generated password.
         *
         * @param password the password, which was used.
         */
        void onGeneratedPasswordAccepted(String password);

        /** Called when the user rejects the generated password. */
        void onGeneratedPasswordRejected();
    }

    @IntDef({
        InteractionResult.USED_GENERATED_PASSWORD,
        InteractionResult.DISMISSED_GENERATED_PASSWORD,
        InteractionResult.DISMISSED_FROM_NATIVE
    })
    @Retention(RetentionPolicy.SOURCE)
    private @interface InteractionResult {
        int USED_GENERATED_PASSWORD = 0;
        int DISMISSED_GENERATED_PASSWORD = 1;
        int DISMISSED_FROM_NATIVE = 2;
    }

    private final WebContents mWebContents;
    private final PrefService mPrefService;
    private final TouchToFillPasswordGenerationView mTouchToFillPasswordGenerationView;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final Delegate mTouchToFillPasswordGenerationDelegate;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    onDismissed(sheetStateChangeReasonToInteractionResult(reason));
                }
            };

    public TouchToFillPasswordGenerationCoordinator(
            BottomSheetController bottomSheetController,
            Context context,
            WebContents webContents,
            PrefService prefService,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            Delegate touchToFillPasswordGenerationDelegate) {
        this(
                webContents,
                prefService,
                bottomSheetController,
                createView(context),
                keyboardVisibilityDelegate,
                touchToFillPasswordGenerationDelegate);
    }

    private static TouchToFillPasswordGenerationView createView(Context context) {
        return new TouchToFillPasswordGenerationView(context,
                LayoutInflater.from(context).inflate(
                        R.layout.touch_to_fill_password_generation, null));
    }

    @VisibleForTesting
    TouchToFillPasswordGenerationCoordinator(
            WebContents webContents,
            PrefService prefService,
            BottomSheetController bottomSheetController,
            TouchToFillPasswordGenerationView touchToFillPasswordGenerationView,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            Delegate touchToFillPasswordGenerationDelegate) {
        mWebContents = webContents;
        mPrefService = prefService;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mTouchToFillPasswordGenerationDelegate = touchToFillPasswordGenerationDelegate;
        mBottomSheetController = bottomSheetController;
        mTouchToFillPasswordGenerationView = touchToFillPasswordGenerationView;
    }

    /** Displays the bottom sheet. */
    boolean show(String generatedPassword, String account) {
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

    void hideFromNative() {
        onDismissed(InteractionResult.DISMISSED_FROM_NATIVE);
    }

    private void onDismissed(@InteractionResult int interactionResult) {
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.hideContent(mTouchToFillPasswordGenerationView, true);
        mTouchToFillPasswordGenerationDelegate.onDismissed(
                interactionResult == InteractionResult.USED_GENERATED_PASSWORD);

        setPasswordGenerationBottomSheetDismissCount(interactionResult);
    }

    private void onGeneratedPasswordAccepted(String password) {
        mTouchToFillPasswordGenerationDelegate.onGeneratedPasswordAccepted(password);
        onDismissed(InteractionResult.USED_GENERATED_PASSWORD);
    }

    private void onGeneratedPasswordRejected() {
        mTouchToFillPasswordGenerationDelegate.onGeneratedPasswordRejected();
        onDismissed(InteractionResult.DISMISSED_GENERATED_PASSWORD);
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

    private void setPasswordGenerationBottomSheetDismissCount(
            @InteractionResult int interactionResult) {
        if (interactionResult == InteractionResult.DISMISSED_GENERATED_PASSWORD) {
            mPrefService.setInteger(
                    Pref.PASSWORD_GENERATION_BOTTOM_SHEET_DISMISS_COUNT,
                    mPrefService.getInteger(Pref.PASSWORD_GENERATION_BOTTOM_SHEET_DISMISS_COUNT)
                            + 1);
        } else if (interactionResult == InteractionResult.USED_GENERATED_PASSWORD) {
            mPrefService.setInteger(Pref.PASSWORD_GENERATION_BOTTOM_SHEET_DISMISS_COUNT, 0);
        }
        // DISMISSED_FROM_NATIVE doesn't stem from a user choice and as such isn't counted as a user
        // dismissal.
    }

    private @InteractionResult int sheetStateChangeReasonToInteractionResult(
            @StateChangeReason int reason) {
        if (reason == StateChangeReason.SWIPE
                || reason == StateChangeReason.BACK_PRESS
                || reason == StateChangeReason.TAP_SCRIM
                || reason == StateChangeReason.OMNIBOX_FOCUS) {
            return InteractionResult.DISMISSED_GENERATED_PASSWORD;
        }
        return InteractionResult.USED_GENERATED_PASSWORD;
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
