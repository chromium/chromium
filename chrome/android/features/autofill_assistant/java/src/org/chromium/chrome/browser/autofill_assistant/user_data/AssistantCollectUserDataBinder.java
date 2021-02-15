// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.user_data;

import android.app.Activity;
import android.view.View;
import android.view.ViewGroup;

import androidx.annotation.Nullable;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.autofill.prefeditor.EditorDialog;
import org.chromium.chrome.browser.autofill.settings.CardEditor;
import org.chromium.chrome.browser.autofill_assistant.generic_ui.AssistantValue;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSection.Delegate;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionContainer;
import org.chromium.chrome.browser.payments.AddressEditor;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.chrome.browser.payments.ContactEditor;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.version.ChromeVersionInfo;
import org.chromium.components.payments.BasicCardUtils;
import org.chromium.components.payments.MethodStrings;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.content_public.browser.WebContents;
import org.chromium.payments.mojom.PaymentMethodData;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.Collections;
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
        private final AssistantInfoSection mInfoSection;
        private final AssistantAdditionalSectionContainer mPrependedSections;
        private final AssistantAdditionalSectionContainer mAppendedSections;
        private final ViewGroup mGenericUserInterfaceContainerPrepended;
        private final ViewGroup mGenericUserInterfaceContainerAppended;
        private final Object mDividerTag;
        private final Activity mActivity;

        public ViewHolder(View rootView, AssistantVerticalExpanderAccordion accordion,
                int sectionPadding, AssistantLoginSection loginSection,
                AssistantContactDetailsSection contactDetailsSection,
                AssistantDateSection dateRangeStartSection,
                AssistantDateSection dateRangeEndSection,
                AssistantPaymentMethodSection paymentMethodSection,
                AssistantShippingAddressSection shippingAddressSection,
                AssistantTermsSection termsSection, AssistantTermsSection termsAsCheckboxSection,
                AssistantInfoSection infoSection,
                AssistantAdditionalSectionContainer prependedSections,
                AssistantAdditionalSectionContainer appendedSections,
                ViewGroup genericUserInterfaceContainerPrepended,
                ViewGroup genericUserInterfaceContainerAppended, Object dividerTag,
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
            mInfoSection = infoSection;
            mPrependedSections = prependedSections;
            mAppendedSections = appendedSections;
            mGenericUserInterfaceContainerPrepended = genericUserInterfaceContainerPrepended;
            mGenericUserInterfaceContainerAppended = genericUserInterfaceContainerAppended;
            mDividerTag = dividerTag;
            mActivity = activity;
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
                            collectUserDataDelegate.onTextLinkClicked(link);
                        }
                    };
            AssistantDateSection.Delegate dateStartDelegate =
                    collectUserDataDelegate == null ? null : new AssistantDateSection.Delegate() {
                        @Override
                        public void onDateChanged(@Nullable AssistantDateTime date) {
                            collectUserDataDelegate.onDateTimeRangeStartDateChanged(date);
                            // Set new start date as calendar fallback for the end date.
                            view.mDateRangeEndSection.setCalendarFallbackDate(date);
                        }

                        @Override
                        public void onTimeSlotChanged(@Nullable Integer index) {
                            collectUserDataDelegate.onDateTimeRangeStartTimeSlotChanged(index);
                        }
                    };
            AssistantDateSection.Delegate dateEndDelegate =
                    collectUserDataDelegate == null ? null : new AssistantDateSection.Delegate() {
                        @Override
                        public void onDateChanged(@Nullable AssistantDateTime date) {
                            collectUserDataDelegate.onDateTimeRangeEndDateChanged(date);
                            // Set new end date as calendar fallback for the start date.
                            view.mDateRangeStartSection.setCalendarFallbackDate(date);
                        }

                        @Override
                        public void onTimeSlotChanged(@Nullable Integer index) {
                            collectUserDataDelegate.onDateTimeRangeEndTimeSlotChanged(index);
                        }
                    };
            view.mTermsSection.setDelegate(termsDelegate);
            view.mTermsAsCheckboxSection.setDelegate(termsDelegate);
            view.mInfoSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onTextLinkClicked
                            : null);
            view.mContactDetailsSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onContactInfoChanged
                            : null);
            view.mContactDetailsSection.setCompletenessDelegate(collectUserDataDelegate != null
                            ? collectUserDataDelegate::isContactComplete
                            : null);
            view.mPaymentMethodSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onPaymentMethodChanged
                            : null);
            view.mPaymentMethodSection.setCompletenessDelegate(collectUserDataDelegate != null
                            ? collectUserDataDelegate::isPaymentInstrumentComplete
                            : null);
            view.mShippingAddressSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onShippingAddressChanged
                            : null);
            view.mShippingAddressSection.setCompletenessDelegate(collectUserDataDelegate != null
                            ? collectUserDataDelegate::isShippingAddressComplete
                            : null);
            view.mLoginSection.setListener(collectUserDataDelegate != null
                            ? collectUserDataDelegate::onLoginChoiceChanged
                            : null);
            view.mDateRangeStartSection.setDelegate(dateStartDelegate);
            view.mDateRangeEndSection.setDelegate(dateEndDelegate);
            view.mPrependedSections.setDelegate(collectUserDataDelegate != null
                            ? getAdditionalSectionsDelegate(collectUserDataDelegate)
                            : null);
            view.mAppendedSections.setDelegate(collectUserDataDelegate != null
                            ? getAdditionalSectionsDelegate(collectUserDataDelegate)
                            : null);
        } else {
            assert handled : "Unhandled property detected in AssistantCollectUserDataBinder!";
        }
    }

    private Delegate getAdditionalSectionsDelegate(
            AssistantCollectUserDataDelegate collectUserDataDelegate) {
        return new Delegate() {
            @Override
            public void onValueChanged(String key, AssistantValue value) {
                collectUserDataDelegate.onKeyValueChanged(key, value);
            }

            @Override
            public void onTextFocusLost() {
                collectUserDataDelegate.onTextFocusLost();
            }
        };
    }

    private boolean shouldShowContactDetails(AssistantCollectUserDataModel model) {
        return model.get(AssistantCollectUserDataModel.REQUEST_NAME)
                || model.get(AssistantCollectUserDataModel.REQUEST_PHONE)
                || model.get(AssistantCollectUserDataModel.REQUEST_EMAIL);
    }

    private boolean updateSectionTitles(
            AssistantCollectUserDataModel model, PropertyKey propertyKey, ViewHolder view) {
        if (propertyKey == AssistantCollectUserDataModel.CONTACT_SECTION_TITLE) {
            view.mContactDetailsSection.setTitle(
                    model.get(AssistantCollectUserDataModel.CONTACT_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.LOGIN_SECTION_TITLE) {
            view.mLoginSection.setTitle(
                    model.get(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SHIPPING_SECTION_TITLE) {
            view.mShippingAddressSection.setTitle(
                    model.get(AssistantCollectUserDataModel.SHIPPING_SECTION_TITLE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_START_DATE_LABEL) {
            view.mDateRangeStartSection.setDateTitle(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_START_DATE_LABEL));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_START_TIME_LABEL) {
            view.mDateRangeStartSection.setTimeTitle(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_START_TIME_LABEL));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_END_DATE_LABEL) {
            view.mDateRangeEndSection.setDateTitle(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_END_DATE_LABEL));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_END_TIME_LABEL) {
            view.mDateRangeEndSection.setTimeTitle(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_END_TIME_LABEL));
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
        if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS
                || propertyKey == AssistantCollectUserDataModel.WEB_CONTENTS) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
                List<AutofillPaymentInstrument> paymentInstruments;
                if (model.get(AssistantCollectUserDataModel.WEB_CONTENTS) == null) {
                    paymentInstruments = Collections.emptyList();
                } else {
                    paymentInstruments =
                            model.get(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS);
                }
                view.mPaymentMethodSection.onAvailablePaymentMethodsChanged(paymentInstruments);
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_CONTACTS) {
            if (shouldShowContactDetails(model)) {
                view.mContactDetailsSection.onContactsChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_CONTACTS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
                view.mShippingAddressSection.onAddressesChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_BILLING_ADDRESSES) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
                view.mPaymentMethodSection.onAddressesChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_BILLING_ADDRESSES));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.REQUIRE_BILLING_POSTAL_CODE
                || propertyKey == AssistantCollectUserDataModel.BILLING_POSTAL_CODE_MISSING_TEXT) {
            view.mPaymentMethodSection.setRequiresBillingPostalCode(
                    model.get(AssistantCollectUserDataModel.REQUIRE_BILLING_POSTAL_CODE));
            view.mPaymentMethodSection.setBillingPostalCodeMissingText(
                    model.get(AssistantCollectUserDataModel.BILLING_POSTAL_CODE_MISSING_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.CREDIT_CARD_EXPIRED_TEXT) {
            view.mPaymentMethodSection.setCreditCardExpiredText(
                    model.get(AssistantCollectUserDataModel.CREDIT_CARD_EXPIRED_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.AVAILABLE_LOGINS) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE)) {
                view.mLoginSection.onLoginsChanged(
                        model.get(AssistantCollectUserDataModel.AVAILABLE_LOGINS));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_START_OPTIONS) {
            view.mDateRangeStartSection.setDateChoiceOptions(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_START_OPTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_START_DATE) {
            view.mDateRangeStartSection.setCurrentDate(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_START_DATE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_START_TIMESLOT) {
            view.mDateRangeStartSection.setCurrentTimeSlot(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_START_TIMESLOT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_END_OPTIONS) {
            view.mDateRangeEndSection.setDateChoiceOptions(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_END_OPTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_END_DATE) {
            view.mDateRangeEndSection.setCurrentDate(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_END_DATE));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.DATE_RANGE_END_TIMESLOT) {
            view.mDateRangeEndSection.setCurrentTimeSlot(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_END_TIMESLOT));
            return true;
        } else if (propertyKey
                == AssistantCollectUserDataModel.DATE_RANGE_DATE_NOT_SET_ERROR_MESSAGE) {
            view.mDateRangeStartSection.setDateNotSetErrorMessage(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_DATE_NOT_SET_ERROR_MESSAGE));
            view.mDateRangeEndSection.setDateNotSetErrorMessage(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_DATE_NOT_SET_ERROR_MESSAGE));
            return true;
        } else if (propertyKey
                == AssistantCollectUserDataModel.DATE_RANGE_TIME_NOT_SET_ERROR_MESSAGE) {
            view.mDateRangeStartSection.setTimeNotSetErrorMessage(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_TIME_NOT_SET_ERROR_MESSAGE));
            view.mDateRangeEndSection.setTimeNotSetErrorMessage(
                    model.get(AssistantCollectUserDataModel.DATE_RANGE_TIME_NOT_SET_ERROR_MESSAGE));
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
        } else if (propertyKey == AssistantCollectUserDataModel.INFO_SECTION_TEXT) {
            view.mInfoSection.setText(model.get(AssistantCollectUserDataModel.INFO_SECTION_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.INFO_SECTION_TEXT_CENTER) {
            view.mInfoSection.setCenter(
                    model.get(AssistantCollectUserDataModel.INFO_SECTION_TEXT_CENTER));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT) {
            view.mTermsSection.setPrivacyNoticeText(
                    model.get(AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT));
            view.mTermsAsCheckboxSection.setPrivacyNoticeText(
                    model.get(AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED) {
            view.mGenericUserInterfaceContainerPrepended.removeAllViews();
            if (model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED) != null) {
                view.mGenericUserInterfaceContainerPrepended.addView(
                        model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_PREPENDED));
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED) {
            view.mGenericUserInterfaceContainerAppended.removeAllViews();
            if (model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED) != null) {
                view.mGenericUserInterfaceContainerAppended.addView(
                        model.get(AssistantCollectUserDataModel.GENERIC_USER_INTERFACE_APPENDED));
            }
            return true;
        } else if (propertyKey
                == AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS) {
            view.mContactDetailsSection.setContactSummaryOptions(
                    model.get(AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS));
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS) {
            view.mContactDetailsSection.setContactFullOptions(
                    model.get(AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS));
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
        if (propertyKey == AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS)) {
                AutofillAddress shippingAddress =
                        model.get(AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS);
                if (shippingAddress != null) {
                    view.mShippingAddressSection.addOrUpdateItem(
                            shippingAddress, /* select= */ true);
                }
                // No need to reset selection if null, this will be handled by setItems().
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT) {
            if (model.get(AssistantCollectUserDataModel.REQUEST_PAYMENT)) {
                AutofillPaymentInstrument paymentInstrument =
                        model.get(AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT);
                if (paymentInstrument != null) {
                    view.mPaymentMethodSection.addOrUpdateItem(
                            paymentInstrument, /* select= */ true);
                }
                // No need to reset selection if null, this will be handled by setItems().
            }
            return true;
        } else if (propertyKey == AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS) {
            if (shouldShowContactDetails(model)) {
                AutofillContact contact =
                        model.get(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS);
                if (contact != null) {
                    view.mContactDetailsSection.addOrUpdateItem(contact, /* select= */ true);
                }
                // No need to reset selection if null, this will be handled by setItems().
            }
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
        // Do not set padding to the view.mTermsAsCheckboxSection, it already has "padding" from
        // its checkbox (that coincidentally matches the padding of mSectionToSectionPadding).
        view.mInfoSection.setPaddings(view.mSectionToSectionPadding, 0);

        // Hide dividers for currently invisible sections and after the expanded section, if any.
        boolean prevSectionIsExpandedOrInvisible = false;
        for (int i = 0; i < view.mPaymentRequestExpanderAccordion.getChildCount(); i++) {
            View child = view.mPaymentRequestExpanderAccordion.getChildAt(i);
            if (child instanceof AssistantVerticalExpander
                    || child == view.mDateRangeStartSection.getView()
                    || child == view.mDateRangeEndSection.getView()) {
                prevSectionIsExpandedOrInvisible = child.getVisibility() != View.VISIBLE
                        || (child instanceof AssistantVerticalExpander
                                && ((AssistantVerticalExpander) child).isExpanded());
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
                && (propertyKey != AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS)) {
            return false;
        }

        WebContents webContents = model.get(AssistantCollectUserDataModel.WEB_CONTENTS);
        if (webContents == null) {
            view.mContactDetailsSection.setEditor(null);
            view.mPaymentMethodSection.setEditor(null);
            view.mShippingAddressSection.setEditor(null);
            return true;
        }

        Profile profile = Profile.fromWebContents(webContents);
        if (shouldShowContactDetails(model)) {
            ContactEditor contactEditor =
                    new ContactEditor(model.get(AssistantCollectUserDataModel.REQUEST_NAME),
                            model.get(AssistantCollectUserDataModel.REQUEST_PHONE),
                            model.get(AssistantCollectUserDataModel.REQUEST_EMAIL),
                            !webContents.isIncognito());
            contactEditor.setEditorDialog(new EditorDialog(view.mActivity,
                    /*deleteRunnable =*/null, profile));
            view.mContactDetailsSection.setEditor(contactEditor);
        }

        AddressEditor addressEditor = new AddressEditor(AddressEditor.Purpose.PAYMENT_REQUEST,
                /* saveToDisk= */ !webContents.isIncognito());
        addressEditor.setEditorDialog(new EditorDialog(view.mActivity,
                /*deleteRunnable =*/null, profile));

        CardEditor cardEditor = new CardEditor(webContents, addressEditor,
                /* includeOrgLabel= */ false);
        List<String> supportedCardNetworks =
                model.get(AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS);
        if (supportedCardNetworks != null) {
            cardEditor.addAcceptedPaymentMethodIfRecognized(
                    getPaymentMethodDataFromNetworks(supportedCardNetworks));
        }

        EditorDialog cardEditorDialog = new EditorDialog(view.mActivity,
                /*deleteRunnable =*/null, profile);
        if (ChromeVersionInfo.isBetaBuild() || ChromeVersionInfo.isStableBuild()) {
            cardEditorDialog.disableScreenshots();
        }
        cardEditor.setEditorDialog(cardEditorDialog);

        view.mShippingAddressSection.setEditor(addressEditor);
        view.mPaymentMethodSection.setEditor(cardEditor);
        return true;
    }

    private PaymentMethodData getPaymentMethodDataFromNetworks(List<String> supportedCardNetworks) {
        // Only enable 'basic-card' payment method.
        PaymentMethodData methodData = new PaymentMethodData();
        methodData.supportedMethod = MethodStrings.BASIC_CARD;

        // Apply basic-card filter if specified
        if (supportedCardNetworks != null && supportedCardNetworks.size() > 0) {
            ArrayList<Integer> filteredNetworks = new ArrayList<>();
            Map<String, Integer> networks = BasicCardUtils.getNetworkIdentifiers();
            for (String network : supportedCardNetworks) {
                assert networks.containsKey(network);
                if (networks.containsKey(network)) {
                    filteredNetworks.add(networks.get(network));
                }
            }

            methodData.supportedNetworks = new int[filteredNetworks.size()];
            for (int i = 0; i < filteredNetworks.size(); ++i) {
                methodData.supportedNetworks[i] = filteredNetworks.get(i);
            }
        }

        return methodData;
    }
}
