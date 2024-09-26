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

import org.chromium.base.metrics.RecordHistogram;
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

    /**
     * The outcome of the interaction with the password generation bottom sheet. Used for metrics.
     *
     * <p>Entries should not be renumbered and numeric values should never be reused. Needs to stay
     * in sync with PasswordManager.PasswordGenerationBottomSheet.InteractionResult in enums.xml.
     */
    @IntDef({
        InteractionResult.USED_GENERATED_PASSWORD,
        InteractionResult.REJECTED_GENERATED_PASSWORD,
        InteractionResult.DISMISSED_FROM_NATIVE,
        InteractionResult.DISMISSED_SHEET,
    })
    @Retention(RetentionPolicy.SOURCE)
    @interface InteractionResult {
        int USED_GENERATED_PASSWORD = 0;
        int REJECTED_GENERATED_PASSWORD = 1;
        int DISMISSED_FROM_NATIVE = 2;
        int DISMISSED_SHEET = 3;

        int COUNT = DISMISSED_SHEET + 1;
    }

    static final String INTERACTION_RESULT_HISTOGRAM =
            "PasswordManager.PasswordGenerationBottomSheet.InteractionResult";

    private final WebContents mWebContents;
    private final PrefService mPrefService;
    private TouchToFillPasswordGenerationView mTouchToFillPasswordGenerationView;
    private View mTouchToFillPasswordGenerationContent;
    private final KeyboardVisibilityDelegate mKeyboardVisibilityDelegate;
    private final Delegate mTouchToFillPasswordGenerationDelegate;
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    // This is only called when the user swipes or touches the area outside the
                    // sheet to dismiss it. When the user clicks on one of the button inside the
                    // sheet, this observer is removed before the sheet is actually dismissed.
                    onDismissed(InteractionResult.DISMISSED_SHEET);
                }
            };

    private TouchToFillPasswordGenerationView createView(Context context) {
        mTouchToFillPasswordGenerationContent =
                LayoutInflater.from(context)
                        .inflate(R.layout.touch_to_fill_password_generation, null);
        return new TouchToFillPasswordGenerationView(
                context, mTouchToFillPasswordGenerationContent);
    }

    @VisibleForTesting
    TouchToFillPasswordGenerationCoordinator(
            WebContents webContents,
            PrefService prefService,
            BottomSheetController bottomSheetController,
            KeyboardVisibilityDelegate keyboardVisibilityDelegate,
            Delegate touchToFillPasswordGenerationDelegate) {
        mWebContents = webContents;
        mPrefService = prefService;
        mKeyboardVisibilityDelegate = keyboardVisibilityDelegate;
        mTouchToFillPasswordGenerationDelegate = touchToFillPasswordGenerationDelegate;
        mBottomSheetController = bottomSheetController;
    }

    /** Displays the bottom sheet. */
    boolean show(String generatedPassword, String account, Context context) {
        mTouchToFillPasswordGenerationView = createView(context);
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
            hideKeyboard();
            return true;
        }
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        return false;
    }

    void hideFromNative() {
        hideBottomSheet(InteractionResult.DISMISSED_FROM_NATIVE);
    }

    private void onDismissed(@InteractionResult int interactionResult) {
        hideBottomSheet(interactionResult);
        mTouchToFillPasswordGenerationDelegate.onDismissed(
                interactionResult == InteractionResult.USED_GENERATED_PASSWORD);
    }

    private void hideBottomSheet(@InteractionResult int interactionResult) {
        // It's important to remove the observer before the `mBottomSheetController.hideContent`
        // call to avoid calling `onDismissed` twice.
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController.hideContent(mTouchToFillPasswordGenerationView, true);

        setPasswordGenerationBottomSheetDismissCount(interactionResult);
        RecordHistogram.recordEnumeratedHistogram(
                INTERACTION_RESULT_HISTOGRAM, interactionResult, InteractionResult.COUNT);
    }

    private void onGeneratedPasswordAccepted(String password) {
        mTouchToFillPasswordGenerationDelegate.onGeneratedPasswordAccepted(password);
        onDismissed(InteractionResult.USED_GENERATED_PASSWORD);
    }

    private void onGeneratedPasswordRejected() {
        mTouchToFillPasswordGenerationDelegate.onGeneratedPasswordRejected();
        onDismissed(InteractionResult.REJECTED_GENERATED_PASSWORD);
    }

    private void hideKeyboard() {
        if (mWebContents.getViewAndroidDelegate() == null) return;
        View webContentView = mWebContents.getViewAndroidDelegate().getContainerView();
        if (webContentView == null) return;

        mKeyboardVisibilityDelegate.hideKeyboard(webContentView);
    }

    private void setPasswordGenerationBottomSheetDismissCount(
            @InteractionResult int interactionResult) {
        if (interactionResult == InteractionResult.REJECTED_GENERATED_PASSWORD
                || interactionResult == InteractionResult.DISMISSED_SHEET) {
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

    View getContentViewForTesting() {
        return mTouchToFillPasswordGenerationContent;
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
