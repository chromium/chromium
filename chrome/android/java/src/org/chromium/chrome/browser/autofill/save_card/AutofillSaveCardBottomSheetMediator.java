// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.save_card;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * Mediator class for the autofill save card UI.
 *
 * <p>This component shows a bottom sheet to let the user choose to save a payment card (either
 * locally or uploaded).
 *
 * <p>This mediator manages the lifecycle of the bottom sheet by observing layout and tab changes.
 * When the layout is no longer on browsing (for example the tab switcher) the bottom sheet is
 * hidden. When the selected tab changes the bottom sheet is hidden.
 *
 * <p>This mediator sends UI events (OnUiShown, OnUiAccepted, etc.) to the bridge.
 */
/*package*/ class AutofillSaveCardBottomSheetMediator
        implements AutofillSaveCardBottomSheetLifecycle.ControllerDelegate {
    @VisibleForTesting
    static final String LOADING_SHOWN_HISTOGRAM = "Autofill.CreditCardUpload.LoadingShown";

    @VisibleForTesting
    static final String LOADING_RESULT_HISTOGRAM = "Autofill.CreditCardUpload.LoadingResult";

    private final AutofillSaveCardBottomSheetContent mContent;
    private final AutofillSaveCardBottomSheetLifecycle mLifecycle;
    private final BottomSheetController mBottomSheetController;
    private final PropertyModel mModel;
    private final AutofillSaveCardBottomSheetCoordinator.NativeDelegate mDelegate;
    private final boolean mIsServerCard;
    private final boolean mIsLoadingDisabled;
    private @SaveCardPromptResult int mLoadingResult;

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    // Needs to stay in sync with AutofillSavePaymentMethodPromptResultEnum in enums.xml.
    @IntDef({
        SaveCardPromptResult.ACCEPTED,
        SaveCardPromptResult.CANCELLED,
        SaveCardPromptResult.CLOSED,
        SaveCardPromptResult.NOT_INTERACTED,
        SaveCardPromptResult.LOST_FOCUS,
        SaveCardPromptResult.UNKNOWN,
        SaveCardPromptResult.COUNT
    })
    @VisibleForTesting
    @interface SaveCardPromptResult {
        int ACCEPTED = 0;
        int CANCELLED = 1;
        int CLOSED = 2;
        int NOT_INTERACTED = 3;
        int LOST_FOCUS = 4;
        int UNKNOWN = 5;
        int COUNT = 6;
    }

    /**
     * Creates the mediator.
     *
     * @param content The bottom sheet content to be shown.
     * @param lifecycle A custom lifecycle that ignores page navigation.
     * @param bottomSheetController The controller to use for showing or hiding the content.
     * @param delegate The delegate to signal UI flow events (OnUiShown, OnUiAccepted, etc.) to.
     * @param isServerCard Whether or not the bottom sheet is for a server card save.
     * @param isLoadingDisabled Whether or not the loading for the card save is disabled.
     */
    AutofillSaveCardBottomSheetMediator(
            AutofillSaveCardBottomSheetContent content,
            AutofillSaveCardBottomSheetLifecycle lifecycle,
            BottomSheetController bottomSheetController,
            PropertyModel model,
            AutofillSaveCardBottomSheetCoordinator.NativeDelegate delegate,
            boolean isServerCard,
            boolean isLoadingDisabled) {
        mContent = content;
        mLifecycle = lifecycle;
        mBottomSheetController = bottomSheetController;
        mModel = model;
        mDelegate = delegate;
        mIsServerCard = isServerCard;
        mIsLoadingDisabled = isLoadingDisabled;
    }

    /** Requests to show the bottom sheet content. */
    void requestShowContent() {
        if (mBottomSheetController.requestShowContent(mContent, /* animate= */ true)) {
            mLifecycle.begin(this);
            mDelegate.onUiShown();
        } else {
            mDelegate.onUiIgnored();
        }
    }

    public void onAccepted() {
        if (mIsServerCard
                && !mIsLoadingDisabled
                && ChromeFeatureList.isEnabled(
                        ChromeFeatureList.AUTOFILL_ENABLE_SAVE_CARD_LOADING_AND_CONFIRMATION)) {
            mModel.set(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE, true);
            // Set the loading result here so if the bottom sheet is closed without user actions,
            // it will be recorded with a finished loading result.
            mLoadingResult = SaveCardPromptResult.ACCEPTED;
            RecordHistogram.recordBooleanHistogram(LOADING_SHOWN_HISTOGRAM, true);
        } else {
            hide(StateChangeReason.INTERACTION_COMPLETE);
        }
        mDelegate.onUiAccepted();
    }

    @Override
    public void onCanceled() {
        // Don't call the onUiCanceled callback if the bottom sheet is in a loading state because
        // the bottom sheet has already been accepted.
        if (mModel.get(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE)) {
            mLoadingResult = SaveCardPromptResult.CLOSED;
        } else {
            mDelegate.onUiCanceled();
        }
        hide(StateChangeReason.INTERACTION_COMPLETE);
    }

    @Override
    public void onIgnored() {
        hide(StateChangeReason.INTERACTION_COMPLETE);
        mDelegate.onUiIgnored();
    }

    /** Hide the bottom sheet (if showing) and end the lifecycle. */
    void hide(@StateChangeReason int hideReason) {
        mLifecycle.end();
        mBottomSheetController.hideContent(mContent, /* animate= */ true, hideReason);
        if (mModel.get(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE)) {
            // Reset loading state to false to prevent a race condition from recording the metric
            // twice.
            mModel.set(AutofillSaveCardBottomSheetProperties.SHOW_LOADING_STATE, false);
            RecordHistogram.recordEnumeratedHistogram(
                    LOADING_RESULT_HISTOGRAM, mLoadingResult, SaveCardPromptResult.COUNT);
        }
    }
}
