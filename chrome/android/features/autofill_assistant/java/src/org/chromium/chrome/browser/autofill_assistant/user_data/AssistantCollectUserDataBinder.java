// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.app.Activity;
import android.view.View;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.ChromeVersionInfo;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionContainer;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillPaymentApp;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.payments.BasicCardUtils;
import org.chromium.chrome.browser.payments.CardEditor;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.payments.PaymentInstrument;
import org.chromium.chrome.browser.widget.prefeditor.EditorDialog;
import org.chromium.components.payments.MethodStrings;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

/**
 * This class is responsible for pushing updates to the Autofill Assistant UI for requesting user
 * data. These updates are pulled from the {@link AssistantCollectUserDataModel} when a notification
 * of an update is received.
 */
class AssistantCollectUserDataBinder
        implements PropertyModelChangeProcessor.ViewBinder<AssistantCollectUserDataModel,
                AssistantCollectUserDataBinder.ViewHolder, PropertyKey> {

    /**
     * A wrapper class that holds the different views of the CollectUserData request.
     */
    static class ViewHolder {
        private final View mRootView;
        private final AssistantVerticalExpanderAccordion mPaymentRequestExpanderAccordion;
        private final int mSectionToSectionPadding;
        private final AssistantLoginSection mLoginSection;
        private final AssistantContactDetailsSection mContactDetailsSection;
        private final AssistantDateSection mDateRangeStartSection;
        private final AssistantDateSection mDateRangeEndSection;
        private final AssistantPaymentMethodSection mPaymentMethodSection;
        private final AssistantShippingAddressSection mShippingAddressSection;
        private final AssistantTermsSection mTermsSection;
        private final AssistantTermsSection mTermsAsCheckboxSection;
        private final AssistantAdditionalSectionContainer mPrependedSections;
        private final AssistantAdditionalSectionContainer mAppendedSections;
        private final Object mDividerTag;
        private final Activity mActivity;
        private PersonalDataManager.PersonalDataManagerObserver mPersonalDataManagerObserver;

        public ViewHolder(View rootView, AssistantVerticalExpanderAccordion accordion,
                int sectionPadding, AssistantLoginSection loginSection,
                AssistantContactDetailsSection contactDetailsSection,
                AssistantDateSection dateRangeStartSection,
                AssistantDateSection dateRangeEndSection,
                AssistantPaymentMethodSection paymentMethodSection,
                AssistantShippingAddressSection shippingAddressSection,
                AssistantTermsSection termsSection, AssistantTermsSection termsAsCheckboxSection,
                AssistantAdditionalSectionContainer prependedSections,
                AssistantAdditionalSectionContainer appendedSections, Object dividerTag,
                Activity activity) {
            mRootView = rootView;
            mPaymentRequestExpanderAccordion = accordion;
            mSectionToSectionPadding = sectionPadding;
            mLoginSection = loginSection;
            mContactDetailsSection = contactDetailsSection;
            mDateRangeStartSection = dateRangeStartSection;
            mDateRangeEndSection = dateRangeEndSection;
            mPaymentMethodSection = paymentMethodSection;
            mShippingAddressSection = shippingAddressSection;
            mTermsSection = termsSection;
            mTermsAsCheckboxSection = termsAsCheckboxSection;
            mPrependedSections = prependedSections;
            mAppendedSections = appendedSections;
            mDividerTag = dividerTag;
            mActivity = activity;
        }

        /**
         * Explicitly clean up.
         */
        public void destroy() {
            stopListenToPersonalDataManager();
        }

        private void startListenToPersonalDataManager(
                PersonalDataManager.PersonalDataManagerObserver observer) {
            if (mPersonalDataManagerObserver != null) {
                return;
            }
            mPersonalDataManagerObserver = observer;
            PersonalDataManager.getInstance().registerDataObserver(mPersonalDataManagerObserver);
        }

        private void stopListenToPersonalDataManager() {
            if (mPersonalDataManagerObserver == null) {
                return;
            }
            PersonalDataManager.getInstance().unregisterDataObserver(mPersonalDataManagerObserver);
            mPersonalDataManagerObserver = null;
        }
    }

    @Override
    public void bind(
            AssistantCollectUserDataModel model, ViewHolder view, PropertyKey propertyKey) {
        boolean handled = updateEditors(model, propertyKey, view);
        handled = updateRootVisibility(model, propertyKey, view) || handled;
        handled = updateSectionVisibility(model, propertyKey, view) || handled;
        handled = updateSectionTitles(model, propertyKey, view) || handled;
        handled = updateSectionContents(model, propertyKey, view) || handled;
        handled = updateSectionSelectedItem(model, propertyKey, view) || handled;
        /* Update section paddings *after* updating section visibility. */
        handled = updateSectionPaddings(model, propertyKey, view) || handled;

        if (propertyKey == AssistantCollectUserDataModel.DELEGATE) {
            AssistantCollectUserDataDelegate collectUserDataDelegate =
                    model.get(AssistantCollectUserDataModel.DELEGATE);

            AssistantTermsSection.Delegate termsDelegate =
                    collectUserDataDelegate == null ? null : new AssistantTermsSection.Delegate() {
                        @Override
                        public void onStateChanged(@AssistantTermsAndConditionsState int state) {
                            collectUserDataDelegate.onTermsAndConditionsChanged(state);
                        }

                        @Override
                        public void onLinkClicked(int link) {
                            // TODO(b/143128544) refactor to do this the right way.
                            PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> {
                                view.mTermsSection.setTermsStatus(
                                        AssistantTermsAndConditionsState.NOT_SELECTED);
                                view.mTermsAsCheckboxSection.setTermsStatus(
                                        AssistantTermsAndConditionsState.NOT_SELECTED);
                            });
                            collectUserDataDelegate.onTermsAndConditionsLinkClicked(link);
                        }
                    };
            AssistantDateSection.Delegate dateStartDelegate = collectUserDataDelegate == null
                    ? null
                    : (year, month, day, hour, minute, second) -> {
                AssistantDateTime newStartValue =
                        new AssistantDateTime(year, month, day, hour, minute, second);
                if (newStartValue.getTimeInUtcMillis()
                        > view.mDateRangeEndSection.getCurrentValue().getTimeInUtcMillis()) {
                    view.mDateRangeEndSection.setCurrentValue(newStartValue);
                }
                collectUserDataDelegate.onDateTimeRangeStartChanged(
                        year, month, day, hour, minute, second);
            };
            AssistantDateSection.Delegate dateEndDelegate = collectUserDataDelegate == null
                    ? null
                    : (year, month, day, hour, minute, second) -> {
                AssistantDateTime newEndValue =
                        new AssistantDateTime(year, month, day, hour, minute, second);
                if (newEndValue.getTimeInUtcMillis()
                        < view.mDateRangeStartSection.getCurrentValue().getTimeInUtcMillis()) {
                    view.mDateRangeStartSection.setCurrentValue(newEndValue);
                }
                collectUserDataDelegate.onDateTimeRangeEndChanged(
                        year, month, day, hour, minute, second);
            };
            view.mTermsSection.setDelegate(termsDelegate);
            view.mTermsAsCheckboxSection.setDelegate(termsDelegate);
            view.mContactDetailsSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onContactInfoChanged
                            : null);
            view.mPaymentMethodSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onPaymentMethodChanged
                            : null);
            view.mShippingAddressSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onShippingAddressChanged
                            : null);
            view.mLoginSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onLoginChoiceChanged
                            : null);
            view.mDateRangeStartSection.setDelegate(dateStartDelegate);
            view.mDateRangeEndSection.setDelegate(dateEndDelegate);
            view.mPrependedSections.setDelegate(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onKeyValueChanged
                            : null);
            view.mAppendedSections.setDelegate(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onKeyValueChanged
                            : null);
        } else if (propertyKey == AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS) {
            updateAvailablePaymentMethods(model);
        } else if (propertyKey == AssistantCollectUserDataModel.SUPPORTED_PAYMENT_METHODS) {
            updateAvailableAutofillPaymentMethods(model);
        } else {
            assert handled : "Unhandled property detected in AssistantCollectUserDataBinder!";
        }
    }

    private boolean shouldShowContactDetails(AssistantCollectUserDataModel model) {
        return model.get(AssistantCollectUserDataModel.REQUEST_NAME)
                || model.get(AssistantCollectUserDataModel.REQUEST_PHONE)
                || model.get(AssistantCollectUserDataModel.REQUEST_EMAIL);
    }

    private boolean updateSectionTitles(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.LOGIN_SECTION_TITLE) {
            view.mLoginSection.setTitle(
                    model.get(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_START_LABEL) {
            view.mDateRangeStartSection.setTitle(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_START_LABEL));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_END_LABEL) {
            view.mDateRangeEndSection.setTitle(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_END_LABEL));
            return true;
        }
        return false;
    }

    /**
     * Updates the available items for each PR section.
     * @return whether the property key was handled.
     */
    private boolean updateSectionContents(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS) {
            List<AutofillPaymentInstrument> availablePaymentMethods =
                    model.get(AssistantCollectUserDataModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS);
            if (availablePaymentMethods == null) availablePaymentMethods = Collections.emptyList();
            view.mPaymentMethodSection.onAvailablePaymentMethodsChanged(availablePaymentMethods);
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_PROFILES
                || propertyKey == AssistantCollectUserDataModel.DEFAULT_EMAIL) {
            List<PersonalDataManager.AutofillProfile> autofillProfiles =
                    model.get(AssistantCollectUserDataModel.AVAILABLE_PROFILES);
            if (autofillProfiles == null) {
                autofillProfiles = Collections.emptyList();
            }
            if (shouldShowContactDetails(model)) {
                view.mContactDetailsSection.onProfilesChanged(autofillProfiles,
                        model.get(AssistantCollectUserDataModel.REQUEST_EMAIL),
                        model.get(AssistantCollectUserDataModel.REQUEST_NAME),
                        model.get(AssistantCollectUserDataModel.REQUEST_PHONE),
                        model.get(AssistantCollectUserDataModel.DEFAULT_EMAIL));
            }
            if (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
                view.mPaymentMethodSection.onProfilesChanged(autofillProfiles);
            }
            if (model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
                view.mShippingAddressSection.onProfilesChanged(autofillProfiles);
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUIRE_BILLING_POSTAL_CODE
                || propertyKey == AssistantCollectUserDataModel.BILLING_POSTAL_CODE_MISSING_TEXT) {
            view.mPaymentMethodSection.setRequiresBillingPostalCode(
                    model.get(AssistantCollectUserDataModel.REQUIRE_BILLING_POSTAL_CODE));
            view.mPaymentMethodSection.setBillingPostalCodeMissingText(
                    model.get(AssistantCollectUserDataModel.BILLING_POSTAL_CODE_MISSING_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_LOGINS) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
                view.mLoginSection.onLoginsChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_LOGINS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_START) {
            view.mDateRangeStartSection.setDateChoiceOptions(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_START));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_END) {
            view.mDateRangeEndSection.setDateChoiceOptions(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_END));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PREPENDED_SECTIONS) {
            view.mPrependedSections.setSections(
                    model.get(AssistantCollectUserDataModel.PREPENDED_SECTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.APPENDED_SECTIONS) {
            view.mAppendedSections.setSections(
                    model.get(AssistantCollectUserDataModel.APPENDED_SECTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT) {
            view.mTermsSection.setAcceptTermsAndConditionsText(
                    model.get(AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT));
            view.mTermsAsCheckboxSection.setAcceptTermsAndConditionsText(
                    model.get(AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT) {
            view.mTermsSection.setTermsRequireReviewText(
                    model.get(AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT));
            view.mTermsAsCheckboxSection.setTermsRequireReviewText(
                    model.get(AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.THIRDPARTY_PRIVACY_NOTICE_TEXT) {
            view.mTermsSection.setThirdPartyPrivacyNoticeText(
                    model.get(AssistantCollectUserDataModel.THIRDPARTY_PRIVACY_NOTICE_TEXT));
            view.mTermsAsCheckboxSection.setThirdPartyPrivacyNoticeText(
                    model.get(AssistantCollectUserDataModel.THIRDPARTY_PRIVACY_NOTICE_TEXT));
            return true;
        }

        return false;
    }

    /**
     * Updates visibility of PR sections.
     * @return whether the property key was handled.
     */
    private boolean updateSectionVisibility(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if ((propertyKey == AssistantCollectUserDataModel.REQUEST_NAME)
                || (propertyKey == AssistantCollectUserDataModel.REQUEST_EMAIL)
                || (propertyKey == AssistantCollectUserDataModel.REQUEST_PHONE)) {
            view.mContactDetailsSection.setVisible(shouldShowContactDetails(model));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS) {
            view.mShippingAddressSection.setVisible(
                    model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUEST_PAYMENT) {
            view.mPaymentMethodSection.setVisible(
                    (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX) {
            if (model.get(AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX)) {
                view.mTermsSection.getView().setVisibility(View.GONE);
                view.mTermsAsCheckboxSection.getView().setVisibility(View.VISIBLE);
            } else {
                view.mTermsSection.getView().setVisibility(View.VISIBLE);
                view.mTermsAsCheckboxSection.getView().setVisibility(View.GONE);
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE) {
            view.mLoginSection.setVisible(
                    model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUEST_DATE_RANGE) {
            boolean requestDateRange = model.get(AssistantCollectUserDataModel.REQUEST_DATE_RANGE);
            view.mDateRangeStartSection.setVisible(requestDateRange);
            view.mDateRangeEndSection.setVisible(requestDateRange);
            return true;
        }
        return false;
    }

    /**
     * Updates visibility of the root widget.
     * @return whether the property key was handled.
     */
    private boolean updateRootVisibility(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey != AssistantCollectUserDataModel.VISIBLE) {
            return false;
        }
        int visibility =
                model.get(AssistantCollectUserDataModel.VISIBLE) ? View.VISIBLE : View.GONE;
        if (view.mRootView.getVisibility() != visibility) {
            if (visibility == View.VISIBLE) {
                // Update credit cards before PR is made visible.
                updateAvailablePaymentMethods(model);

                view.startListenToPersonalDataManager(() -> {
                    AssistantCollectUserDataBinder.this.updateAvailablePaymentMethods(model);
                });
            } else {
                view.stopListenToPersonalDataManager();
            }
            view.mRootView.setVisibility(visibility);
        }
        return true;
    }

    /**
     * Updates the currently selected item in each PR section.
     * @return whether the property key was handled.
     */
    private boolean updateSectionSelectedItem(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.SHIPPING_ADDRESS) {
            view.mShippingAddressSection.addOrUpdateItem(
                    model.get(AssistantCollectUserDataModel.SHIPPING_ADDRESS), true);
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PAYMENT_METHOD) {
            view.mPaymentMethodSection.addOrUpdateItem(
                    model.get(AssistantCollectUserDataModel.PAYMENT_METHOD), true);
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.CONTACT_DETAILS) {
            view.mContactDetailsSection.addOrUpdateItem(
                    model.get(AssistantCollectUserDataModel.CONTACT_DETAILS), true);
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.TERMS_STATUS) {
            int termsStatus = model.get(AssistantCollectUserDataModel.TERMS_STATUS);
            view.mTermsSection.setTermsStatus(termsStatus);
            view.mTermsAsCheckboxSection.setTermsStatus(termsStatus);
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_LOGIN) {
            view.mLoginSection.addOrUpdateItem(
                    model.get(AssistantCollectUserDataModel.SELECTED_LOGIN), true);
            return true;
        }
        return false;
    }

    /**
     * Updates the paddings between sections and section dividers.
     * @return whether the property key was handled.
     */
    private boolean updateSectionPaddings(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if ((propertyKey != AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_NAME)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_EMAIL)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_PHONE)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_PAYMENT)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_DATE_RANGE)
                && (propertyKey != AssistantCollectUserDataModel.EXPANDED_SECTION)
                && (propertyKey != AssistantCollectUserDataModel.PREPENDED_SECTIONS)
                && (propertyKey != AssistantCollectUserDataModel.APPENDED_SECTIONS)) {
            return false;
        }

        // Update section paddings such that the first and last section are flush to the top/bottom,
        // and all other sections have the same amount of padding in-between them.

        if (!model.get(AssistantCollectUserDataModel.PREPENDED_SECTIONS).isEmpty()) {
            view.mPrependedSections.setPaddings(
                    0, view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mLoginSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mContactDetailsSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mDateRangeStartSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mDateRangeEndSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
            view.mLoginSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mContactDetailsSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mDateRangeStartSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mDateRangeEndSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (shouldShowContactDetails(model)) {
            view.mContactDetailsSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mDateRangeStartSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mDateRangeEndSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_DATE_RANGE)) {
            view.mDateRangeStartSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mDateRangeEndSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mPaymentMethodSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
            view.mPaymentMethodSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mShippingAddressSection.setPaddings(
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
            view.mShippingAddressSection.setPaddings(0, view.mSectionToSectionPadding);
            view.mAppendedSections.setPaddings(view.mSectionToSectionPadding,
                    view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        } else if (!model.get(AssistantCollectUserDataModel.APPENDED_SECTIONS).isEmpty()) {
            view.mAppendedSections.setPaddings(
                    0, view.mSectionToSectionPadding, view.mSectionToSectionPadding);
        }
        view.mTermsSection.setPaddings(view.mSectionToSectionPadding, 0);
        view.mTermsAsCheckboxSection.setPaddings(view.mSectionToSectionPadding, 0);

        // Hide dividers for currently invisible sections and after the expanded section, if any.
        boolean prevSectionIsExpandedOrInvisible = false;
        for (int i = 0; i < view.mPaymentRequestExpanderAccordion.getChildCount(); i++) {
            View child = view.mPaymentRequestExpanderAccordion.getChildAt(i);
            if (child instanceof AssistantVerticalExpander) {
                prevSectionIsExpandedOrInvisible = ((AssistantVerticalExpander) child).isExpanded()
                        || child.getVisibility() != View.VISIBLE;
            } else if (child.getTag() == view.mDividerTag) {
                child.setVisibility(prevSectionIsExpandedOrInvisible ? View.GONE : View.VISIBLE);
            } else {
                prevSectionIsExpandedOrInvisible = false;
            }
        }
        return true;
    }

    /**
     * Updates/recreates section editors.
     * @return whether the property key was handled.
     */
    private boolean updateEditors(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if ((propertyKey != AssistantCollectUserDataModel.WEB_CONTENTS)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_NAME)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_EMAIL)
                && (propertyKey != AssistantCollectUserDataModel.REQUEST_PHONE)
                && (propertyKey
                        != AssistantCollectUserDataModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS)) {
            return false;
        }

        WebContents webContents = model.get(AssistantCollectUserDataModel.WEB_CONTENTS);
        if (webContents == null) {
            view.mContactDetailsSection.setEditor(null);
            view.mPaymentMethodSection.setEditor(null);
            view.mShippingAddressSection.setEditor(null);
            return true;
        }

        if (shouldShowContactDetails(model)) {
            ContactEditor contactEditor =
                    new ContactEditor(model.get(AssistantCollectUserDataModel.REQUEST_NAME),
                            model.get(AssistantCollectUserDataModel.REQUEST_PHONE),
                            model.get(AssistantCollectUserDataModel.REQUEST_EMAIL),
                            !webContents.isIncognito());
            contactEditor.setEditorDialog(new EditorDialog(view.mActivity, null,
                    /*deleteRunnable =*/null));
            view.mContactDetailsSection.setEditor(contactEditor);
        }

        AddressEditor addressEditor = new AddressEditor(AddressEditor.Purpose.PAYMENT_REQUEST,
                /* saveToDisk= */ !webContents.isIncognito());
        addressEditor.setEditorDialog(new EditorDialog(view.mActivity, null,
                /*deleteRunnable =*/null));

        CardEditor cardEditor = new CardEditor(webContents, addressEditor,
                /* includeOrgLabel= */ false, /* observerForTest= */ null);
        Map<String, PaymentMethodData> paymentMethods =
                model.get(AssistantCollectUserDataModel.SUPPORTED_PAYMENT_METHODS);
        if (paymentMethods != null) {
            for (Map.Entry<String, PaymentMethodData> entry : paymentMethods.entrySet()) {
                cardEditor.addAcceptedPaymentMethodIfRecognized(entry.getValue());
            }
        }

        EditorDialog cardEditorDialog = new EditorDialog(view.mActivity, null,
                /*deleteRunnable =*/null);
        if (ChromeVersionInfo.isBetaBuild() || ChromeVersionInfo.isStableBuild()) {
            cardEditorDialog.disableScreenshots();
        }
        cardEditor.setEditorDialog(cardEditorDialog);

        view.mShippingAddressSection.setEditor(addressEditor);
        view.mPaymentMethodSection.setEditor(cardEditor);
        return true;
    }

    /**
     * Updates the map of supported payment methods (identifier -> methodData), filtered by
     * |SUPPORTED_BASIC_CARD_NETWORKS|.
     */
    // TODO(crbug.com/806868): Move the logic to retrieve and filter payment methods to native.
    private void updateAvailablePaymentMethods(AssistantCollectUserDataModel model) {
        // Only enable 'basic-card' payment method.
        PaymentMethodData methodData = new PaymentMethodData();
        methodData.supportedMethod = MethodStrings.BASIC_CARD;

        // Apply basic-card filter if specified
        List<String> supportedBasicCardNetworks =
                model.get(AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS);
        if (supportedBasicCardNetworks != null && supportedBasicCardNetworks.size() > 0) {
            ArrayList<Integer> filteredNetworks = new ArrayList<>();
            Map<String, Integer> networks = BasicCardUtils.getNetworkIdentifiers();
            for (int i = 0; i < supportedBasicCardNetworks.size(); i++) {
                assert networks.containsKey(supportedBasicCardNetworks.get(i));
                filteredNetworks.add(networks.get(supportedBasicCardNetworks.get(i)));
            }

            methodData.supportedNetworks = new int[filteredNetworks.size()];
            for (int i = 0; i < filteredNetworks.size(); i++) {
                methodData.supportedNetworks[i] = filteredNetworks.get(i);
            }
        }

        Map<String, PaymentMethodData> supportedPaymentMethods = new HashMap<>();
        supportedPaymentMethods.put(MethodStrings.BASIC_CARD, methodData);
        model.set(AssistantCollectUserDataModel.SUPPORTED_PAYMENT_METHODS, supportedPaymentMethods);
    }

    /**
     * Updates the list of available autofill payment methods (i.e., the list of available payment
     * methods available to the user as items in the UI).
     */
    // TODO(crbug.com/806868): Move this logic to native.
    private void updateAvailableAutofillPaymentMethods(AssistantCollectUserDataModel model) {
        WebContents webContents = model.get(AssistantCollectUserDataModel.WEB_CONTENTS);

        AutofillPaymentApp autofillPaymentApp = new AutofillPaymentApp(webContents);
        Map<String, PaymentMethodData> supportedPaymentMethods =
                model.get(AssistantCollectUserDataModel.SUPPORTED_PAYMENT_METHODS);
        List<PaymentInstrument> paymentMethods = autofillPaymentApp.getInstruments(
                supportedPaymentMethods != null ? supportedPaymentMethods : Collections.emptyMap(),
                /*forceReturnServerCards=*/true);

        List<AutofillPaymentInstrument> availablePaymentMethods = new ArrayList<>();
        for (PaymentInstrument method : paymentMethods) {
            if (method instanceof AutofillPaymentInstrument) {
                availablePaymentMethods.add(((AutofillPaymentInstrument) method));
            }
        }
        model.set(AssistantCollectUserDataModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS,
                availablePaymentMethods);
    }
}
