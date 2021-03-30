// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.payments;

import android.os.Handler;
import android.text.TextUtils;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.components.payments.BasicCardUtils;
import org.chromium.components.payments.MethodStrings;
import org.chromium.components.payments.PaymentApp;
import org.chromium.components.payments.PaymentAppFactoryDelegate;
import org.chromium.components.payments.PaymentAppFactoryInterface;
import org.chromium.components.payments.PaymentAppFactoryParams;
import org.chromium.components.payments.PaymentFeatureList;
import org.chromium.content_public.browser.RenderFrameHost;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentDetailsModifier;
import org.chromium.payments.mojom.PaymentItem;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.payments.mojom.PaymentOptions;

import java.util.Collections;
import java.util.List;
import java.util.Map;
import java.util.Set;

/** Creates one payment app per card on file in Autofill. */
public class AutofillPaymentAppFactory implements PaymentAppFactoryInterface {
    private Handler mHandler;

    // PaymentAppFactoryInterface implementation.
    @Override
    public void create(PaymentAppFactoryDelegate delegate) {
        Creator creator = new Creator(delegate);
        if (mHandler == null) mHandler = new Handler();
        mHandler.post(() -> {
            boolean canMakePayment = creator.createPaymentApps();
            delegate.onCanMakePaymentCalculated(canMakePayment);
            delegate.onDoneCreatingPaymentApps(AutofillPaymentAppFactory.this);
        });
    }

    /**
     * Create an instance of {@link AutofillPaymentAppFactory.Creator}.
     * @param delegate The delegate of a payment app factory.
     * @return The created instance.
     */
    public static AutofillPaymentAppCreator createAppCreator(PaymentAppFactoryDelegate delegate) {
        return new Creator(delegate);
    }

    /**
     * @param methodData The payment methods and their corresponding data.
     * @return Whether the method data supports making payments with an autofill payment app.
     */
    /* package */ static boolean canMakePayments(Map<String, PaymentMethodData> methodData) {
        return !extractNetworksFromMethodData(methodData).isEmpty();
    }

    /**
     * Extracts network strings from the given method data.
     * @param methodData The payment methods and their corresponding data.
     * @return A set of strings representing the payment networks.
     */
    private static Set<String> extractNetworksFromMethodData(
            Map<String, PaymentMethodData> methodData) {
        if (!methodData.containsKey(MethodStrings.BASIC_CARD)) return Collections.emptySet();
        PaymentMethodData data = methodData.get(MethodStrings.BASIC_CARD);
        return BasicCardUtils.convertBasicCardToNetworks(data);
    }

    /** Creates one payment app per Autofill card on file that matches the given payment request. */
    private static final class Creator implements AutofillPaymentAppCreator {
        private final PaymentAppFactoryDelegate mDelegate;
        private final boolean mCanMakePayment;
        private final Set<String> mNetworks;

        // The caller should ensure the params has not been closed before creating this.
        private Creator(PaymentAppFactoryDelegate delegate) {
            assert !delegate.getParams().hasClosed();
            mDelegate = delegate;
            mNetworks = extractNetworksFromMethodData(mDelegate.getParams().getMethodData());
            mCanMakePayment = !mNetworks.isEmpty();
        }

        /** @return Whether can make payments with basic card. */
        private boolean createPaymentApps() {
            if (mDelegate.getParams().hasClosed() || !mCanMakePayment) return false;

            List<CreditCard> cards = PersonalDataManager.getInstance().getCreditCardsToSuggest(
                    /*includeServerCards=*/PaymentFeatureList.isEnabled(
                            PaymentFeatureList.WEB_PAYMENTS_RETURN_GOOGLE_PAY_IN_BASIC_CARD));
            int numberOfCards = cards.size();
            for (int i = 0; i < numberOfCards; i++) {
                // createPaymentAppForCard(card) returns null if the card network or type does not
                // match mNetworks.
                PaymentApp app = createPaymentAppForCard(cards.get(i));
                if (app != null) mDelegate.onPaymentAppCreated(app);
            }

            return true;
        }

        // AutofillPaymentAppCreator interface.
        @Override
        @Nullable
        public PaymentApp createPaymentAppForCard(CreditCard card) {
            if (!mCanMakePayment || mDelegate.getParams().hasClosed()) return null;

            if (!mNetworks.contains(card.getBasicCardIssuerNetwork())) return null;

            AutofillProfile billingAddress = TextUtils.isEmpty(card.getBillingAddressId())
                    ? null
                    : PersonalDataManager.getInstance().getProfile(card.getBillingAddressId());

            if (billingAddress != null
                    && AutofillAddress.checkAddressCompletionStatus(
                               billingAddress, AutofillAddress.CompletenessCheckType.IGNORE_PHONE)
                            != AutofillAddress.CompletionStatus.COMPLETE) {
                billingAddress = null;
            }

            if (billingAddress == null) card.setBillingAddressId(null);

            return new AutofillPaymentInstrument(mDelegate.getParams().getWebContents(), card,
                    billingAddress, MethodStrings.BASIC_CARD);
        }
    }

    /**
     * Checks for usable Autofill card on file.
     *
     * @param webContents The web contents where PaymentRequest was invoked.
     * @param methodData The payment methods and their corresponding data.
     * @return Whether there's a usable Autofill card on file.
     */
    /* package */ static boolean hasUsableAutofillCard(
            WebContents webContents, Map<String, PaymentMethodData> methodData) {
        PaymentAppFactoryParams params = new PaymentAppFactoryParams() {
            @Override
            public WebContents getWebContents() {
                return webContents;
            }

            @Override
            public Map<String, PaymentMethodData> getMethodData() {
                return methodData;
            }

            @Override
            public boolean hasClosed() {
                return false;
            }

            @Override
            public RenderFrameHost getRenderFrameHost() {
                // AutofillPaymentAppFactory.Creator doesn't need RenderFrameHost.
                assert false : "getRenderFrameHost() should not be called";
                return null;
            }

            @Override
            public boolean isOffTheRecord() {
                // AutofillPaymentAppFactory.Creator doesn't need to know isOffTheRecord.
                assert false : "isOffTheRecord() should not be called";
                return false;
            }

            @Override
            public PaymentOptions getPaymentOptions() {
                // AutofillPaymentAppFactory.Creator doesn't need PaymentOptions.
                assert false : "getPaymentOptions() should not be called";
                return null;
            }

            @Override
            public PaymentItem getRawTotal() {
                // AutofillPaymentAppFactory.Creator doesn't need raw totals.
                assert false : "getRawTotals() should not be called";
                return null;
            }

            @Override
            public Map<String, PaymentDetailsModifier> getUnmodifiableModifiers() {
                // AutofillPaymentAppFactory.Creator doesn't need modifiers.
                assert false : "getUnmodifiableModifiers() should not be called";
                return null;
            }
        };
        final class UsableCardFinder implements PaymentAppFactoryDelegate {
            private boolean mResult;

            @Override
            public PaymentAppFactoryParams getParams() {
                return params;
            }

            @Override
            public void onPaymentAppCreated(PaymentApp app) {
                app.setHaveRequestedAutofillData(true);
                assert app instanceof AutofillPaymentInstrument;
                if (((AutofillPaymentInstrument) app).strictCanMakePayment()) mResult = true;
            }
        };
        UsableCardFinder usableCardFinder = new UsableCardFinder();
        Creator creator = new Creator(/*delegate=*/usableCardFinder);
        // Synchronously invokes usableCardFinder.onPaymentAppCreated(app).
        creator.createPaymentApps();
        return usableCardFinder.mResult;
    }
}
