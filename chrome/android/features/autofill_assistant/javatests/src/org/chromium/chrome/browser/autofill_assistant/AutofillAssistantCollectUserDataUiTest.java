// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static android.support.test.espresso.Espresso.onView;
import static android.support.test.espresso.action.ViewActions.click;
import static android.support.test.espresso.action.ViewActions.replaceText;
import static android.support.test.espresso.assertion.PositionAssertions.isAbove;
import static android.support.test.espresso.assertion.PositionAssertions.isBelow;
import static android.support.test.espresso.assertion.ViewAssertions.doesNotExist;
import static android.support.test.espresso.assertion.ViewAssertions.matches;
import static android.support.test.espresso.contrib.PickerActions.setDate;
import static android.support.test.espresso.contrib.PickerActions.setTime;
import static android.support.test.espresso.matcher.RootMatchers.isDialog;
import static android.support.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static android.support.test.espresso.matcher.ViewMatchers.isDisplayed;
import static android.support.test.espresso.matcher.ViewMatchers.withContentDescription;
import static android.support.test.espresso.matcher.ViewMatchers.withId;
import static android.support.test.espresso.matcher.ViewMatchers.withTagValue;
import static android.support.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertThat;

import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.COLLECT_USER_DATA_CHOICE_LIST;
import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.COLLECT_USER_DATA_TERMS_REQUIRE_REVIEW;
import static org.chromium.chrome.browser.autofill_assistant.AssistantTagsForTesting.VERTICAL_EXPANDER_CHEVRON;

import android.support.test.filters.MediumTest;
import android.view.View;
import android.widget.TextView;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.autofill.CardType;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantCollectUserDataTestHelper.ViewHolder;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataCoordinator;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantCollectUserDataModel;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantDateChoiceOptions;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantDateTime;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantInfoPopup;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantLoginChoice;
import org.chromium.chrome.browser.autofill_assistant.user_data.AssistantTermsAndConditionsState;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionFactory;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantStaticTextSection;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantTextInputSection;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantTextInputSection.TextInputFactory;
import org.chromium.chrome.browser.autofill_assistant.user_data.additional_sections.AssistantTextInputType;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

/**
 * Tests for the Autofill Assistant collect user data UI.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantCollectUserDataUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private AutofillAssistantCollectUserDataTestHelper mHelper;

    @Before
    public void setUp() throws Exception {
        AutofillAssistantUiTestUtil.startOnBlankPage(mTestRule);
        mHelper = new AutofillAssistantCollectUserDataTestHelper();
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantCollectUserDataCoordinator createCollectUserDataCoordinator(
            AssistantCollectUserDataModel model) throws Exception {
        AssistantCollectUserDataCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AssistantCollectUserDataCoordinator(mTestRule.getActivity(), model));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantUiTestUtil.attachToCoordinator(
                                mTestRule.getActivity(), coordinator.getView()));
        return coordinator;
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantCollectUserDataCoordinator createCollectUserDataCoordinator(
            AssistantCollectUserDataModel model, Locale locale, DateFormat dateFormat)
            throws Exception {
        AssistantCollectUserDataCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new AssistantCollectUserDataCoordinator(
                                mTestRule.getActivity(), model, locale, dateFormat));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> AutofillAssistantUiTestUtil.attachToCoordinator(
                                mTestRule.getActivity(), coordinator.getView()));
        return coordinator;
    }

    /**
     * Test assumptions about the initial state of the UI.
     */
    @Test
    @MediumTest
    public void testInitialState() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);

        /* Test initial model state. */
        assertThat(model.get(AssistantCollectUserDataModel.VISIBLE), is(false));
        assertThat(model.get(AssistantCollectUserDataModel.AVAILABLE_PROFILES), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.AVAILABLE_AUTOFILL_PAYMENT_METHODS),
                nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.SUPPORTED_PAYMENT_METHODS), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS),
                nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.EXPANDED_SECTION), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.DELEGATE), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.WEB_CONTENTS), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.SHIPPING_ADDRESS), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.PAYMENT_METHOD), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.CONTACT_DETAILS), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.TERMS_STATUS),
                is(AssistantTermsAndConditionsState.NOT_SELECTED));
        assertThat(model.get(AssistantCollectUserDataModel.SELECTED_LOGIN), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.APPENDED_SECTIONS), empty());
        assertThat(model.get(AssistantCollectUserDataModel.PREPENDED_SECTIONS), empty());

        /* Test initial UI state. */
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mLoginsSection))))
                .check(matches(not(isDisplayed())));

        /* No section divider is visible. */
        for (View divider : viewHolder.mDividers) {
            onView(is(divider)).check(matches(not(isDisplayed())));
        }
    }

    /**
     * Sections become visible/invisible depending on model changes.
     */
    @Test
    @MediumTest
    public void testSectionVisibility() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        /* Initially, everything is invisible. */
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));

        /* PR is visible, but no section was requested: all sections should be invisible. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.VISIBLE, true));
        onView(is(coordinator.getView())).check(matches(isDisplayed()));
        onView(is(viewHolder.mContactSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        /* Contact details should be visible if either name, phone, or email is requested. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.REQUEST_NAME, true));
        onView(is(viewHolder.mContactSection)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, false);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, true);
        });
        onView(is(viewHolder.mContactSection)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, false);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
        });
        onView(is(viewHolder.mContactSection)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
        });
        onView(is(viewHolder.mContactSection)).check(matches(isDisplayed()));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        /* Payment method section visibility test. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true));
        onView(is(viewHolder.mPaymentSection)).check(matches(isDisplayed()));

        /* Shipping address visibility test. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true));
        onView(is(viewHolder.mShippingSection)).check(matches(isDisplayed()));

        /* Login section visibility test. */
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true));
        onView(is(viewHolder.mLoginsSection)).check(matches(isDisplayed()));

        /* Prepended section visibility test. */
        List<AssistantAdditionalSectionFactory> prependedSections = new ArrayList<>();
        prependedSections.add(
                new AssistantStaticTextSection.Factory("Prepended section", "Lorem ipsum."));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCollectUserDataModel.PREPENDED_SECTIONS,
                                prependedSections));
        /* Login section is top-most regular section. */
        onView(withText("Prepended section")).check(isAbove(is(viewHolder.mLoginsSection)));

        /* Appended section visibility test. */
        List<AssistantAdditionalSectionFactory> appendedSections = new ArrayList<>();
        appendedSections.add(
                new AssistantStaticTextSection.Factory("Appended section", "Lorem ipsum."));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.APPENDED_SECTIONS, appendedSections));
        /* Shipping address is bottom-most regular section. */
        onView(withText("Appended section")).check(isBelow(is(viewHolder.mShippingSection)));
    }

    /**
     * Test assumptions about a payment request for a case where the personal data manager does not
     * contain any profiles or payment methods, i.e., all PR sections should be empty.
     */
    @Test
    @MediumTest
    public void testEmptyPaymentRequest() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        /* Request all PR sections. */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_DATE_RANGE, true);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        /* Empty sections should display the 'add' button in their title. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(isDisplayed()));
        /* ... Except for logins, date/time and additional sections, which currently do not support
         * adding items.*/
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mLoginsSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mDateRangeStartSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mDateRangeEndSection))))
                .check(matches(not(isDisplayed())));

        /* Empty sections should be 'fixed', i.e., they can not be expanded. */
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));

        /* Date/time range sections should always display the chevron. */
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mDateRangeStartSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mDateRangeEndSection))))
                .check(matches(isDisplayed()));

        /* Empty sections are collapsed. */
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));

        /* Empty sections should be empty. */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertThat(viewHolder.mContactList.getItemCount(), is(0));
            assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(0));
            assertThat(viewHolder.mShippingAddressList.getItemCount(), is(0));
            assertThat(viewHolder.mLoginList.getItemCount(), is(0));
        });

        /* Test delegate status. */
        assertThat(delegate.mPaymentMethod, nullValue());
        assertThat(delegate.mContact, nullValue());
        assertThat(delegate.mAddress, nullValue());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));
        assertThat(delegate.mLoginChoice, nullValue());
    }

    /**
     * Shows a payment request, then pushes a new contact list from the controller.
     * Tests whether the new contact is added to the payment request.
     */
    @Test
    @MediumTest
    public void testContactDetailsUpdates() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // WEB_CONTENTS are necessary for the creation of the editors.
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_PROFILES, Collections.emptyList());
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        // Contact details section should be empty.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        assertThat(viewHolder.mContactList.getItemCount(), is(0));

        // Add profile to the list and send the updated model.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.AVAILABLE_PROFILES,
                    new ArrayList<PersonalDataManager.AutofillProfile>() {
                        { add(mHelper.createDummyProfile("John Doe", "john@gmail.com")); }
                    });
        });

        // Contact details section should now contain and have pre-selected the new contact.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        assertThat(viewHolder.mContactList.getItemCount(), is(1));
        onView(allOf(withId(R.id.contact_summary),
                       isDescendantOfA(is(viewHolder.mContactSection.getCollapsedView()))))
                .check(matches(withText("john@gmail.com")));

        // Remove profile from the list and send the updated model. Section should be empty again.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.AVAILABLE_PROFILES, Collections.emptyList());
        });

        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        assertThat(viewHolder.mContactList.getItemCount(), is(0));

        // Tap the 'add' button to open the editor, to make sure that it still works.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .perform(click());
        onView(withId(R.id.editor_container)).check(matches(isDisplayed()));
    }

    /**
     * Shows a payment request, then adds a new payment method to the personal data manager.
     * Tests whether the new payment method is added to the payment request.
     */
    @Test
    @MediumTest
    public void testPaymentMethodsLiveUpdate() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // WEB_CONTENTS are necessary for the creation of the editors.
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        /* Payment method section should be empty and show the 'add' button in the title. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(0));

        /* Add profile and credit card to the personal data manager. */
        String billingAddressId = mHelper.addDummyProfile("Jill Doe", "jill@gmail.com");
        String creditCardId = mHelper.addDummyCreditCard(billingAddressId);

        /* Payment method section contains the new credit card, which should be pre-selected. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(1));
        onView(allOf(withId(R.id.credit_card_name),
                       isDescendantOfA(is(viewHolder.mPaymentMethodList.getItem(0)))))
                .check(matches(withText("Jill Doe")));

        /* Remove credit card from personal data manager. Section should be empty again. */
        mHelper.deleteCreditCard(creditCardId);
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(0));

        /* Tap the 'add' button to open the editor, to make sure that it still works. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .perform(click());
        onView(withId(R.id.editor_container)).check(matches(isDisplayed()));
    }

    /**
     * Test assumptions about a payment request for a personal data manager with a complete profile
     * and payment method, i.e., all PR sections should be non-empty.
     */
    @Test
    @MediumTest
    public void testNonEmptyPaymentRequest() throws Exception {
        /* Add complete profile and credit card to the personal data manager. */
        PersonalDataManager.AutofillProfile profile = new PersonalDataManager.AutofillProfile(
                "" /* guid */, "https://www.example.com" /* origin */, "Maggie Simpson",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "Uzbekistan",
                "555 123-4567", "maggie@simpson.com", "");
        String billingAddressId = mHelper.setProfile(profile);
        PersonalDataManager.CreditCard creditCard =
                new PersonalDataManager.CreditCard("", "https://example.com", true, true, "Jon Doe",
                        "4111111111111111", "1111", "12", "2050", "amex", R.drawable.amex_card,
                        CardType.UNKNOWN, billingAddressId, "" /* serverId */);
        mHelper.setCreditCard(creditCard);

        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        /* Request all PR sections. */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_PROFILES,
                    new ArrayList<PersonalDataManager.AutofillProfile>() {
                        { add(profile); }
                    });
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS,
                    Collections.singletonList(new AssistantLoginChoice(
                            "id", "Guest", "Description of guest checkout", "", 0, null)));
        });

        /* Non-empty sections should not display the 'add' button in their title. */
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mLoginsSection))))
                .check(matches(not(isDisplayed())));

        /* Non-empty sections should not be 'fixed', i.e., they can be expanded. */
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mLoginsSection))))
                .check(matches(isDisplayed()));

        /* All section dividers are visible. */
        for (View divider : viewHolder.mDividers) {
            onView(is(divider)).check(matches(isDisplayed()));
        }

        /* Check contents of sections. */
        assertThat(viewHolder.mContactList.getItemCount(), is(1));
        assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(1));
        assertThat(viewHolder.mShippingAddressList.getItemCount(), is(1));
        assertThat(viewHolder.mLoginList.getItemCount(), is(1));

        testContact("maggie@simpson.com", "Maggie Simpson\nmaggie@simpson.com",
                viewHolder.mContactSection.getCollapsedView(), viewHolder.mContactList.getItem(0));
        testPaymentMethod("1111", "Jon Doe", "12/2050",
                viewHolder.mPaymentSection.getCollapsedView(),
                viewHolder.mPaymentMethodList.getItem(0));
        testShippingAddress("Maggie Simpson", "Acme Inc., 123 Main, 90210 Los Angeles, California",
                "Acme Inc., 123 Main, 90210 Los Angeles, California, Uzbekistan",
                viewHolder.mShippingSection.getCollapsedView(),
                viewHolder.mShippingAddressList.getItem(0));
        testLoginDetails("Guest", "Description of guest checkout",
                viewHolder.mLoginsSection.getCollapsedView(), viewHolder.mLoginList.getItem(0));

        /* Check delegate status. */
        assertThat(delegate.mPaymentMethod.getCard().getNumber(), is("4111111111111111"));
        assertThat(delegate.mPaymentMethod.getCard().getName(), is("Jon Doe"));
        assertThat(delegate.mPaymentMethod.getCard().getBasicCardIssuerNetwork(), is("visa"));
        assertThat(delegate.mPaymentMethod.getCard().getBillingAddressId(), is(billingAddressId));
        assertThat(delegate.mPaymentMethod.getCard().getMonth(), is("12"));
        assertThat(delegate.mPaymentMethod.getCard().getYear(), is("2050"));
        assertThat(delegate.mContact.getPayerName(), is("Maggie Simpson"));
        assertThat(delegate.mContact.getPayerEmail(), is("maggie@simpson.com"));
        assertThat(delegate.mAddress.getProfile().getFullName(), is("Maggie Simpson"));
        assertThat(delegate.mAddress.getProfile().getStreetAddress(), containsString("123 Main"));
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));
        assertThat(delegate.mLoginChoice.getIdentifier(), is("id"));
    }

    /**
     * When the last contact info, payment method or shipping address is removed from the personal
     * data manager, the user's selection has implicitly changed (from whatever it was before to
     * null).
     */
    @Test
    @MediumTest
    public void testRemoveLastItemImplicitSelection() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();

        // Add complete profile and credit card to the personal data manager.
        PersonalDataManager.AutofillProfile profile =
                mHelper.createDummyProfile("John Doe", "john@gmail.com");
        String profileId = mHelper.setProfile(profile);
        String creditCardId = mHelper.addDummyCreditCard(profileId);

        // Request all PR sections.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_PROFILES,
                    new ArrayList<PersonalDataManager.AutofillProfile>() {
                        { add(profile); }
                    });
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        // Profile and payment method should be automatically selected.
        assertThat(delegate.mContact, not(nullValue()));
        assertThat(delegate.mAddress, not(nullValue()));
        assertThat(delegate.mPaymentMethod, not(nullValue()));

        // Remove payment method and profile
        mHelper.deleteCreditCard(creditCardId);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.AVAILABLE_PROFILES, Collections.emptyList());
        });

        // Note: before asserting that the delegate was updated, we need to ensure that the
        // UI thread has processed all events.
        onView(is(coordinator.getView())).check(matches(isDisplayed()));

        assertThat(delegate.mContact, nullValue());
        assertThat(delegate.mAddress, nullValue());
        assertThat(delegate.mPaymentMethod, nullValue());
    }

    @Test
    @MediumTest
    public void testTermsAndConditions() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();

        String acceptTermsText = "I accept";

        // Display terms as 2 radio buttons "I accept" vs "I don't".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT,
                    acceptTermsText);
            model.set(AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX, false);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));

        // Adding #isDisplayed as a requirement makes sure only one of the accept terms text is
        // shown (plus #onView requires the matcher to match exactly one view).
        Matcher<View> acceptMatcher = allOf(withText(acceptTermsText), isDisplayed());
        Matcher<View> declineMatcher = withTagValue(is(COLLECT_USER_DATA_TERMS_REQUIRE_REVIEW));

        onView(acceptMatcher).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.ACCEPTED));

        // Second click on accept doesn't change the state.
        onView(acceptMatcher).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.ACCEPTED));

        onView(declineMatcher).check(matches(isDisplayed())).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.REQUIRES_REVIEW));

        // Display the terms as a single checbox.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX, true));

        // The decline choice is not shown.
        onView(declineMatcher).check(matches(not(isDisplayed())));

        // First click marks the terms as accepted.
        onView(acceptMatcher).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.ACCEPTED));

        // Second click marks the terms as not selected.
        onView(acceptMatcher).perform(click());
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));

        // Change the "I accept" text to be a clickable link.
        String acceptTermsText2 =
                "<link42>I accept</link42>"; // second variable is necessary because used in lambda
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCollectUserDataModel.ACCEPT_TERMS_AND_CONDITIONS_TEXT,
                                acceptTermsText2));
        acceptMatcher = allOf(withText(acceptTermsText), isDisplayed());

        // Clicking the text will trigger the link.
        onView(acceptMatcher).perform(click());
        assertThat(delegate.mLastLinkClicked, is(42));
    }

    @Test
    @MediumTest
    public void testTermsRequireReview() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        createCollectUserDataCoordinator(model);

        // Setting a text from "backend".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.TERMS_REQUIRE_REVIEW_TEXT, "Check terms");
            model.set(AssistantCollectUserDataModel.SHOW_TERMS_AS_CHECKBOX, false);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        onView(withTagValue(is(COLLECT_USER_DATA_TERMS_REQUIRE_REVIEW)))
                .check(matches(allOf(withText("Check terms"), isDisplayed())));
    }

    @Test
    @MediumTest
    public void testThirdpartyPrivacyNotice() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        TextView privacyNotice = viewHolder.mTermsSection.findViewById(
                R.id.payment_request_3rd_party_privacy_notice);

        // Setting a text from "backend".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.THIRDPARTY_PRIVACY_NOTICE_TEXT,
                    "Thirdparty privacy notice");
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        onView(is(privacyNotice))
                .check(matches(allOf(withText("Thirdparty privacy notice"), isDisplayed())));
    }

    /**
     * Test that if the billing address does not have a postal code and the postal code is required,
     * an error message is displayed.
     */
    @Test
    @MediumTest
    public void testCreditCardWithoutPostcode() throws Exception {
        // add credit card without postcode.
        String profileId = mHelper.addDummyProfile("John Doe", "john@gmail.com", "");
        mHelper.addDummyCreditCard(profileId);

        // setup the view to require a billing postcode.
        AutofillAssistantCollectUserDataTestHelper.ViewHolder viewHolder =
                setupCreditCardPostalCodeTest(/* requireBillingPostalCode: */ true);

        // check that the card is not accepted (i.e. an error message is shown).
        onView(is(getPaymentSummaryErrorView(viewHolder))).check(matches(isDisplayed()));
        onView(is(getPaymentSummaryErrorView(viewHolder)))
                .check(matches(withText("Billing postcode missing")));

        // setup the view to not require a billing postcode.
        // TODO: clean previous view.
        viewHolder = setupCreditCardPostalCodeTest(/* requireBillingPostalCode: */ false);

        // check that the card is now accepted.
        onView(is(getPaymentSummaryErrorView(viewHolder))).check(matches(not(isDisplayed())));
    }

    /**
     * Test that requiring a billing postal code for a billing address that has it does not display
     * an error message.
     */
    @Test
    @MediumTest
    public void testCreditCardWithPostcode() throws Exception {
        // setup a card with a postcode.
        String profileId = mHelper.addDummyProfile("Jane Doe", "jane@gmail.com", "98004");
        mHelper.addDummyCreditCard(profileId);

        // setup the view to require a billing postcode.
        AutofillAssistantCollectUserDataTestHelper.ViewHolder viewHolder =
                setupCreditCardPostalCodeTest(/* requireBillingPostalCode: */ true);

        // check that the card is accepted.
        onView(is(getPaymentSummaryErrorView(viewHolder))).check(matches(not(isDisplayed())));
    }

    private AutofillAssistantCollectUserDataTestHelper.ViewHolder setupCreditCardPostalCodeTest(
            boolean requireBillingPostalCode) throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUIRE_BILLING_POSTAL_CODE,
                    requireBillingPostalCode);
            model.set(AssistantCollectUserDataModel.BILLING_POSTAL_CODE_MISSING_TEXT,
                    "Billing postcode missing");
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        return viewHolder;
    }

    /**
     * If the default email is set, the most complete profile with that email address should be
     * default-selected.
     */
    @Test
    @MediumTest
    public void testDefaultEmail() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        /* Set up fake profiles such that the correct default choice is last. */
        List<PersonalDataManager.AutofillProfile> profiles =
                new ArrayList<PersonalDataManager.AutofillProfile>() {
                    {
                        add(mHelper.createDummyProfile("Jane Doe", "jane@gmail.com", "98004"));
                        add(mHelper.createDummyProfile("", "joe@gmail.com", ""));
                        add(mHelper.createDummyProfile("Joe Doe", "joe@gmail.com", "98004"));
                    }
                };

        /* Request all PR sections. */
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.DEFAULT_EMAIL, "joe@gmail.com");
            model.set(AssistantCollectUserDataModel.AVAILABLE_PROFILES, profiles);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        for (int i = 0; i < viewHolder.mContactList.getItemCount(); i++) {
            if (viewHolder.mContactList.isChecked(viewHolder.mContactList.getItem(i))) {
                testContact("joe@gmail.com", "Joe Doe\njoe@gmail.com",
                        viewHolder.mContactSection.getCollapsedView(),
                        viewHolder.mContactList.getItem(i));
                break;
            }
        }

        assertThat(delegate.mContact.getPayerEmail(), is("joe@gmail.com"));
        assertThat(delegate.mContact.getPayerName(), is("Joe Doe"));
    }

    @Test
    @MediumTest
    public void testDateRangeLocaleUS() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        Locale locale = LocaleUtils.forLanguageTag("en-US");
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(
                model, locale, new SimpleDateFormat("MMM d, yyyy h:mm a", locale));
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        AssistantDateTime startTime = new AssistantDateTime(2019, 10, 21, 8, 0, 0);
        AssistantDateTime endTime = new AssistantDateTime(2019, 11, 7, 18, 30, 0);
        AssistantDateTime minTime = new AssistantDateTime(2019, 10, 21, 8, 0, 0);
        AssistantDateTime maxTime = new AssistantDateTime(2020, 10, 21, 8, 0, 0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_DATE_RANGE, true);
            model.set(AssistantCollectUserDataModel.DATE_RANGE_START,
                    new AssistantDateChoiceOptions(startTime, minTime, maxTime));
            model.set(AssistantCollectUserDataModel.DATE_RANGE_END,
                    new AssistantDateChoiceOptions(endTime, minTime, maxTime));
            model.set(AssistantCollectUserDataModel.DATE_RANGE_START_LABEL, "Pick up");
            model.set(AssistantCollectUserDataModel.DATE_RANGE_END_LABEL, "Return");
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        onView(allOf(withId(R.id.datetime), isDescendantOfA(is(viewHolder.mDateRangeStartSection)),
                       withText("Oct 21, 2019 8:00 AM")))
                .check(matches(isDisplayed()));

        onView(allOf(withId(R.id.datetime), isDescendantOfA(is(viewHolder.mDateRangeEndSection)),
                       withText("Nov 7, 2019 6:30 PM")))
                .check(matches(isDisplayed()));

        assertThat(
                delegate.mDateRangeStart.getTimeInUtcMillis(), is(startTime.getTimeInUtcMillis()));
        assertThat(delegate.mDateRangeEnd.getTimeInUtcMillis(), is(endTime.getTimeInUtcMillis()));
    }

    @Test
    @MediumTest
    public void testDateRangeLocaleDE() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        Locale locale = LocaleUtils.forLanguageTag("de-DE");
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(
                model, locale, new SimpleDateFormat("dd.MM.yyyy HH:mm", locale));
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        AssistantDateTime startTime = new AssistantDateTime(2019, 10, 21, 8, 0, 0);
        AssistantDateTime endTime = new AssistantDateTime(2019, 11, 7, 18, 30, 0);
        AssistantDateTime minTime = new AssistantDateTime(2019, 10, 21, 8, 0, 0);
        AssistantDateTime maxTime = new AssistantDateTime(2020, 10, 21, 8, 0, 0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_DATE_RANGE, true);
            model.set(AssistantCollectUserDataModel.DATE_RANGE_START,
                    new AssistantDateChoiceOptions(startTime, minTime, maxTime));
            model.set(AssistantCollectUserDataModel.DATE_RANGE_END,
                    new AssistantDateChoiceOptions(endTime, minTime, maxTime));
            model.set(AssistantCollectUserDataModel.DATE_RANGE_START_LABEL, "Pick up");
            model.set(AssistantCollectUserDataModel.DATE_RANGE_END_LABEL, "Return");
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        onView(allOf(withId(R.id.datetime), isDescendantOfA(is(viewHolder.mDateRangeStartSection)),
                       withText("21.10.2019 08:00")))
                .check(matches(isDisplayed()));

        onView(allOf(withId(R.id.datetime), isDescendantOfA(is(viewHolder.mDateRangeEndSection)),
                       withText("07.11.2019 18:30")))
                .check(matches(isDisplayed()));

        assertThat(
                delegate.mDateRangeStart.getTimeInUtcMillis(), is(startTime.getTimeInUtcMillis()));
        assertThat(delegate.mDateRangeEnd.getTimeInUtcMillis(), is(endTime.getTimeInUtcMillis()));
    }

    @Test
    @MediumTest
    public void testDateRangeClamp() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        Locale locale = LocaleUtils.forLanguageTag("en-US");
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(
                model, locale, new SimpleDateFormat("MMM d, yyyy h:mm a", locale));
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        AssistantDateTime startTime = new AssistantDateTime(2019, 11, 7, 18, 30, 0);
        AssistantDateTime endTime = new AssistantDateTime(2019, 10, 21, 8, 0, 0);
        AssistantDateTime minTime = new AssistantDateTime(2019, 10, 21, 8, 0, 0);
        AssistantDateTime maxTime = new AssistantDateTime(2020, 10, 21, 8, 0, 0);

        // Note the sequence: after the start time is set, the end time is modified to be *before*
        // the start time. This should automatically clamp the start time to the end time.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_DATE_RANGE, true);
            model.set(AssistantCollectUserDataModel.DATE_RANGE_START,
                    new AssistantDateChoiceOptions(startTime, minTime, maxTime));
            model.set(AssistantCollectUserDataModel.DATE_RANGE_END,
                    new AssistantDateChoiceOptions(endTime, minTime, maxTime));
            model.set(AssistantCollectUserDataModel.DATE_RANGE_START_LABEL, "Pick up");
            model.set(AssistantCollectUserDataModel.DATE_RANGE_END_LABEL, "Return");
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        onView(allOf(withId(R.id.datetime), isDescendantOfA(is(viewHolder.mDateRangeStartSection)),
                       withText("Oct 21, 2019 8:00 AM")))
                .check(matches(isDisplayed()));

        onView(allOf(withId(R.id.datetime), isDescendantOfA(is(viewHolder.mDateRangeEndSection)),
                       withText("Oct 21, 2019 8:00 AM")))
                .check(matches(isDisplayed()));

        assertThat(delegate.mDateRangeStart.getTimeInUtcMillis(), is(endTime.getTimeInUtcMillis()));
        assertThat(delegate.mDateRangeEnd.getTimeInUtcMillis(), is(endTime.getTimeInUtcMillis()));
    }

    @Test
    @MediumTest
    public void testDateRangePopup() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        Locale locale = LocaleUtils.forLanguageTag("en-US");
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(
                model, locale, new SimpleDateFormat("MMM d, yyyy h:mm a", locale));
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        AssistantDateTime startTime = new AssistantDateTime(2019, 10, 21, 8, 0, 0);
        AssistantDateTime endTime = new AssistantDateTime(2019, 11, 7, 18, 30, 0);
        AssistantDateTime minTime = new AssistantDateTime(2019, 10, 21, 8, 0, 0);
        AssistantDateTime maxTime = new AssistantDateTime(2020, 10, 21, 8, 0, 0);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_DATE_RANGE, true);
            model.set(AssistantCollectUserDataModel.DATE_RANGE_START,
                    new AssistantDateChoiceOptions(startTime, minTime, maxTime));
            model.set(AssistantCollectUserDataModel.DATE_RANGE_END,
                    new AssistantDateChoiceOptions(endTime, minTime, maxTime));
            model.set(AssistantCollectUserDataModel.DATE_RANGE_START_LABEL, "Pick up");
            model.set(AssistantCollectUserDataModel.DATE_RANGE_END_LABEL, "Return");
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        AssistantDateTime newStartTime = new AssistantDateTime(2019, 11, 3, 12, 0, 0);
        AssistantDateTime newEndTime = new AssistantDateTime(2019, 11, 12, 20, 30, 0);

        onView(allOf(withId(R.id.datetime), isDescendantOfA(is(viewHolder.mDateRangeStartSection))))
                .perform(click());
        onView(withId(R.id.date_picker))
                .inRoot(isDialog())
                .perform(setDate(
                        newStartTime.getYear(), newStartTime.getMonth(), newStartTime.getDay()));
        onView(withId(R.id.time_picker))
                .inRoot(isDialog())
                .perform(setTime(newStartTime.getHour(), newStartTime.getMinute()));
        onView(withId(android.R.id.button1)).inRoot(isDialog()).perform(click());

        onView(allOf(withId(R.id.datetime), isDescendantOfA(is(viewHolder.mDateRangeEndSection))))
                .perform(click());
        onView(withId(R.id.date_picker))
                .inRoot(isDialog())
                .perform(setDate(newEndTime.getYear(), newEndTime.getMonth(), newEndTime.getDay()));
        onView(withId(R.id.time_picker))
                .inRoot(isDialog())
                .perform(setTime(newEndTime.getHour(), newEndTime.getMinute()));
        onView(withId(android.R.id.button1)).inRoot(isDialog()).perform(click());

        assertThat(delegate.mDateRangeStart.getTimeInUtcMillis(),
                is(newStartTime.getTimeInUtcMillis()));
        assertThat(
                delegate.mDateRangeEnd.getTimeInUtcMillis(), is(newEndTime.getTimeInUtcMillis()));
    }

    @Test
    @MediumTest
    public void testAdditionalStaticSections() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);

        List<AssistantAdditionalSectionFactory> prependedSections = new ArrayList<>();
        prependedSections.add(
                new AssistantStaticTextSection.Factory("Prepended section 1", "Lorem ipsum."));
        prependedSections.add(
                new AssistantStaticTextSection.Factory("Prepended section 2", "Lorem ipsum."));

        List<AssistantAdditionalSectionFactory> appendedSections = new ArrayList<>();
        appendedSections.add(
                new AssistantStaticTextSection.Factory("Appended section 1", "Lorem ipsum."));
        appendedSections.add(
                new AssistantStaticTextSection.Factory("Appended section 2", "Lorem ipsum."));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.PREPENDED_SECTIONS, prependedSections);
            model.set(AssistantCollectUserDataModel.APPENDED_SECTIONS, appendedSections);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        onView(withText("Prepended section 1")).check(matches(isDisplayed()));
        onView(withText("Prepended section 2")).check(matches(isDisplayed()));
        onView(withText("Appended section 1")).check(matches(isDisplayed()));
        onView(withText("Appended section 2")).check(matches(isDisplayed()));

        onView(withText("Prepended section 1")).check(isAbove(withText("Prepended section 2")));
        onView(withText("Prepended section 2")).check(isAbove(withText("Appended section 1")));
        onView(withText("Appended section 1")).check(isAbove(withText("Appended section 2")));
    }

    @Test
    @MediumTest
    public void testAdditionalTextInputSections() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();

        List<AssistantAdditionalSectionFactory> prependedSections = new ArrayList<>();
        List<AssistantTextInputSection.TextInputFactory> textInputs = new ArrayList<>();
        textInputs.add(new TextInputFactory(AssistantTextInputType.INPUT_ALPHANUMERIC,
                "Discount code", "123456789", "discount"));
        textInputs.add(new TextInputFactory(
                AssistantTextInputType.INPUT_ALPHANUMERIC, "Loyalty code", "", "loyalty"));
        textInputs.add(
                new TextInputFactory(AssistantTextInputType.INPUT_TEXT, "Comment", "", "comment"));
        prependedSections.add(
                new AssistantTextInputSection.Factory("Discount codes title", textInputs));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.PREPENDED_SECTIONS, prependedSections);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        // Expand section
        onView(withText("Discount codes title")).perform(click());

        onView(withContentDescription("Discount code")).check(matches(isDisplayed()));
        onView(withContentDescription("Loyalty code")).check(matches(isDisplayed()));
        assertThat(delegate.mAdditionalValues.get("discount"), is("123456789"));
        assertThat(delegate.mAdditionalValues.get("loyalty"), is(""));

        onView(withContentDescription("Discount code")).perform(replaceText("D-742394"));
        onView(withContentDescription("Loyalty code")).perform(replaceText("L-394834"));
        assertThat(delegate.mAdditionalValues.get("discount"), is("D-742394"));
        assertThat(delegate.mAdditionalValues.get("loyalty"), is("L-394834"));
    }

    @Test
    @MediumTest
    public void testLoginSectionInfoPopup() throws Exception {
        AssistantCollectUserDataModel model = new AssistantCollectUserDataModel();
        createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();

        AssistantInfoPopup infoPopup =
                new AssistantInfoPopup("Guest checkout", "Text explanation.");
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            model.set(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE, "Login options");
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS,
                    Collections.singletonList(new AssistantLoginChoice(
                            "id", "Guest checkout", "", "", 0, infoPopup)));
        });

        onView(withText("Login options")).perform(click());
        onView(withContentDescription(mTestRule.getActivity().getString(R.string.learn_more)))
                .perform(click());
        onView(withText("Guest checkout")).check(matches(isDisplayed()));
        onView(withText("Text explanation.")).check(matches(isDisplayed()));
        onView(withText(mTestRule.getActivity().getString(R.string.close))).perform(click());
    }

    private View getPaymentSummaryErrorView(ViewHolder viewHolder) {
        return viewHolder.mPaymentSection.findViewById(R.id.payment_method_summary)
                .findViewById(R.id.incomplete_error);
    }

    private void testContact(String expectedContactSummary, String expectedContactFullDescription,
            View summaryView, View fullView) {
        onView(allOf(withId(R.id.contact_summary), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedContactSummary)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(summaryView))))
                .check(matches(not(isDisplayed())));

        onView(allOf(withId(R.id.contact_full), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedContactFullDescription)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(fullView))))
                .check(matches(not(isDisplayed())));
    }

    private void testPaymentMethod(String expectedObfuscatedCardNumber, String expectedCardName,
            String expectedCardExpiration, View summaryView, View fullView) {
        onView(allOf(withId(R.id.credit_card_number), isDescendantOfA(is(summaryView))))
                .check(matches(withText(containsString(expectedObfuscatedCardNumber))));
        onView(allOf(withId(R.id.credit_card_expiration), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedCardExpiration)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(summaryView))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.credit_card_name), isDescendantOfA(is(summaryView))))
                .check(doesNotExist());

        onView(allOf(withId(R.id.credit_card_number), isDescendantOfA(is(fullView))))
                .check(matches(withText(containsString(expectedObfuscatedCardNumber))));
        onView(allOf(withId(R.id.credit_card_expiration), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedCardExpiration)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(fullView))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.credit_card_name), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedCardName)));
    }

    private void testShippingAddress(String expectedFullName, String expectedShortAddress,
            String expectedFullAddress, View summaryView, View fullView) {
        onView(allOf(withId(R.id.full_name), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedFullName)));
        onView(allOf(withId(R.id.short_address), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedShortAddress)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(summaryView))))
                .check(matches(not(isDisplayed())));

        onView(allOf(withId(R.id.full_name), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedFullName)));
        onView(allOf(withId(R.id.full_address), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedFullAddress)));
        onView(allOf(withId(R.id.incomplete_error), isDescendantOfA(is(fullView))))
                .check(matches(not(isDisplayed())));
    }

    private void testLoginDetails(
            String expectedLabel, String expectedSublabel, View summaryView, View fullView) {
        onView(allOf(withId(R.id.label), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedLabel)));
        onView(allOf(withId(R.id.label), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedLabel)));
        onView(allOf(withId(R.id.sublabel), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedSublabel)));
    }
}
