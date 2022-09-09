// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.assertion.PositionAssertions.isAbove;
import static androidx.test.espresso.assertion.PositionAssertions.isBelow;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.empty;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.lessThan;
import static org.hamcrest.Matchers.not;
import static org.hamcrest.Matchers.nullValue;
import static org.hamcrest.core.AllOf.allOf;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertThat;

import static org.chromium.components.autofill_assistant.AssistantTagsForTesting.COLLECT_USER_DATA_CHOICE_LIST;
import static org.chromium.components.autofill_assistant.AssistantTagsForTesting.COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG;
import static org.chromium.components.autofill_assistant.AssistantTagsForTesting.COLLECT_USER_DATA_TERMS_REQUIRE_REVIEW;
import static org.chromium.components.autofill_assistant.AssistantTagsForTesting.VERTICAL_EXPANDER_CHEVRON;

import android.support.test.InstrumentationRegistry;
import android.view.View;
import android.widget.RadioButton;
import android.widget.TextView;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantCollectUserDataTestHelper.ViewHolder;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.customtabs.CustomTabsIntentTestUtils;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.AssistantAutofillCreditCard;
import org.chromium.components.autofill_assistant.AssistantAutofillProfile;
import org.chromium.components.autofill_assistant.AssistantDialogButton;
import org.chromium.components.autofill_assistant.AssistantInfoPopup;
import org.chromium.components.autofill_assistant.AssistantOptionModel.AddressModel;
import org.chromium.components.autofill_assistant.AssistantOptionModel.ContactModel;
import org.chromium.components.autofill_assistant.AssistantOptionModel.PaymentInstrumentModel;
import org.chromium.components.autofill_assistant.AssistantPaymentInstrument;
import org.chromium.components.autofill_assistant.AssistantStaticDependencies;
import org.chromium.components.autofill_assistant.R;
import org.chromium.components.autofill_assistant.generic_ui.AssistantValue;
import org.chromium.components.autofill_assistant.user_data.AssistantChoiceList;
import org.chromium.components.autofill_assistant.user_data.AssistantCollectUserDataCoordinator;
import org.chromium.components.autofill_assistant.user_data.AssistantCollectUserDataModel;
import org.chromium.components.autofill_assistant.user_data.AssistantCollectUserDataModel.DataOriginNoticeConfiguration;
import org.chromium.components.autofill_assistant.user_data.AssistantCollectUserDataModel.LoginChoiceModel;
import org.chromium.components.autofill_assistant.user_data.AssistantContactField;
import org.chromium.components.autofill_assistant.user_data.AssistantLoginChoice;
import org.chromium.components.autofill_assistant.user_data.AssistantTermsAndConditionsState;
import org.chromium.components.autofill_assistant.user_data.additional_sections.AssistantAdditionalSectionFactory;
import org.chromium.components.autofill_assistant.user_data.additional_sections.AssistantStaticTextSection;
import org.chromium.components.autofill_assistant.user_data.additional_sections.AssistantTextInputSection;
import org.chromium.components.autofill_assistant.user_data.additional_sections.AssistantTextInputSection.TextInputFactory;
import org.chromium.components.autofill_assistant.user_data.additional_sections.AssistantTextInputType;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * Tests for the Autofill Assistant collect user data UI.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantCollectUserDataUiTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private AutofillAssistantCollectUserDataTestHelper mHelper;
    AssistantCollectUserDataModel.ContactDescriptionOptions mDefaultContactSummaryOptions;
    AssistantCollectUserDataModel.ContactDescriptionOptions mDefaultContactFullOptions;

    @Before
    public void setUp() throws Exception {
        mTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        InstrumentationRegistry.getTargetContext(), "about:blank"));
        mHelper = new AutofillAssistantCollectUserDataTestHelper();

        mDefaultContactSummaryOptions =
                new AssistantCollectUserDataModel.ContactDescriptionOptions();
        mDefaultContactSummaryOptions.mFields =
                new int[] {AssistantContactField.EMAIL_ADDRESS, AssistantContactField.NAME_FULL};
        mDefaultContactSummaryOptions.mMaxNumberLines = 1;

        mDefaultContactFullOptions = new AssistantCollectUserDataModel.ContactDescriptionOptions();
        mDefaultContactFullOptions.mFields =
                new int[] {AssistantContactField.NAME_FULL, AssistantContactField.EMAIL_ADDRESS};
        mDefaultContactFullOptions.mMaxNumberLines = 2;
    }

    private AssistantCollectUserDataModel createCollectUserDataModel() {
        return TestThreadUtils.runOnUiThreadBlockingNoException(AssistantCollectUserDataModel::new);
    }

    /** Creates a coordinator for use in UI tests, and adds it to the global view hierarchy. */
    private AssistantCollectUserDataCoordinator createCollectUserDataCoordinator(
            AssistantCollectUserDataModel model) throws Exception {
        AssistantStaticDependencies staticDependencies = new AssistantStaticDependenciesChrome();
        AssistantCollectUserDataCoordinator coordinator = TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> new AssistantCollectUserDataCoordinator(mTestRule.getActivity(), model,
                                staticDependencies.createEditorFactory(),
                                staticDependencies.createDependencies(mTestRule.getActivity())
                                        .getWindowAndroid()));

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
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);

        /* Test initial model state. */
        assertThat(model.get(AssistantCollectUserDataModel.VISIBLE), is(false));
        assertThat(model.get(AssistantCollectUserDataModel.AVAILABLE_CONTACTS), empty());
        assertThat(model.get(AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES), empty());
        assertThat(model.get(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS), empty());
        assertThat(model.get(AssistantCollectUserDataModel.SUPPORTED_BASIC_CARD_NETWORKS), empty());
        assertThat(model.get(AssistantCollectUserDataModel.EXPANDED_SECTION), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.DELEGATE), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.WEB_CONTENTS), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS), nullValue());
        assertThat(
                model.get(AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT), nullValue());
        assertThat(model.get(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS), nullValue());
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
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        // Set WebContents such that editors can be built.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCollectUserDataModel.WEB_CONTENTS,
                                mTestRule.getWebContents()));

        // Initially, everything is invisible.
        onView(is(coordinator.getView())).check(matches(not(isDisplayed())));

        // User data block is visible, but no section was requested: all sections should be
        // invisible.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.VISIBLE, true));
        onView(is(coordinator.getView())).check(matches(isDisplayed()));
        onView(is(viewHolder.mContactSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));

        // Contact details should be visible if either name, phone, or email is requested.
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

        // Payment method section visibility test.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true));
        onView(is(viewHolder.mPaymentSection)).check(matches(isDisplayed()));

        // Shipping address visibility test.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true));
        onView(is(viewHolder.mShippingSection)).check(matches(isDisplayed()));

        // Login section visibility test.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true));
        onView(is(viewHolder.mLoginsSection)).check(matches(not(isDisplayed())));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS,
                                Collections.singletonList(new AssistantLoginChoice("id",
                                        /* label= */ "Guest", /* sublabel= */ "Description",
                                        /* sublabelAccessibilityHint= */ "", /* priority= */ 0,
                                        /* infoPopup= */ null,
                                        /* editButtonContentDescription= */ ""))));
        onView(is(viewHolder.mLoginsSection)).check(matches(isDisplayed()));

        // Prepended section visibility test.
        List<AssistantAdditionalSectionFactory> prependedSections = new ArrayList<>();
        prependedSections.add(
                new AssistantStaticTextSection.Factory("Prepended section", "Lorem ipsum."));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCollectUserDataModel.PREPENDED_SECTIONS,
                                prependedSections));
        // Login section is top-most regular section, the prepended section should be above that.
        onView(withText("Prepended section")).check(isAbove(is(viewHolder.mLoginsSection)));

        // Appended section visibility test.
        List<AssistantAdditionalSectionFactory> appendedSections = new ArrayList<>();
        appendedSections.add(
                new AssistantStaticTextSection.Factory("Appended section", "Lorem ipsum."));
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.APPENDED_SECTIONS, appendedSections));
        // Shipping address is bottom-most regular section, the appended section should be below
        // that.
        onView(withText("Appended section")).check(isBelow(is(viewHolder.mShippingSection)));

        // Sections without items and without editor should be hidden. Removing WebContents will
        // clear the editors.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.WEB_CONTENTS, null));
        onView(is(viewHolder.mContactSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mPaymentSection)).check(matches(not(isDisplayed())));
        onView(is(viewHolder.mShippingSection)).check(matches(not(isDisplayed())));
    }

    /**
     * Test assumptions about a payment request for a case where the personal data manager does not
     * contain any profiles or payment methods, i.e., all PR sections should be empty.
     */
    @Test
    @MediumTest
    public void testEmptyPaymentRequest() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        // Request all PR sections.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        // Without WebContents, there are no editors and the 'add' buttons should be removed.
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

        // Add WebContents to set editors.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
        });

        // Empty sections should display the 'add' button in their title.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(isDisplayed()));
        // ... Except for logins and additional sections, which currently do not support
        // adding items.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mLoginsSection))))
                .check(matches(not(isDisplayed())));

        // Empty sections should be 'fixed', i.e., they can not be expanded.
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));

        // Empty sections are collapsed.
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_CHOICE_LIST)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isDisplayed())));

        // Empty sections should be empty.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            assertThat(viewHolder.mContactList.getItemCount(), is(0));
            assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(0));
            assertThat(viewHolder.mShippingAddressList.getItemCount(), is(0));
            assertThat(viewHolder.mLoginList.getItemCount(), is(0));
        });

        // Test delegate status.
        assertThat(delegate.mPaymentInstrument, nullValue());
        assertThat(delegate.mContact, nullValue());
        assertThat(delegate.mShippingAddress, nullValue());
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
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // WEB_CONTENTS are necessary for the creation of the editors.
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS,
                    mDefaultContactSummaryOptions);
            model.set(AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS,
                    mDefaultContactFullOptions);
            model.set(AssistantCollectUserDataModel.AVAILABLE_CONTACTS, Collections.emptyList());
            model.set(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS, null);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        // Contact details section should be empty.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(isDisplayed()));
        assertThat(viewHolder.mContactList.getItemCount(), is(0));

        // Add profile to the list and send the updated model.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantAutofillProfile contact = createDummyContact("John Doe", "john@gmail.com");
            model.set(AssistantCollectUserDataModel.AVAILABLE_CONTACTS,
                    Collections.singletonList(new ContactModel(contact)));
            model.set(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS,
                    new ContactModel(contact));
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
            model.set(AssistantCollectUserDataModel.AVAILABLE_CONTACTS, Collections.emptyList());
            model.set(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS, null);
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
     * Shows a payment request, then pushes a new payment method from the controller.
     * Tests whether the new payment method is added to the payment request.
     */
    @Test
    @MediumTest
    public void testPaymentMethodsUpdates() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // WEB_CONTENTS are necessary for the creation of the editors.
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS,
                    Collections.emptyList());
            model.set(AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT, null);
        });

        // Payment method section should be empty and show the 'add' button in the title.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(0));

        // Add profile to the personal data manager and push new card.
        PersonalDataManager.AutofillProfile billingAddress =
                mHelper.createDummyProfile("Jill Doe", "jill@gmail.com");
        String billingAddressId = mHelper.setProfile(billingAddress);
        PersonalDataManager.CreditCard creditCard = mHelper.createDummyCreditCard(billingAddressId);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantPaymentInstrument paymentInstrument =
                    AssistantCollectUserDataModel.createAssistantPaymentInstrument(
                            createDummyCreditCard(creditCard), createDummyAddress(billingAddress));
            model.set(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS,
                    Collections.singletonList(new PaymentInstrumentModel(paymentInstrument)));
            model.set(AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT,
                    new PaymentInstrumentModel(paymentInstrument));
        });

        // Payment method section contains the new credit card, which should be pre-selected.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isDisplayed())));
        assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(1));
        onView(allOf(withId(R.id.credit_card_name),
                       isDescendantOfA(is(viewHolder.mPaymentMethodList.getItem(0)))))
                .check(matches(withText("Jill Doe")));

        // Remove credit card from the list. Section should be empty again.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS,
                    Collections.emptyList());
            model.set(AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT, null);
        });
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(isDisplayed()));
        assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(0));

        // Tap the 'add' button to open the editor, to make sure that it still works.
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
        // Add complete profile and credit card to the personal data manager.
        PersonalDataManager.AutofillProfile profile = new PersonalDataManager.AutofillProfile(
                "GUID", "https://www.example.com", /* honorificPrefix= */ "", "Maggie Simpson",
                "Acme Inc.", "123 Main", "California", "Los Angeles", "", "90210", "", "UZ",
                "555 123-4567", "maggie@simpson.com", "");
        PersonalDataManager.CreditCard creditCard =
                new PersonalDataManager.CreditCard("", "https://example.com", true, true, "Jon Doe",
                        "4111111111111111", "1111", "12", "2050", "visa", R.drawable.visa_card,
                        /* billingAddressId= */ "GUID", /* serverId= */ "");

        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        // Request all PR sections.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY, true);
            model.set(AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS,
                    mDefaultContactSummaryOptions);
            model.set(AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS,
                    mDefaultContactFullOptions);
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true);
            AssistantAutofillProfile contact = createDummyContact(profile);
            model.set(AssistantCollectUserDataModel.AVAILABLE_CONTACTS,
                    Collections.singletonList(new ContactModel(contact)));
            model.set(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS,
                    new ContactModel(contact));
            AssistantAutofillProfile phone_number = createDummyContact(profile);
            model.set(AssistantCollectUserDataModel.AVAILABLE_PHONE_NUMBERS,
                    Collections.singletonList(new ContactModel(phone_number)));
            model.set(
                    AssistantCollectUserDataModel.SELECTED_PHONE_NUMBER, new ContactModel(contact));
            model.set(AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES,
                    Collections.singletonList(createAddressModel(profile)));
            model.set(AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS,
                    createAddressModel(profile));
            AssistantPaymentInstrument paymentInstrument =
                    AssistantCollectUserDataModel.createAssistantPaymentInstrument(
                            createDummyCreditCard(creditCard), createDummyAddress(profile));
            model.set(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS,
                    Collections.singletonList(new PaymentInstrumentModel(paymentInstrument)));
            model.set(AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT,
                    new PaymentInstrumentModel(paymentInstrument));
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.SELECTED_LOGIN,
                    new LoginChoiceModel(new AssistantLoginChoice(
                            "id", "Guest", "Description of guest checkout", "", 0, null, "")));
        });

        // Non-empty sections should not display the 'add' button in their title.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isDisplayed())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
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

        // Non-empty sections should not be 'fixed', i.e., they can be expanded.
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        onView(allOf(withTagValue(is(VERTICAL_EXPANDER_CHEVRON)),
                       isDescendantOfA(is(viewHolder.mLoginsSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));

        // All section dividers are visible.
        for (View divider : viewHolder.mDividers) {
            onView(is(divider)).check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        }

        // Check contents of sections.
        assertThat(viewHolder.mContactList.getItemCount(), is(1));
        assertThat(viewHolder.mPhoneNumberList.getItemCount(), is(1));
        assertThat(viewHolder.mPaymentMethodList.getItemCount(), is(1));
        assertThat(viewHolder.mShippingAddressList.getItemCount(), is(1));
        assertThat(viewHolder.mLoginList.getItemCount(), is(1));

        testContact("maggie@simpson.com", "Maggie Simpson\nmaggie@simpson.com",
                viewHolder.mContactSection.getCollapsedView(), viewHolder.mContactList.getItem(0),
                /* isComplete = */ true);
        testContact("555 123-4567", "555 123-4567",
                viewHolder.mPhoneNumberSection.getCollapsedView(),
                viewHolder.mPhoneNumberList.getItem(0), /* isComplete= */ true);
        testPaymentMethod("1111", "Jon Doe", "12/2050",
                viewHolder.mPaymentSection.getCollapsedView(),
                viewHolder.mPaymentMethodList.getItem(0));
        testShippingAddress("Maggie Simpson", "Acme Inc., 123 Main, 90210 Los Angeles, California",
                "Acme Inc., 123 Main, 90210 Los Angeles, California, Uzbekistan",
                viewHolder.mShippingSection.getCollapsedView(),
                viewHolder.mShippingAddressList.getItem(0));
        testLoginDetails("Guest", "Description of guest checkout",
                viewHolder.mLoginsSection.getCollapsedView(), viewHolder.mLoginList.getItem(0));

        // Check delegate status. The selections set in the model have not been sent to the
        // delegate. |setItems()| has been called first (without selection) and selecting the item
        // does not trigger a notification.
        assertThat(delegate.mPaymentInstrument, is(nullValue()));
        assertThat(delegate.mContact, is(nullValue()));
        assertThat(delegate.mPhoneNumber, is(nullValue()));
        assertThat(delegate.mShippingAddress, is(nullValue()));
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));
        assertThat(delegate.mLoginChoice, is(nullValue()));

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            AssistantAutofillProfile contact = createDummyContact(profile);
            model.set(AssistantCollectUserDataModel.AVAILABLE_CONTACTS,
                    Collections.singletonList(new ContactModel(contact)));
            model.set(AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES,
                    Collections.singletonList(createAddressModel(profile)));
            AssistantPaymentInstrument paymentInstrument =
                    AssistantCollectUserDataModel.createAssistantPaymentInstrument(
                            createDummyCreditCard(creditCard), createDummyAddress(profile));
            model.set(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS,
                    Collections.singletonList(new PaymentInstrumentModel(paymentInstrument)));
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS,
                    Collections.singletonList(new AssistantLoginChoice(
                            "id", "Guest", "Description of guest checkout", "", 0, null, "")));
        });

        // Check delegate status. Setting items again will not send a notification to the delegate.
        assertThat(delegate.mPaymentInstrument, is(nullValue()));
        assertThat(delegate.mContact, is(nullValue()));
        assertThat(delegate.mPhoneNumber, is(nullValue()));
        assertThat(delegate.mShippingAddress, is(nullValue()));
        assertThat(delegate.mTermsStatus, is(AssistantTermsAndConditionsState.NOT_SELECTED));
        assertThat(delegate.mLoginChoice, is(nullValue()));
    }

    /**
     * Test that disabling the CollectUserData UI properly disables all the required elements.
     */
    @Test
    @MediumTest
    public void testDisabledState() throws Exception {
        PersonalDataManager.AutofillProfile profileJohn = new PersonalDataManager.AutofillProfile(
                /* guid= */ "john", "https://www.example.com", /* honorificPrefix= */ "",
                "John Doe",
                /* companyName= */ "", "123 Main", "California", "Los Angeles",
                /* dependentLocality= */ "", "90210", /* sortingCode= */ "", "US", "555 123-4567",
                "johndoe@google.com", /* languageCode= */ "");
        PersonalDataManager.CreditCard creditCardJohn =
                new PersonalDataManager.CreditCard(/* guid= */ "john", "https://example.com",
                        /* isLocal= */ true, /* isCached= */ true, "John Doe", "4111111111111111",
                        "1111", "12", "2050", "visa", R.drawable.visa_card,
                        /* billingAddressId= */ "john", /* serverId= */ "");

        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        // Disable the UI.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.ENABLE_UI_INTERACTIONS, false));

        // Request all PR sections but leave them empty.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS,
                    mDefaultContactSummaryOptions);
            model.set(AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS,
                    mDefaultContactFullOptions);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
        });

        // Empty sections should have their title add button disabled.
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withId(R.id.section_title_add_button),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isEnabled())));
        onView(is(viewHolder.mLoginsSection)).check(matches(not(isDisplayed())));

        // Fill all PR sections.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            List<ContactModel> contacts = new ArrayList<>();
            contacts.add(new ContactModel(createDummyContact(profileJohn)));
            model.set(AssistantCollectUserDataModel.AVAILABLE_CONTACTS, contacts);
            model.set(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS, contacts.get(0));
            model.set(AssistantCollectUserDataModel.AVAILABLE_PHONE_NUMBERS, contacts);
            model.set(AssistantCollectUserDataModel.SELECTED_PHONE_NUMBER, contacts.get(0));
            List<AddressModel> addresses = new ArrayList<>();
            addresses.add(new AddressModel(createDummyAddress(profileJohn),
                    /* fullDescription= */ "John", /* summaryDescription= */ "John"));
            model.set(AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES, addresses);
            model.set(AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS, addresses.get(0));
            List<PaymentInstrumentModel> instruments = new ArrayList<>();
            instruments.add(new PaymentInstrumentModel(
                    AssistantCollectUserDataModel.createAssistantPaymentInstrument(
                            createDummyCreditCard(creditCardJohn),
                            createDummyAddress(profileJohn))));
            model.set(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS, instruments);
            model.set(
                    AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT, instruments.get(0));
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            List<AssistantLoginChoice> logins = new ArrayList<>();
            logins.add(new AssistantLoginChoice(/* identifier= */ "john", /* label= */ "John Doe",
                    /* sublabel= */ "", /* sublabelAccessibilityHint= */ "", /* priority= */ 0,
                    /* infoPopup= */ null, /* editButtonContentDescription= */ ""));
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS, logins);
            model.set(AssistantCollectUserDataModel.SELECTED_LOGIN,
                    new LoginChoiceModel(logins.get(0)));
        });

        // Radio buttons, edit icons and the add button should be disabled.
        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withTagValue(is(AssistantChoiceList.EDIT_BUTTON_TAG)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withTagValue(is(AssistantChoiceList.ADD_BUTTON_TAG)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withTagValue(is(AssistantChoiceList.EDIT_BUTTON_TAG)),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withTagValue(is(AssistantChoiceList.ADD_BUTTON_TAG)),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withTagValue(is(AssistantChoiceList.EDIT_BUTTON_TAG)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withTagValue(is(AssistantChoiceList.ADD_BUTTON_TAG)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withTagValue(is(AssistantChoiceList.EDIT_BUTTON_TAG)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withTagValue(is(AssistantChoiceList.ADD_BUTTON_TAG)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(not(isEnabled())));
        onView(allOf(withClassName(is(RadioButton.class.getName())),
                       isDescendantOfA(is(viewHolder.mLoginsSection))))
                .check(matches(not(isEnabled())));
    }

    /**
     * Test that marking the CollectUserData UI as loading behaves as expected.
     */
    @Test
    @MediumTest
    public void testLoadingState() throws Exception {
        PersonalDataManager.AutofillProfile profileJohn = new PersonalDataManager.AutofillProfile(
                /* guid= */ "john", "https://www.example.com", /* honorificPrefix= */ "",
                "John Doe",
                /* companyName= */ "", "123 Main", "California", "Los Angeles",
                /* dependentLocality= */ "", "90210", /* sortingCode= */ "", "US", "555 123-4567",
                "johndoe@google.com", /* languageCode= */ "");
        PersonalDataManager.CreditCard creditCardJohn =
                new PersonalDataManager.CreditCard(/* guid= */ "john", "https://example.com",
                        /* isLocal= */ true, /* isCached= */ true, "John Doe", "4111111111111111",
                        "1111", "12", "2050", "visa", R.drawable.visa_card,
                        /* billingAddressId= */ "john", /* serverId= */ "");

        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        // Request all PR sections and fill them with data.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS,
                    mDefaultContactSummaryOptions);
            model.set(AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS,
                    mDefaultContactFullOptions);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE_NUMBER_SEPARATELY, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PAYMENT, true);
            model.set(AssistantCollectUserDataModel.REQUEST_SHIPPING_ADDRESS, true);
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            List<ContactModel> contacts = new ArrayList<>();
            contacts.add(new ContactModel(createDummyContact(profileJohn)));
            model.set(AssistantCollectUserDataModel.AVAILABLE_CONTACTS, contacts);
            model.set(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS, contacts.get(0));
            model.set(AssistantCollectUserDataModel.AVAILABLE_PHONE_NUMBERS, contacts);
            model.set(AssistantCollectUserDataModel.SELECTED_PHONE_NUMBER, contacts.get(0));
            List<AddressModel> addresses = new ArrayList<>();
            addresses.add(new AddressModel(createDummyAddress(profileJohn),
                    /* fullDescription= */ "John", /* summaryDescription= */ "John"));
            model.set(AssistantCollectUserDataModel.AVAILABLE_SHIPPING_ADDRESSES, addresses);
            model.set(AssistantCollectUserDataModel.SELECTED_SHIPPING_ADDRESS, addresses.get(0));
            List<PaymentInstrumentModel> instruments = new ArrayList<>();
            instruments.add(new PaymentInstrumentModel(
                    AssistantCollectUserDataModel.createAssistantPaymentInstrument(
                            createDummyCreditCard(creditCardJohn),
                            createDummyAddress(profileJohn))));
            model.set(AssistantCollectUserDataModel.AVAILABLE_PAYMENT_INSTRUMENTS, instruments);
            model.set(
                    AssistantCollectUserDataModel.SELECTED_PAYMENT_INSTRUMENT, instruments.get(0));
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            List<AssistantLoginChoice> logins = new ArrayList<>();
            logins.add(new AssistantLoginChoice(/* identifier= */ "john", /* label= */ "John Doe",
                    /* sublabel= */ "", /* sublabelAccessibilityHint= */ "", /* priority= */ 0,
                    /* infoPopup= */ null, /* editButtonContentDescription= */ ""));
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS, logins);
            model.set(AssistantCollectUserDataModel.SELECTED_LOGIN,
                    new LoginChoiceModel(logins.get(0)));
        });

        // All sections should show the summary.
        onView(allOf(withId(R.id.contact_summary), isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        onView(allOf(withId(R.id.contact_summary),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG)),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        onView(allOf(withId(R.id.payment_method_summary),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        onView(allOf(withId(R.id.address_summary),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));

        // Load contacts.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> model.set(AssistantCollectUserDataModel.MARK_CONTACTS_LOADING, true));

        onView(allOf(withId(R.id.contact_summary), isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG)),
                       isDescendantOfA(is(viewHolder.mContactSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));

        // Load phone numbers.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.MARK_CONTACTS_LOADING, false);
            model.set(AssistantCollectUserDataModel.MARK_PHONE_NUMBERS_LOADING, true);
        });

        onView(allOf(withId(R.id.contact_summary),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG)),
                       isDescendantOfA(is(viewHolder.mPhoneNumberSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));

        // Load payment methods.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.MARK_PHONE_NUMBERS_LOADING, false);
            model.set(AssistantCollectUserDataModel.MARK_PAYMENT_METHODS_LOADING, true);
        });

        onView(allOf(withId(R.id.payment_method_summary),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG)),
                       isDescendantOfA(is(viewHolder.mPaymentSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));

        // Load shipping addresses.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.MARK_PAYMENT_METHODS_LOADING, false);
            model.set(AssistantCollectUserDataModel.MARK_SHIPPING_ADDRESSES_LOADING, true);
        });

        onView(allOf(withId(R.id.address_summary),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        onView(allOf(withTagValue(is(COLLECT_USER_DATA_SUMMARY_LOADING_SPINNER_TAG)),
                       isDescendantOfA(is(viewHolder.mShippingSection))))
                .check(matches(withEffectiveVisibility(Visibility.VISIBLE)));
    }

    /** Tests custom summary options for the contact details section. */
    @Test
    @MediumTest
    public void testContactDetailsCustomSummary() throws Exception {
        AssistantAutofillProfile contactFull =
                createDummyContact("Maggie Simpson", "maggie@simpson.com", "555 123-4567");
        AssistantAutofillProfile contactWithoutEmail =
                createDummyContact("John Simpson", /* email= */ "", "555 123-4567");

        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        AssistantCollectUserDataModel.ContactDescriptionOptions summaryOptions =
                new AssistantCollectUserDataModel.ContactDescriptionOptions();
        summaryOptions.mFields = new int[] {AssistantContactField.NAME_FULL,
                AssistantContactField.EMAIL_ADDRESS, AssistantContactField.PHONE_HOME_WHOLE_NUMBER};
        summaryOptions.mMaxNumberLines = 3;

        AssistantCollectUserDataModel.ContactDescriptionOptions fullOptions =
                new AssistantCollectUserDataModel.ContactDescriptionOptions();
        fullOptions.mFields = new int[] {AssistantContactField.NAME_FULL,
                AssistantContactField.PHONE_HOME_WHOLE_NUMBER, AssistantContactField.EMAIL_ADDRESS};
        fullOptions.mMaxNumberLines = 3;

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.WEB_CONTENTS, mTestRule.getWebContents());
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.REQUEST_NAME, true);
            model.set(AssistantCollectUserDataModel.REQUEST_PHONE, true);
            model.set(AssistantCollectUserDataModel.REQUEST_EMAIL, true);
            model.set(AssistantCollectUserDataModel.CONTACT_SUMMARY_DESCRIPTION_OPTIONS,
                    summaryOptions);
            model.set(AssistantCollectUserDataModel.CONTACT_FULL_DESCRIPTION_OPTIONS, fullOptions);
            List<ContactModel> contacts = new ArrayList<>();
            contacts.add(new ContactModel(contactFull));
            contacts.add(new ContactModel(contactWithoutEmail));
            model.set(AssistantCollectUserDataModel.AVAILABLE_CONTACTS, contacts);
            model.set(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS,
                    new ContactModel(contactFull));
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        testContact("Maggie Simpson\nmaggie@simpson.com\n555 123-4567",
                "Maggie Simpson\n555 123-4567\nmaggie@simpson.com",
                viewHolder.mContactSection.getCollapsedView(), viewHolder.mContactList.getItem(0),
                /* isComplete= */ true);

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> model.set(AssistantCollectUserDataModel.SELECTED_CONTACT_DETAILS,
                                new ContactModel(contactWithoutEmail,
                                        Collections.singletonList("Missing email"),
                                        /* canEdit= */ true)));

        testContact("John Simpson\n555 123-4567", "John Simpson\n555 123-4567",
                viewHolder.mContactSection.getCollapsedView(), viewHolder.mContactList.getItem(0),
                /* isComplete= */ false);
    }

    @Test
    @MediumTest
    public void testTermsAndConditions() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
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
        AssistantCollectUserDataModel model = createCollectUserDataModel();
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
    public void testInfoSectionText() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        // Setting a text from "backend".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.INFO_SECTION_TEXT, "Info section text.");
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });
        onView(is(viewHolder.mInfoSection))
                .check(matches(allOf(withText("Info section text."), isDisplayed())));

        // Set the text back to empty, which should remove the text section.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { model.set(AssistantCollectUserDataModel.INFO_SECTION_TEXT, ""); });
        onView(is(viewHolder.mInfoSection)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    public void testPrivacyNotice() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        TextView privacyNotice =
                viewHolder.mTermsSection.findViewById(R.id.collect_data_privacy_notice);

        // Setting a text from "backend".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(
                    AssistantCollectUserDataModel.PRIVACY_NOTICE_TEXT, "Thirdparty privacy notice");
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        onView(is(privacyNotice))
                .check(matches(allOf(withText("Thirdparty privacy notice"), isDisplayed())));
    }

    @Test
    @MediumTest
    public void testDataOriginNotice() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));

        // Not visible initially.
        onView(is(viewHolder.mDataOriginNotice))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));

        // Setting a text from "backend".
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DATA_ORIGIN_NOTICE_CONFIGURATION,
                    new DataOriginNoticeConfiguration(/* linkText= */ "About this data",
                            /* title= */ "About your personal information",
                            /* text= */
                            "This is some text describing the <link2>user's data</link2> info.",
                            /* buttonText= */ "Got it"));
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
        });

        onView(is(withId(R.id.link_to_data_origin_dialog)))
                .check(matches(allOf(withText("About this data"), isDisplayed())));
        onView(withText("About this data")).perform(click());
        onView(withText("This is some text describing the user's data info."))
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testAdditionalStaticSections() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
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

        // Unset additional sections and check that the number of dividers has decreased (see
        // b/152500114).
        AutofillAssistantCollectUserDataTestHelper
                .ViewHolder viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));
        int numDividers = viewHolder.mDividers.size();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.PREPENDED_SECTIONS, Collections.emptyList());
            model.set(AssistantCollectUserDataModel.APPENDED_SECTIONS, Collections.emptyList());
        });

        viewHolder = TestThreadUtils.runOnUiThreadBlocking(
                () -> new AutofillAssistantCollectUserDataTestHelper.ViewHolder(coordinator));
        assertThat(viewHolder.mDividers.size(), lessThan(numDividers));
    }

    @Test
    @MediumTest
    public void testAdditionalTextInputSections() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
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
        assertEquals(delegate.mAdditionalValues.get("discount"),
                new AssistantValue(new String[] {"123456789"}));
        assertEquals(
                delegate.mAdditionalValues.get("loyalty"), new AssistantValue(new String[] {""}));

        onView(withContentDescription("Discount code")).perform(replaceText("D-742394"));
        onView(withContentDescription("Loyalty code")).perform(replaceText("L-394834"));
        assertEquals(delegate.mAdditionalValues.get("discount"),
                new AssistantValue(new String[] {"D-742394"}));
        assertEquals(delegate.mAdditionalValues.get("loyalty"),
                new AssistantValue(new String[] {"L-394834"}));
    }

    @Test
    @MediumTest
    public void testLoginSectionInfoPopup() throws Exception {
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();

        AssistantInfoPopup infoPopup = new AssistantInfoPopup("Guest checkout", "Text explanation.",
                new AssistantDialogButton(null, "Close", null), null, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            model.set(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE, "Login options");
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS,
                    Collections.singletonList(new AssistantLoginChoice("id", "Guest checkout", "",
                            "", 0, infoPopup, "Description of edit button")));
        });

        onView(withText("Login options")).perform(click());
        onView(withContentDescription("Description of edit button")).perform(click());
        onView(withText("Guest checkout")).check(matches(isDisplayed()));
        onView(withText("Text explanation.")).check(matches(isDisplayed()));
        onView(withText(mTestRule.getActivity().getString(R.string.close))).perform(click());
        onView(withContentDescription("Description of edit button")).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSuppliedNonEmptyEditContentDescriptionIsUsed() throws Exception {
        String contentDescription = "Description of edit button";
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();

        AssistantInfoPopup infoPopup = new AssistantInfoPopup("", "", null, null, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            model.set(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE, "Login options");
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS,
                    Collections.singletonList(new AssistantLoginChoice(
                            "id", "Guest checkout", "", "", 0, infoPopup, contentDescription)));
        });

        onView(withText("Login options")).perform(click());
        onView(withContentDescription(contentDescription)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testSuppliedEmptyEditContentDescriptionIsUsed() throws Exception {
        String contentDescription = "";
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();

        AssistantInfoPopup infoPopup = new AssistantInfoPopup("", "", null, null, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            model.set(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE, "Login options");
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS,
                    Collections.singletonList(new AssistantLoginChoice(
                            "id", "Guest checkout", "", "", 0, infoPopup, contentDescription)));
        });

        onView(withText("Login options")).perform(click());
        onView(withContentDescription(contentDescription)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void testWhenNullEditContentDescriptionIsSuppliedIconDescriptionIsUsed()
            throws Exception {
        String contentDescription = null;
        AssistantCollectUserDataModel model = createCollectUserDataModel();
        AssistantCollectUserDataCoordinator coordinator = createCollectUserDataCoordinator(model);
        AutofillAssistantCollectUserDataTestHelper.MockDelegate delegate =
                new AutofillAssistantCollectUserDataTestHelper.MockDelegate();

        AssistantInfoPopup infoPopup = new AssistantInfoPopup("", "", null, null, null);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            model.set(AssistantCollectUserDataModel.DELEGATE, delegate);
            model.set(AssistantCollectUserDataModel.VISIBLE, true);
            model.set(AssistantCollectUserDataModel.LOGIN_SECTION_TITLE, "Login options");
            model.set(AssistantCollectUserDataModel.REQUEST_LOGIN_CHOICE, true);
            model.set(AssistantCollectUserDataModel.AVAILABLE_LOGINS,
                    Collections.singletonList(new AssistantLoginChoice(
                            "id", "Guest checkout", "", "", 0, infoPopup, contentDescription)));
        });

        onView(withText("Login options")).perform(click());
        onView(withContentDescription(mTestRule.getActivity().getString(R.string.learn_more)))
                .check(matches(isDisplayed()));
    }

    private View getPaymentSummaryErrorView(ViewHolder viewHolder) {
        return viewHolder.mPaymentSection.findViewById(R.id.payment_method_summary)
                .findViewById(R.id.incomplete_error);
    }

    private void testContact(String expectedContactSummary, String expectedContactFullDescription,
            View summaryView, View fullView, boolean isComplete) {
        onView(allOf(withId(R.id.contact_summary), isDescendantOfA(is(summaryView))))
                .check(matches(withText(expectedContactSummary)));
        assertThat(summaryView.findViewById(R.id.incomplete_error).getVisibility(),
                is(isComplete ? View.GONE : View.VISIBLE));

        onView(allOf(withId(R.id.contact_full), isDescendantOfA(is(fullView))))
                .check(matches(withText(expectedContactFullDescription)));
        assertThat(fullView.findViewById(R.id.incomplete_error).getVisibility(),
                is(isComplete ? View.GONE : View.VISIBLE));
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

    private AssistantAutofillProfile createDummyContact(String name, String email) {
        return createDummyContact(name, email, /* phone= */ "");
    }

    private AssistantAutofillProfile createDummyContact(
            PersonalDataManager.AutofillProfile profile) {
        return createDummyContact(
                profile.getFullName(), profile.getEmailAddress(), profile.getPhoneNumber());
    }

    private AssistantAutofillProfile createDummyContact(String name, String email, String phone) {
        return new AssistantAutofillProfile(/* guid= */ "", /* origin= */ "", /* isLocal= */ true,
                /* honorificPrefix= */ "", name,
                /* companyName= */ "", /* streetAddress= */ "", /* region= */ "",
                /* locality= */ "",
                /* dependentLocality= */ "", /* postalCode= */ "", /* sortingCode= */ "",
                /* countryCode= */ "", phone, email, /* languageCode= */ "en-US");
    }

    private AssistantAutofillProfile createDummyAddress(
            PersonalDataManager.AutofillProfile profile) {
        return new AssistantAutofillProfile(profile.getGUID(), profile.getOrigin(),
                profile.getIsLocal(), profile.getHonorificPrefix(), profile.getFullName(),
                profile.getCompanyName(), profile.getStreetAddress(), profile.getRegion(),
                profile.getLocality(), profile.getDependentLocality(), profile.getPostalCode(),
                profile.getSortingCode(), profile.getCountryCode(), profile.getPhoneNumber(),
                profile.getEmailAddress(), profile.getLanguageCode());
    }

    private AssistantAutofillCreditCard createDummyCreditCard(
            PersonalDataManager.CreditCard creditCard) {
        return new AssistantAutofillCreditCard(creditCard.getGUID(), creditCard.getOrigin(),
                creditCard.getIsLocal(), creditCard.getIsCached(), creditCard.getName(),
                creditCard.getNumber(), creditCard.getObfuscatedNumber(), creditCard.getMonth(),
                creditCard.getYear(), creditCard.getBasicCardIssuerNetwork(),
                creditCard.getIssuerIconDrawableId(), creditCard.getBillingAddressId(),
                creditCard.getServerId(), creditCard.getInstrumentId(), creditCard.getNickname(),
                creditCard.getCardArtUrl(), creditCard.getVirtualCardEnrollmentState(),
                creditCard.getProductDescription());
    }

    private AddressModel createAddressModel(PersonalDataManager.AutofillProfile profile) {
        String fullDescription =
                PersonalDataManager.getInstance()
                        .getShippingAddressLabelWithCountryForPaymentRequest(profile);
        String summaryDescription =
                PersonalDataManager.getInstance()
                        .getShippingAddressLabelWithoutCountryForPaymentRequest(profile);
        return new AddressModel(createDummyAddress(profile), fullDescription, summaryDescription);
    }
}
