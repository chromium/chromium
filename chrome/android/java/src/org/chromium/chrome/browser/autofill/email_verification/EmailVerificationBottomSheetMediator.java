// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.email_verification;

import android.content.Context;
import android.os.Handler;
import android.os.Looper;
import android.os.SystemClock;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillSheetUiController;
import org.chromium.chrome.browser.autofill.AutofillSheetUiControllerFactory;
import org.chromium.components.autofill.EmailVerificationPermissionUiStatus;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class for the email verification UI.
 *
 * <p>This component shows a bottom sheet to let the user confirm or decline automatic email
 * verification.
 *
 * <p>This mediator sends UI events (onUiShown, onUiDecision) to the delegate.
 */
@NullMarked
/*package*/ class EmailVerificationBottomSheetMediator implements BottomSheetObserver {
    /*package*/ static final long MIN_LOADING_TIME_MS = 800L;

    private final EmailVerificationBottomSheetContent mContent;
    private final AutofillSheetUiController mUiController;
    private final EmailVerificationBottomSheetCoordinator.Delegate mDelegate;
    private final PropertyModel mModel;
    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private @Nullable Runnable mPendingDismissRunnable;
    private long mLoadingStartTimeMs;
    private boolean mActionTaken;

    /**
     * Creates the mediator.
     *
     * @param context The context for this component.
     * @param title The title text of the bottom sheet.
     * @param description The description text of the bottom sheet.
     * @param content The bottom sheet content to be shown.
     * @param uiController The controller to use for showing or hiding the content.
     * @param delegate The delegate to signal UI flow events to.
     */
    EmailVerificationBottomSheetMediator(
            Context context,
            String title,
            String description,
            EmailVerificationBottomSheetContent content,
            AutofillSheetUiController uiController,
            EmailVerificationBottomSheetCoordinator.Delegate delegate) {
        mContent = content;
        mUiController = uiController;
        mDelegate = delegate;

        String confirmButtonLabel =
                context.getString(R.string.autofill_email_verifier_prompt_verify);
        String cancelButtonLabel =
                context.getString(R.string.autofill_email_verifier_prompt_not_now);

        mModel =
                new PropertyModel.Builder(EmailVerificationBottomSheetProperties.ALL_KEYS)
                        .with(EmailVerificationBottomSheetProperties.TITLE, title)
                        .with(EmailVerificationBottomSheetProperties.DESCRIPTION, description)
                        .with(
                                EmailVerificationBottomSheetProperties.CONFIRM_BUTTON_LABEL,
                                confirmButtonLabel)
                        .with(
                                EmailVerificationBottomSheetProperties.CANCEL_BUTTON_LABEL,
                                cancelButtonLabel)
                        .with(
                                EmailVerificationBottomSheetProperties.DRAG_HANDLE_VISIBLE,
                                !AutofillSheetUiControllerFactory.shouldUseNonBlockingDialog(
                                        context))
                        .with(
                                EmailVerificationBottomSheetProperties.ON_CONFIRM_CLICKED,
                                this::onAccepted)
                        .with(
                                EmailVerificationBottomSheetProperties.ON_CANCEL_CLICKED,
                                this::onDeclined)
                        .with(EmailVerificationBottomSheetProperties.SHOW_LOADING_STATE, false)
                        .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    /** Requests to show the bottom sheet content. */
    void requestShowContent() {
        if (mUiController.requestShowContent(mContent, /* animate= */ true)) {
            mUiController.addObserver(this);
            mDelegate.onUiShown();
        } else {
            notifyDecision(EmailVerificationPermissionUiStatus.OTHER);
        }
    }

    public void onAccepted() {
        if (mActionTaken) return;
        mLoadingStartTimeMs = SystemClock.elapsedRealtime();
        mModel.set(EmailVerificationBottomSheetProperties.SHOW_LOADING_STATE, true);
        notifyDecision(EmailVerificationPermissionUiStatus.ALLOWED);
    }

    public void onDeclined() {
        if (mActionTaken) return;
        hideImmediately(StateChangeReason.INTERACTION_COMPLETE);
        notifyDecision(EmailVerificationPermissionUiStatus.DECLINED);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        if (mPendingDismissRunnable != null) {
            mHandler.removeCallbacks(mPendingDismissRunnable);
            mPendingDismissRunnable = null;
        }
        mHandler.removeCallbacksAndMessages(null);
        mUiController.removeObserver(this);
        mModel.set(EmailVerificationBottomSheetProperties.SHOW_LOADING_STATE, false);
        mDelegate.onUiDismissed();
        switch (reason) {
            case StateChangeReason.BACK_PRESS:
            case StateChangeReason.SWIPE:
            case StateChangeReason.TAP_SCRIM:
                notifyDecision(EmailVerificationPermissionUiStatus.USER_ABORTED);
                break;
            case StateChangeReason.NAVIGATION:
            case StateChangeReason.COMPOSITED_UI:
            case StateChangeReason.VR:
            case StateChangeReason.PROMOTE_TAB:
            case StateChangeReason.OMNIBOX_FOCUS:
                notifyDecision(EmailVerificationPermissionUiStatus.TAB_GONE);
                break;
            case StateChangeReason.INTERACTION_COMPLETE:
                // Handled by onAccepted() / onDeclined() / hide().
                break;
            default:
                notifyDecision(EmailVerificationPermissionUiStatus.OTHER);
                break;
        }
    }

    private void notifyDecision(@EmailVerificationPermissionUiStatus int status) {
        if (mActionTaken) return;
        mActionTaken = true;
        mDelegate.onUiDecision(status);
    }

    /**
     * Hides the bottom sheet (if showing). If the bottom sheet is currently in the loading state,
     * ensures it remains visible for at least {@link #MIN_LOADING_TIME_MS} (800ms) since loading
     * started when completing the interaction.
     */
    void hide(@StateChangeReason int hideReason) {
        if (!mActionTaken) {
            hideImmediately(hideReason);
            notifyDecision(EmailVerificationPermissionUiStatus.USER_ABORTED);
            return;
        }
        if (mModel.get(EmailVerificationBottomSheetProperties.SHOW_LOADING_STATE)
                && hideReason == StateChangeReason.INTERACTION_COMPLETE) {
            if (mPendingDismissRunnable != null) {
                return;
            }
            long elapsed = SystemClock.elapsedRealtime() - mLoadingStartTimeMs;
            long remaining = Math.max(0, MIN_LOADING_TIME_MS - elapsed);
            if (remaining > 0) {
                mPendingDismissRunnable =
                        () -> {
                            mPendingDismissRunnable = null;
                            hideImmediately(hideReason);
                        };
                mHandler.postDelayed(mPendingDismissRunnable, remaining);
                return;
            }
        }
        hideImmediately(hideReason);
    }

    private void hideImmediately(@StateChangeReason int hideReason) {
        if (mPendingDismissRunnable != null) {
            mHandler.removeCallbacks(mPendingDismissRunnable);
            mPendingDismissRunnable = null;
        }
        mHandler.removeCallbacksAndMessages(null);
        mUiController.removeObserver(this);
        mUiController.hideContent(mContent, /* animate= */ true, hideReason);
        mModel.set(EmailVerificationBottomSheetProperties.SHOW_LOADING_STATE, false);
        mDelegate.onUiDismissed();
    }

    /*package*/ @Nullable Runnable getPendingDismissRunnableForTesting() {
        return mPendingDismissRunnable;
    }

    /*package*/ long getLoadingStartTimeMsForTesting() {
        return mLoadingStartTimeMs;
    }
}
