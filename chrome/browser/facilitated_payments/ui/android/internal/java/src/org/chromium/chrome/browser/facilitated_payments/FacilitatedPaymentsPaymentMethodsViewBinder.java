// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.facilitated_payments;

import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_ICON_BITMAP;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_SUMMARY;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_ACCOUNT_TRANSACTION_LIMIT;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.BANK_NAME;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.BankAccountProperties.ON_BANK_ACCOUNT_CLICK_ACTION;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.DISMISS_HANDLER;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.DESCRIPTION_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.IMAGE_DRAWABLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.HeaderProperties.TITLE_ID;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SCREEN_VIEW_MODEL;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.ERROR_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.FOP_SELECTOR;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.PROGRESS_SCREEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.SequenceScreen.UNINITIALIZED;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VISIBLE_STATE;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.HIDDEN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SHOWN;
import static org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.VisibleState.SWAPPING_SCREEN;

import android.content.Context;
import android.text.SpannableString;
import android.text.method.LinkMovementMethod;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.appcompat.content.res.AppCompatResources;

import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.AdditionalInfoProperties;
import org.chromium.chrome.browser.facilitated_payments.FacilitatedPaymentsPaymentMethodsProperties.FooterProperties;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.text.NoUnderlineClickableSpan;
import org.chromium.ui.text.SpanApplier;
import org.chromium.ui.widget.TextViewWithClickableSpans;

/**
 * Provides functions that map {@link FacilitatedPaymentsPaymentMethodsProperties} changes in a
 * {@link PropertyModel} to the suitable method in {@link FacilitatedPaymentsPaymentMethodsView}.
 */
class FacilitatedPaymentsPaymentMethodsViewBinder {
    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link FacilitatedPaymentsPaymentMethodsView} to update.
     * @param propertyKey The {@link PropertyKey} which changed.
     */
    static void bindFacilitatedPaymentsPaymentMethodsView(
            PropertyModel model,
            FacilitatedPaymentsPaymentMethodsView view,
            PropertyKey propertyKey) {
        if (propertyKey == VISIBLE_STATE) {
            switch (model.get(VISIBLE_STATE)) {
                case HIDDEN:
                    view.setVisible(false);
                    break;
                case SHOWN:
                    view.setVisible(true);
                    break;
                default:
                    assert model.get(VISIBLE_STATE) == SWAPPING_SCREEN : "Undefined visible state";
            }
        } else if (propertyKey == SCREEN) {
            switch (model.get(SCREEN)) {
                case FOP_SELECTOR:
                    {
                        FacilitatedPaymentsSequenceView fop_selector_screen =
                                new FacilitatedPaymentsFopSelectorScreen();
                        fop_selector_screen.setupView(view.getScreenHolder());
                        view.setNextScreen(fop_selector_screen);
                        model.set(SCREEN_VIEW_MODEL, fop_selector_screen.getModel());
                        break;
                    }
                case PROGRESS_SCREEN:
                    {
                        FacilitatedPaymentsSequenceView progress_screen =
                                new FacilitatedPaymentsProgressScreen();
                        progress_screen.setupView(view.getScreenHolder());
                        view.setNextScreen(progress_screen);
                        model.set(SCREEN_VIEW_MODEL, progress_screen.getModel());
                        break;
                    }
                case ERROR_SCREEN:
                    {
                        FacilitatedPaymentsSequenceView error_screen =
                                new FacilitatedPaymentsErrorScreen();
                        error_screen.setupView(view.getScreenHolder());
                        view.setNextScreen(error_screen);
                        model.set(SCREEN_VIEW_MODEL, error_screen.getModel());
                        break;
                    }
                default:
                    assert model.get(SCREEN) == UNINITIALIZED : "Undefined screen type.";
            }
        } else if (propertyKey == SCREEN_VIEW_MODEL) {
            // This property contains the model to manipulate the {@link #SCREEN} view. No need to
            // update the {@code view} for this property. Intentional fall-through.
        } else if (propertyKey == DISMISS_HANDLER) {
            view.setDismissHandler(model.get(DISMISS_HANDLER));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    private FacilitatedPaymentsPaymentMethodsViewBinder() {}

    /**
     * Factory used to create a new header inside the ListView inside the
     * FacilitatedPaymentsPaymentMethodsView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createHeaderItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.facilitated_payments_payment_methods_sheet_header_item,
                        parent,
                        false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindHeaderView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == TITLE_ID) {
            TextView sheetTitleText = view.findViewById(R.id.sheet_title);
            sheetTitleText.setText(view.getContext().getResources().getString(model.get(TITLE_ID)));
        } else if (propertyKey == DESCRIPTION_ID) {
            TextView sheetDescriptionText = view.findViewById(R.id.description_text);
            sheetDescriptionText.setText(
                    view.getContext().getResources().getString(model.get(DESCRIPTION_ID)));
        } else if (propertyKey == IMAGE_DRAWABLE_ID) {
            ImageView sheetHeaderImage = view.findViewById(R.id.branding_icon);
            sheetHeaderImage.setImageDrawable(
                    AppCompatResources.getDrawable(
                            view.getContext(), model.get(IMAGE_DRAWABLE_ID)));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create additional info below the payment methods inside the
     * FacilitatedPaymentsPaymentMethodsView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createAdditionalInfoView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.facilitated_payments_payment_methods_additional_info,
                        parent,
                        false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the additional info to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindAdditionalInfoView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == AdditionalInfoProperties.DESCRIPTION_ID
                || propertyKey == SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK) {
            TextViewWithClickableSpans descriptionLine1 =
                    (TextViewWithClickableSpans) view.findViewById(R.id.description_line);
            descriptionLine1.setText(
                    getSpannableStringWithClickableSpansToOpenLinks(
                            view.getContext(),
                            model.get(AdditionalInfoProperties.DESCRIPTION_ID),
                            model.get(SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK)));
            descriptionLine1.setMovementMethod(LinkMovementMethod.getInstance());
        } else if (propertyKey == SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK) {
            // Skip because the callback is already handled above.
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static View createContinueButtonView(ViewGroup parent) {
        View buttonView =
                LayoutInflater.from(parent.getContext())
                        .inflate(R.layout.facilitated_payments_continue_button, parent, false);
        return buttonView;
    }

    static void bindContinueButtonView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == ON_BANK_ACCOUNT_CLICK_ACTION) {
            view.setOnClickListener(unusedView -> model.get(ON_BANK_ACCOUNT_CLICK_ACTION).run());
            TextView buttonTitleText =
                    view.findViewById(R.id.facilitated_payments_continue_button_title);
            buttonTitleText.setText(R.string.autofill_payment_method_continue_button);
        } else if (propertyKey == BANK_NAME
                || propertyKey == BANK_ACCOUNT_SUMMARY
                || propertyKey == BANK_ACCOUNT_TRANSACTION_LIMIT
                || propertyKey == BANK_ACCOUNT_DRAWABLE_ID
                || propertyKey == BANK_ACCOUNT_ICON_BITMAP) {
            // Skip, because none of these changes affect the button
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    /**
     * Factory used to create a new footer inside the ListView inside the
     * FacilitatedPaymentsPaymentMethodsView.
     *
     * @param parent The parent {@link ViewGroup} of the new item.
     */
    static View createFooterItemView(ViewGroup parent) {
        return LayoutInflater.from(parent.getContext())
                .inflate(
                        R.layout.facilitated_payments_payment_methods_sheet_footer_item,
                        parent,
                        false);
    }

    /**
     * Called whenever a property in the given model changes. It updates the given view accordingly.
     *
     * @param model The observed {@link PropertyModel}. Its data need to be reflected in the view.
     * @param view The {@link View} of the header to update.
     * @param key The {@link PropertyKey} which changed.
     */
    static void bindFooterView(PropertyModel model, View view, PropertyKey propertyKey) {
        if (propertyKey == FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK) {
            setShowPaymentMethodsSettingsCallback(
                    view, model.get(FooterProperties.SHOW_PAYMENT_METHOD_SETTINGS_CALLBACK));
        } else {
            assert false : "Unhandled update to property:" + propertyKey;
        }
    }

    static SpannableString getSpannableStringWithClickableSpansToOpenLinks(
            Context context, int stringResourceId, Runnable callback) {
        return SpanApplier.applySpans(
                context.getResources().getString(stringResourceId),
                new SpanApplier.SpanInfo(
                        "<link1>",
                        "</link1>",
                        new NoUnderlineClickableSpan(context, unused -> callback.run())));
    }

    private static void setShowPaymentMethodsSettingsCallback(View view, Runnable callback) {
        View managePaymentMethodsButton = view.findViewById(R.id.manage_payment_methods);
        managePaymentMethodsButton.setOnClickListener(unused -> callback.run());
    }
}
