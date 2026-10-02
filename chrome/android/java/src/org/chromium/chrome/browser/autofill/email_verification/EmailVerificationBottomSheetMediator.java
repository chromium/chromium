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
 * <p>This mediator sends UI events (onUiShown, onUiAccepted, onUiDismissed) to the delegate.
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
    private boolean mDismissed;

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
            notifyDismissed(EmailVerificationPermissionUiStatus.OTHER);
        }
    }

    public void onAccepted() {
        mLoadingStartTimeMs = SystemClock.elapsedRealtime();
        mModel.set(EmailVerificationBottomSheetProperties.SHOW_LOADING_STATE, true);
        mDelegate.onUiAccepted();
    }

    public void onDeclined() {
        hideImmediately(StateChangeReason.INTERACTION_COMPLETE, /* animate= */ true);
        notifyDismissed(EmailVerificationPermissionUiStatus.DECLINED);
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
        @EmailVerificationPermissionUiStatus int status;
        switch (reason) {
            case StateChangeReason.BACK_PRESS:
            case StateChangeReason.SWIPE:
            case StateChangeReason.TAP_SCRIM:
                status = EmailVerificationPermissionUiStatus.USER_ABORTED;
                break;
            case StateChangeReason.NAVIGATION:
            case StateChangeReason.COMPOSITED_UI:
            case StateChangeReason.VR:
            case StateChangeReason.PROMOTE_TAB:
            case StateChangeReason.OMNIBOX_FOCUS:
                status = EmailVerificationPermissionUiStatus.TAB_GONE;
                break;
            default:
                status = EmailVerificationPermissionUiStatus.OTHER;
                break;
        }
        notifyDismissed(status);
    }

    private void notifyDismissed(@EmailVerificationPermissionUiStatus int reason) {
        if (mDismissed) return;
        mDismissed = true;
        mDelegate.onUiDismissed(reason);
    }

    /**
     * Hides the bottom sheet (if showing). If the bottom sheet is currently in the loading state,
     * ensures it remains visible for at least {@link #MIN_LOADING_TIME_MS} (800ms) since loading
     * started when completing the interaction.
     */
    void hide() {
        if (mModel.get(EmailVerificationBottomSheetProperties.SHOW_LOADING_STATE)) {
            if (mPendingDismissRunnable != null) {
                return;
            }
            long elapsed = SystemClock.elapsedRealtime() - mLoadingStartTimeMs;
            long remaining = Math.max(0, MIN_LOADING_TIME_MS - elapsed);
            if (remaining > 0) {
                mPendingDismissRunnable =
                        () -> {
                            mPendingDismissRunnable = null;
                            hideImmediately(
                                    StateChangeReason.INTERACTION_COMPLETE, /* animate= */ true);
                            notifyDismissed(EmailVerificationPermissionUiStatus.ALLOWED);
                        };
                mHandler.postDelayed(mPendingDismissRunnable, remaining);
                return;
            }
            hideImmediately(StateChangeReason.INTERACTION_COMPLETE, /* animate= */ true);
            notifyDismissed(EmailVerificationPermissionUiStatus.ALLOWED);
            return;
        }
        hideImmediately(StateChangeReason.INTERACTION_COMPLETE, /* animate= */ true);
        notifyDismissed(EmailVerificationPermissionUiStatus.OTHER);
    }

    /**
     * Destroys this component immediately, dismissing the bottom sheet without animation and
     * cancelling any pending callbacks.
     */
    void destroy() {
        hideImmediately(StateChangeReason.NONE, /* animate= */ false);
        notifyDismissed(EmailVerificationPermissionUiStatus.OTHER);
    }

    private void hideImmediately(@StateChangeReason int hideReason, boolean animate) {
        if (mPendingDismissRunnable != null) {
            mHandler.removeCallbacks(mPendingDismissRunnable);
            mPendingDismissRunnable = null;
        }
        mHandler.removeCallbacksAndMessages(null);
        mUiController.removeObserver(this);
        mUiController.hideContent(mContent, animate, hideReason);
        mModel.set(EmailVerificationBottomSheetProperties.SHOW_LOADING_STATE, false);
    }

    /*package*/ @Nullable Runnable getPendingDismissRunnableForTesting() {
        return mPendingDismissRunnable;
    }

    /*package*/ long getLoadingStartTimeMsForTesting() {
        return mLoadingStartTimeMs;
    }
}
