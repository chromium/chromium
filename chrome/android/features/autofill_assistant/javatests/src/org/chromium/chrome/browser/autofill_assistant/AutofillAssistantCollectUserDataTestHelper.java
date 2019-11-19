// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.COLLECT_USER_DATA_CHOICE_LIST;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.findViewsWithTag;
import static org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataCoordinator.DIVIDER_TAG;

import android.view.View;
import android.widget.LinearLayout;

import androidx.annotation.Nullable;

import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.AutofillProfile;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantChoiceList;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataCoordinator;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataDelegate;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantDateTime;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantLoginChoice;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantTermsAndConditionsState;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantVerticalExpander;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantVerticalExpanderAccordion;
import org.chromium.chrome.browser.payments.AutofillAddress;
import org.chromium.chrome.browser.payments.AutofillContact;
import org.chromium.chrome.browser.payments.AutofillPaymentInstrument;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.concurrent.TimeoutException;

/**
 * Helper class for testing autofill assistant payment request. Code adapted from
 * https://cs.chromium.org/chromium/src/chrome/android/javatests/src/org/chromium/chrome/browser/autofill/AutofillTestHelper.java
 */
public class AutofillAssistantCollectUserDataTestHelper {
    private final CallbackHelper mOnPersonalDataChangedHelper = new CallbackHelper();

    /** Extracts the views from a coordinator. */
    static class ViewHolder {
        final AssistantVerticalExpanderAccordion mAccordion;
        final AssistantVerticalExpander mContactSection;
        final AssistantVerticalExpander mPaymentSection;
        final AssistantVerticalExpander mShippingSection;
        final AssistantVerticalExpander mLoginsSection;
        final AssistantVerticalExpander mDateRangeStartSection;
        final AssistantVerticalExpander mDateRangeEndSection;
        final LinearLayout mTermsSection;
        final AssistantChoiceList mContactList;
        final AssistantChoiceList mPaymentMethodList;
        final AssistantChoiceList mShippingAddressList;
        final AssistantChoiceList mLoginList;
        final List<View> mDividers;

        ViewHolder(AssistantCollectUserDataCoordinator coordinator) {
            mAccordion = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_ACCORDION_TAG);
            mContactSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_CONTACT_DETAILS_SECTION_TAG);
            mPaymentSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_PAYMENT_METHOD_SECTION_TAG);
            mShippingSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_SHIPPING_ADDRESS_SECTION_TAG);
            mLoginsSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_LOGIN_SECTION_TAG);
            mDateRangeStartSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_DATE_RANGE_START_TAG);
            mDateRangeEndSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_DATE_RANGE_END_TAG);
            mTermsSection = coordinator.getView().findViewWithTag(
                    AssistantTagsForTesting.COLLECT_USER_DATA_TERMS_SECTION_TAG);
            mDividers = findViewsWithTag(coordinator.getView(), DIVIDER_TAG);
            mContactList = (AssistantChoiceList) (findViewsWithTag(
                    mContactSection, COLLECT_USER_DATA_CHOICE_LIST)
                                                          .get(0));
            mPaymentMethodList = (AssistantChoiceList) (findViewsWithTag(
                    mPaymentSection, COLLECT_USER_DATA_CHOICE_LIST)
                                                                .get(0));
            mShippingAddressList = (AssistantChoiceList) (findViewsWithTag(
                    mShippingSection, COLLECT_USER_DATA_CHOICE_LIST)
                                                                  .get(0));
            mLoginList = (AssistantChoiceList) (findViewsWithTag(
                    mLoginsSection, COLLECT_USER_DATA_CHOICE_LIST)
                                                        .get(0));
        }
    }

    /**
     * Simple mock delegate which stores the currently selected PR items.
     *  TODO(crbug.com/860868): Remove this once PR is fully a MVC component, in which case one
     *  should be able to get the currently selected items by asking the model.
     */
    static class MockDelegate implements AssistantCollectUserDataDelegate {
        AutofillContact mContact;
        AutofillAddress mAddress;
        AutofillPaymentInstrument mPaymentMethod;
        AssistantLoginChoice mLoginChoice;
        AssistantDateTime mDateRangeStart;
        AssistantDateTime mDateRangeEnd;
        @AssistantTermsAndConditionsState
        int mTermsStatus;
        @Nullable
        Integer mLastLinkClicked;
        Map<String, String> mAdditionalValues = new HashMap<>();

        @Override
        public void onContactInfoChanged(@Nullable AutofillContact contact) {
            mContact = contact;
        }

        @Override
        public void onShippingAddressChanged(@Nullable AutofillAddress address) {
            mAddress = address;
        }

        @Override
        public void onPaymentMethodChanged(@Nullable AutofillPaymentInstrument paymentInstrument) {
            mPaymentMethod = paymentInstrument;
        }

        @Override
        public void onTermsAndConditionsChanged(@AssistantTermsAndConditionsState int state) {
            mTermsStatus = state;
        }

        @Override
        public void onLoginChoiceChanged(@Nullable AssistantLoginChoice loginChoice) {
            mLoginChoice = loginChoice;
        }

        @Override
        public void onTermsAndConditionsLinkClicked(int link) {
            mLastLinkClicked = link;
        }

        @Override
        public void onDateTimeRangeStartChanged(
                int year, int month, int day, int hour, int minute, int second) {
            mDateRangeStart = new AssistantDateTime(year, month, day, hour, minute, second);
        }

        @Override
        public void onDateTimeRangeEndChanged(
                int year, int month, int day, int hour, int minute, int second) {
            mDateRangeEnd = new AssistantDateTime(year, month, day, hour, minute, second);
        }

        @Override
        public void onKeyValueChanged(String key, String value) {
            mAdditionalValues.put(key, value);
        }
    }

    public AutofillAssistantCollectUserDataTestHelper() throws TimeoutException {
        registerDataObserver();
        setRequestTimeoutForTesting();
        setSyncServiceForTesting();
    }

    void setRequestTimeoutForTesting() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().setRequestTimeoutForTesting(0));
    }

    void setSyncServiceForTesting() {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().setSyncServiceForTesting());
    }

    public String setProfile(final AutofillProfile profile) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().setProfile(profile));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    /**
     * Adds a new profile with dummy data to the personal data manager.
     *
     * @param fullName The full name for the profile to create.
     * @param email The email for the profile to create.
     * @param postcode The postcode of the billing address.
     * @return the GUID of the created profile.
     */
    public String addDummyProfile(String fullName, String email, String postcode)
            throws TimeoutException {
        PersonalDataManager.AutofillProfile profile = createDummyProfile(fullName, email, postcode);
        return setProfile(profile);
    }

    public String addDummyProfile(String fullName, String email) throws TimeoutException {
        return addDummyProfile(fullName, email, "90210");
    }

    /**
     * Create a new profile.
     *
     * @param fullName The full name for the profile to create.
     * @param email The email for the profile to create.
     * @param postcode The postcode of the billing address.
     * @return the GUID of the created profile.
     */
    public PersonalDataManager.AutofillProfile createDummyProfile(
            String fullName, String email, String postcode) {
        return new PersonalDataManager.AutofillProfile("" /* guid */,
                "https://www.example.com" /* origin */, fullName, "Acme Inc.", "123 Main",
                "California", "Los Angeles", "", postcode, "", "Uzbekistan", "555 123-4567", email,
                "");
    }

    public PersonalDataManager.AutofillProfile createDummyProfile(String fullName, String email) {
        return createDummyProfile(fullName, email, "90210");
    }

    public CreditCard getCreditCard(final String guid) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getCreditCard(guid));
    }

    public String getShippingAddressLabelWithoutCountryForPaymentRequest(AutofillProfile profile) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> PersonalDataManager.getInstance()
                                   .getShippingAddressLabelWithoutCountryForPaymentRequest(
                                           profile));
    }

    public String getShippingAddressLabelWithCountryForPaymentRequest(AutofillProfile profile) {
        return TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> PersonalDataManager.getInstance()
                                   .getShippingAddressLabelWithCountryForPaymentRequest(profile));
    }

    public String setCreditCard(final CreditCard card) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        String guid = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().setCreditCard(card));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
        return guid;
    }

    /**
     * Adds a credit card with dummy data to the personal data manager.
     *
     * @param billingAddressId The billing address profile GUID.
     * @return the GUID of the created credit card
     */
    public String addDummyCreditCard(String billingAddressId) throws TimeoutException {
        String profileName = TestThreadUtils.runOnUiThreadBlockingNoException(
                () -> PersonalDataManager.getInstance().getProfile(billingAddressId).getFullName());

        PersonalDataManager.CreditCard creditCard = new PersonalDataManager.CreditCard("",
                "https://example.com", true, true, profileName, "4111111111111111", "1111", "12",
                "2050", "visa", org.chromium.chrome.autofill_assistant.R.drawable.visa_card,
                CardType.UNKNOWN, billingAddressId, "" /* serverId */);
        return setCreditCard(creditCard);
    }

    public void deleteProfile(final String guid) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().deleteProfile(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    public void deleteCreditCard(final String guid) throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.getInstance().deleteCreditCard(guid));
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }

    private void registerDataObserver() throws TimeoutException {
        int callCount = mOnPersonalDataChangedHelper.getCallCount();
        boolean isDataLoaded = TestThreadUtils.runOnUiThreadBlockingNoException(
                ()
                        -> PersonalDataManager.getInstance().registerDataObserver(
                                () -> mOnPersonalDataChangedHelper.notifyCalled()));
        if (isDataLoaded) return;
        mOnPersonalDataChangedHelper.waitForCallback(callCount);
    }
}
