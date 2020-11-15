// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.minimal;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.payments.ui.LineItem;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.components.payments.PaymentApp;
import org.chromium.payments.mojom.PaymentComplete;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Payment minimal UI coordinator, which owns the component overall, i.e., creates other objects in
 * the component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class MinimalUICoordinator {
    private MinimalUIMediator mMediator;
    private Runnable mHider;

    /** Observer for minimal UI being ready for user input. */
    public interface ReadyObserver {
        /**
         * Called when the minimal UI is ready for user input, i.e., fingerprint scan or
         * click on [Pay] button.
         */
        void onReady();
    }

    /** Observer for the confirmation of the minimal UI. */
    public interface ConfirmObserver {
        /**
         * Called after the user has confirmed payment in the minimal UI.
         * @param app The app to be used for the minimal UI.
         */
        void onConfirmed(PaymentApp app);
    }

    /** Observer for the dismissal of the minimal UI. */
    public interface DismissObserver {
        /**
         * Called after the user has dismissed the minimal UI by swiping it down or tapping
         * on the scrim behind the UI.
         */
        void onDismissed();
    }

    /** Observer for the closing of the minimal UI after showing the "Payment complete" message. */
    public interface CompleteAndCloseObserver {
        /** Called after the UI has shown the "Payment complete" message and has closed itself. */
        void onCompletedAndClosed();
    }

    /**
     * Observer for the closing of the minimal UI after showing a transaction failure error message.
     */
    public interface ErrorAndCloseObserver {
        /**
         * Called after the UI has shown transaction failure error message and has closed itself.
         */
        void onErroredAndClosed();
    }

    /** Constructs the minimal UI component coordinator. */
    public MinimalUICoordinator() {}

    /**
     * Shows the minimal UI.
     *
     * @param context         The context where the UI should be shown.
     * @param app             The app that contains the details to display and can be invoked
     *                        upon user confirmation.
     * @param formatter       Formats the account balance amount according to its currency.
     * @param total           The total amount and currency for this payment.
     * @param readyObserver   The observer to be notified when the UI is ready for user input.
     * @param confirmObserver The observer to be notified when the user has confirmed payment.
     * @param dismissObserver The observer to be notified when the user has dismissed the UI.
     * @return Whether the minimal UI was shown. Can be false if the UI was suppressed.
     */
    public boolean show(Context context, BottomSheetController bottomSheetController,
            PaymentApp app, CurrencyFormatter formatter, LineItem total,
            ReadyObserver readyObserver, ConfirmObserver confirmObserver,
            DismissObserver dismissObserver) {
        assert mMediator == null : "Already showing minimal UI";

        PropertyModel model =
                new PropertyModel.Builder(MinimalUIProperties.ALL_KEYS)
                        .with(MinimalUIProperties.ACCOUNT_BALANCE,
                                formatter.format(app.accountBalance()))
                        .with(MinimalUIProperties.AMOUNT, total.getPrice())
                        .with(MinimalUIProperties.CURRENCY, total.getCurrency())
                        .with(MinimalUIProperties.IS_PEEK_STATE_ENABLED, true)
                        .with(MinimalUIProperties.IS_SHOWING_LINE_ITEMS, true)
                        .with(MinimalUIProperties.IS_SHOWING_PROCESSING_SPINNER, false)
                        .with(MinimalUIProperties.PAYMENT_APP_ICON, app.getDrawableIcon())
                        .with(MinimalUIProperties.PAYMENT_APP_NAME, app.getLabel())
                        .build();

        mMediator = new MinimalUIMediator(
                context, app, model, readyObserver, confirmObserver, dismissObserver, this::hide);

        bottomSheetController.addObserver(mMediator);

        MinimalUIView view = new MinimalUIView(context);
        view.mToolbarPayButton.setOnClickListener(mMediator);
        view.mContentPayButton.setOnClickListener(mMediator);

        PropertyModelChangeProcessor changeProcessor =
                PropertyModelChangeProcessor.create(model, view, MinimalUIViewBinder::bind);

        mHider = () -> {
            mMediator.hide();
            changeProcessor.destroy();
            bottomSheetController.removeObserver(mMediator);
            bottomSheetController.hideContent(/*content=*/view, /*animate=*/true);
        };

        return bottomSheetController.requestShowContent(/*content=*/view, /*animate=*/true);
    }

    /** Hides the minimal UI. */
    public void hide() {
        mHider.run();
    }

    /**
     * Shows the "Payment complete" observer, closes the UI, and notifies the observer after the UI
     * has closed.
     * @param observer The observer to notify when the UI has closed.
     */
    private void showCompleteAndClose(CompleteAndCloseObserver observer) {
        mMediator.showCompleteAndClose(observer);
    }

    /**
     * Shows the given error message, closes the UI, and notifies the observer after the UI has
     * closed.
     * @param observer               The observer to notify when the UI has closed.
     * @param errorMessageResourceId The resource identifier for the error message string to be
     *                               displayed in the UI before closing.
     */
    public void showErrorAndClose(ErrorAndCloseObserver observer, int errorMessageResourceId) {
        mMediator.showErrorAndClose(observer, null, errorMessageResourceId);
    }

    /**
     * Runs when the payment request is closed.
     * @param result The status of PaymentComplete.
     * @param onErroredAndClosed The function called when the UI errors and closes.
     * @param onCompletedAndClosed The function called when the UI completes and closes.
     */
    public void onPaymentRequestComplete(int result, ErrorAndCloseObserver onErroredAndClosed,
            CompleteAndCloseObserver onCompletedAndClosed) {
        if (result == PaymentComplete.FAIL) {
            showErrorAndClose(onErroredAndClosed, R.string.payments_error_message);
        } else {
            showCompleteAndClose(onCompletedAndClosed);
        }
    }

    /** Confirms payment in minimal UI. Used only in tests. */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public void confirmForTest() {
        mMediator.confirmForTest();
    }

    /** Dismisses the minimal UI. Used only in tests. */
    @VisibleForTesting(otherwise = VisibleForTesting.NONE)
    public void dismissForTest() {
        mMediator.dismissForTest();
    }
}
