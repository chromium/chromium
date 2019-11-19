// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments.micro;

import android.content.Context;

import org.chromium.chrome.browser.payments.PaymentInstrument;
import org.chromium.chrome.browser.payments.ui.LineItem;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.payments.CurrencyFormatter;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/**
 * Microtransaction coordinator, which owns the component overall, i.e., creates other objects in
 * the component and connects them. It decouples the implementation of this component from other
 * components and acts as the point of contact between them. Any code in this component that needs
 * to interact with another component does that through this coordinator.
 */
public class MicrotransactionCoordinator {
    private MicrotransactionMediator mMediator;
    private Runnable mHider;

    /** Observer for the confirmation of the microtransaction UI. */
    public interface ConfirmObserver {
        /**
         * Called after the user has confirmed payment in the microtransaction UI.
         * @param instrument The instrument to be used for the microtransaction.
         */
        void onConfirmed(PaymentInstrument instrument);
    }

    /** Observer for the dismissal of the microtransaction UI. */
    public interface DismissObserver {
        /**
         * Called after the user has dismissed the microtransaction UI by swiping it down or tapping
         * on the scrim behind the UI.
         */
        void onDismissed();
    }

    /**
     * Observer for the closing of the microtransaction UI after showing the "Payment complete"
     * message.
     */
    public interface CompleteAndCloseObserver {
        /**
         * Called after the UI has shown the "Payment complete" message and has closed itself.
         */
        void onCompletedAndClosed();
    }

    /**
     * Observer for the closing of the microtransaction UI after showing a transaction failure error
     * message.
     */
    public interface ErrorAndCloseObserver {
        /**
         * Called after the UI has shown transaction failure error message and has closed itself.
         */
        void onErroredAndClosed();
    }

    /** Constructs the microtransaction component coordinator. */
    public MicrotransactionCoordinator() {}

    /**
     * Shows the microtransaction UI.
     *
     * @param chromeActivity  The activity where the UI should be shown.
     * @param instrument      The instrument that contains the details to display and can be invoked
     *                        upon user confirmation.
     * @param formatter       Formats the account balance amount according to its currency.
     * @param total           The total amount and currency for this microtransaction.
     * @param confirmObserver The observer to be notified when the user has confirmed the
     *                        microtransaction.
     * @param dismissObserver The observer to be notified when the user has dismissed the UI.
     * @return Whether the microtransaction UI was shown. Can be false if the UI was suppressed.
     */
    public boolean show(Context context, BottomSheetController bottomSheetController,
            PaymentInstrument instrument, CurrencyFormatter formatter, LineItem total,
            ConfirmObserver confirmObserver, DismissObserver dismissObserver) {
        assert mMediator == null : "Already showing microtransaction UI";

        PropertyModel model =
                new PropertyModel.Builder(MicrotransactionProperties.ALL_KEYS)
                        .with(MicrotransactionProperties.ACCOUNT_BALANCE,
                                formatter.format(instrument.accountBalance()))
                        .with(MicrotransactionProperties.AMOUNT, total.getPrice())
                        .with(MicrotransactionProperties.CURRENCY, total.getCurrency())
                        .with(MicrotransactionProperties.IS_PEEK_STATE_ENABLED, true)
                        .with(MicrotransactionProperties.IS_SHOWING_LINE_ITEMS, true)
                        .with(MicrotransactionProperties.IS_SHOWING_PROCESSING_SPINNER, false)
                        .with(MicrotransactionProperties.PAYMENT_APP_ICON,
                                instrument.getDrawableIcon())
                        .with(MicrotransactionProperties.PAYMENT_APP_NAME, instrument.getLabel())
                        .build();

        mMediator = new MicrotransactionMediator(
                context, instrument, model, confirmObserver, dismissObserver, this::hide);

        bottomSheetController.addObserver(mMediator);

        MicrotransactionView view = new MicrotransactionView(context);
        view.mToolbarPayButton.setOnClickListener(mMediator);
        view.mContentPayButton.setOnClickListener(mMediator);

        PropertyModelChangeProcessor changeProcessor =
                PropertyModelChangeProcessor.create(model, view, MicrotransactionViewBinder::bind);

        mHider = () -> {
            mMediator.hide();
            changeProcessor.destroy();
            bottomSheetController.removeObserver(mMediator);
            bottomSheetController.hideContent(/*content=*/view, /*animate=*/true);
        };

        return bottomSheetController.requestShowContent(/*content=*/view, /*animate=*/true);
    }

    /** Hides the microtransaction UI. */
    public void hide() {
        mHider.run();
    }

    /**
     * Shows the "Payment complete" observer, closes the UI, and notifies the observer after the UI
     * has closed.
     * @param observer The observer to notify when the UI has closed.
     */
    public void showCompleteAndClose(CompleteAndCloseObserver observer) {
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
}
