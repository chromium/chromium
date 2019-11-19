// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.app.Activity;
import android.os.Build;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.LinearLayout;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionContainer;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.text.DateFormat;
import java.util.Locale;

// TODO(crbug.com/806868): Use mCarouselCoordinator to show chips.

/**
 * Coordinator for the Payment Request.
 */
public class AssistantCollectUserDataCoordinator {
    public static final String DIVIDER_TAG = "divider";
    private final Activity mActivity;
    private final LinearLayout mPaymentRequestUI;
    private final AssistantCollectUserDataModel mModel;
    private AssistantCollectUserDataBinder.ViewHolder mViewHolder;

    public AssistantCollectUserDataCoordinator(
            Activity activity, AssistantCollectUserDataModel model) {
        this(activity, model,
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                        ? activity.getResources().getConfiguration().getLocales().get(0)
                        : activity.getResources().getConfiguration().locale);
    }

    private AssistantCollectUserDataCoordinator(
            Activity activity, AssistantCollectUserDataModel model, Locale locale) {
        this(activity, model, locale,
                DateFormat.getDateTimeInstance(DateFormat.MEDIUM, DateFormat.SHORT, locale));
    }

    @VisibleForTesting
    public AssistantCollectUserDataCoordinator(Activity activity,
            AssistantCollectUserDataModel model, Locale locale, DateFormat dateFormat) {
        mActivity = activity;
        mModel = model;
        int sectionToSectionPadding = activity.getResources().getDimensionPixelSize(
                R.dimen.autofill_assistant_payment_request_section_padding);

        mPaymentRequestUI = new LinearLayout(mActivity);
        mPaymentRequestUI.setOrientation(LinearLayout.VERTICAL);
        mPaymentRequestUI.setLayoutParams(
                new ViewGroup.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ ViewGroup.LayoutParams.WRAP_CONTENT));

        AssistantVerticalExpanderAccordion paymentRequestExpanderAccordion =
                new AssistantVerticalExpanderAccordion(mActivity, null);
        paymentRequestExpanderAccordion.setOrientation(LinearLayout.VERTICAL);
        paymentRequestExpanderAccordion.setLayoutParams(
                new LinearLayout.LayoutParams(/* width= */ ViewGroup.LayoutParams.MATCH_PARENT,
                        /* height= */ 0, /* weight= */ 1));
        paymentRequestExpanderAccordion.setOnExpandedViewChangedListener(
                expander -> mModel.set(AssistantCollectUserDataModel.EXPANDED_SECTION, expander));

        mPaymentRequestUI.addView(paymentRequestExpanderAccordion,
                new LinearLayout.LayoutParams(
                        ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT));

        AssistantAdditionalSectionContainer prependedSections =
                new AssistantAdditionalSectionContainer(mActivity, paymentRequestExpanderAccordion);

        AssistantLoginSection loginSection =
                new AssistantLoginSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantContactDetailsSection contactDetailsSection =
                new AssistantContactDetailsSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);

        AssistantDateSection dateRangeStartSection = new AssistantDateSection(
                mActivity, paymentRequestExpanderAccordion, locale, dateFormat);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantDateSection dateRangeEndSection = new AssistantDateSection(
                mActivity, paymentRequestExpanderAccordion, locale, dateFormat);
        createSeparator(paymentRequestExpanderAccordion);

        AssistantPaymentMethodSection paymentMethodSection =
                new AssistantPaymentMethodSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);
        AssistantShippingAddressSection shippingAddressSection =
                new AssistantShippingAddressSection(mActivity, paymentRequestExpanderAccordion);
        createSeparator(paymentRequestExpanderAccordion);

        AssistantAdditionalSectionContainer appendedSections =
                new AssistantAdditionalSectionContainer(mActivity, paymentRequestExpanderAccordion);

        AssistantTermsSection termsSection = new AssistantTermsSection(
                mActivity, paymentRequestExpanderAccordion, /* showAsSingleCheckbox= */ false);
        AssistantTermsSection termsAsCheckboxSection =
                new AssistantTermsSection(mActivity, paymentRequestExpanderAccordion,
                        /* showAsSingleCheckbox= */ true);

        paymentRequestExpanderAccordion.setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_ACCORDION_TAG);
        loginSection.getView().setTag(AssistantTagsForTesting.COLLECT_USER_DATA_LOGIN_SECTION_TAG);
        dateRangeStartSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_DATE_RANGE_START_TAG);
        dateRangeEndSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_DATE_RANGE_END_TAG);
        contactDetailsSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_CONTACT_DETAILS_SECTION_TAG);
        paymentMethodSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_PAYMENT_METHOD_SECTION_TAG);
        shippingAddressSection.getView().setTag(
                AssistantTagsForTesting.COLLECT_USER_DATA_SHIPPING_ADDRESS_SECTION_TAG);
        termsSection.getView().setTag(AssistantTagsForTesting.COLLECT_USER_DATA_TERMS_SECTION_TAG);

        // Bind view and mediator through the model.
        mViewHolder = new AssistantCollectUserDataBinder.ViewHolder(mPaymentRequestUI,
                paymentRequestExpanderAccordion, sectionToSectionPadding, loginSection,
                contactDetailsSection, dateRangeStartSection, dateRangeEndSection,
                paymentMethodSection, shippingAddressSection, termsSection, termsAsCheckboxSection,
                prependedSections, appendedSections, DIVIDER_TAG, activity);
        AssistantCollectUserDataBinder binder = new AssistantCollectUserDataBinder();
        PropertyModelChangeProcessor.create(model, mViewHolder, binder);

        // View is initially invisible.
        model.set(AssistantCollectUserDataModel.VISIBLE, false);
    }

    public View getView() {
        return mPaymentRequestUI;
    }

    /**
     * Explicitly clean up.
     */
    public void destroy() {
        mViewHolder.destroy();
        mViewHolder = null;
    }

    private void createSeparator(ViewGroup parent) {
        View divider = LayoutInflater.from(mActivity).inflate(
                R.layout.autofill_assistant_payment_request_section_divider, parent, false);
        divider.setTag(DIVIDER_TAG);
        parent.addView(divider);
    }
}
