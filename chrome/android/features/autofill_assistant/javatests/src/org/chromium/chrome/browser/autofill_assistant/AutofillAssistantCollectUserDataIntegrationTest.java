// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.contrib.PickerActions.setDate;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withTagValue;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.iterableWithSize;
import static org.hamcrest.Matchers.not;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;

import android.widget.DatePicker;

import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.CoreMatchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.LocaleUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.autofill_assistant.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.carousel.ButtonView;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipIcon;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClickType;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto.TermsAndConditionsState;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataResultProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ContactDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DateProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DateTimeRangeProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DropdownSelectStrategy;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.IntList;
import org.chromium.chrome.browser.autofill_assistant.proto.KeyboardValueFillStrategy;
import org.chromium.chrome.browser.autofill_assistant.proto.ModelProto.ModelValue;
import org.chromium.chrome.browser.autofill_assistant.proto.PopupListSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.SelectorProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto.InputType;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseCreditCardProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseCreditCardProto.RequiredField;
import org.chromium.chrome.browser.autofill_assistant.proto.UserFormSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.WebContents;

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;
import java.util.List;
import java.util.Locale;

/**
 * Integration tests for the collect user data action.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantCollectUserDataIntegrationTest {
    @Rule
    public CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    private static final String TEST_PAGE = "/components/test/data/autofill_assistant/html/"
            + "form_target_website.html";

    private AutofillAssistantCollectUserDataTestHelper mHelper;

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    @Before
    public void setUp() throws Exception {
        AutofillAssistantPreferencesUtil.setInitialPreferences(true);
        mTestRule.startCustomTabActivityWithIntent(
                AutofillAssistantUiTestUtil.createMinimalCustomTabIntentForAutobot(
                        mTestRule.getTestServer().getURL(TEST_PAGE),
                        /* startImmediately = */ true));
        mHelper = new AutofillAssistantCollectUserDataTestHelper();
    }

    /**
     * Fill a form with a saved credit card's details and type the CVC when prompted.
     */
    @Test
    @MediumTest
    public void testEnterPayment() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setRequestPaymentMethod(true)
                                                    .setBillingAddressName("billing_address")
                                                    .addSupportedBasicCardNetworks("visa")
                                                    .setPrivacyNoticeText("3rd party privacy text")
                                                    .setShowTermsAsCheckbox(true)
                                                    .setRequestTermsAndConditions(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());

        RequiredField fallbackTextField =
                (RequiredField) RequiredField.newBuilder()
                        .setForced(true) // Make sure we do actual work.
                        .setValueExpression("${57}")
                        .setElement(SelectorProto.newBuilder().addFilters(
                                SelectorProto.Filter.newBuilder().setCssSelector(
                                        "#fallback_entry")))
                        .setFillStrategy(KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES)
                        .build();
        RequiredField fallbackDropdownField =
                (RequiredField) RequiredField.newBuilder()
                        .setForced(true) // Make sure we do actual work.
                        .setValueExpression("${-2}")
                        .setElement(SelectorProto.newBuilder().addFilters(
                                SelectorProto.Filter.newBuilder().setCssSelector(
                                        "#fallback_dropdown")))
                        .setSelectStrategy(DropdownSelectStrategy.VALUE_MATCH)
                        .build();
        RequiredField fallbackJsDropdownField =
                (RequiredField) RequiredField.newBuilder()
                        .setValueExpression("${55}")
                        .setElement(SelectorProto.newBuilder().addFilters(
                                SelectorProto.Filter.newBuilder().setCssSelector(
                                        "#js_dropdown_value")))
                        .setOptionElementToClick(
                                SelectorProto.newBuilder()
                                        .addFilters(
                                                SelectorProto.Filter.newBuilder().setCssSelector(
                                                        "#js_dropdown_options li"))
                                        .addFilters(
                                                SelectorProto.Filter.newBuilder().setBoundingBox(
                                                        SelectorProto.BoundingBoxFilter
                                                                .getDefaultInstance())))
                        .setClickType(ClickType.TAP)
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setUseCard(
                                 UseCreditCardProto.newBuilder()
                                         .setFormFieldElement(SelectorProto.newBuilder().addFilters(
                                                 SelectorProto.Filter.newBuilder().setCssSelector(
                                                         "#card_number")))
                                         .addRequiredFields(fallbackTextField)
                                         .addRequiredFields(fallbackDropdownField)
                                         .addRequiredFields(fallbackJsDropdownField))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(allOf(isDescendantOfA(withTagValue(
                             is(AssistantTagsForTesting
                                             .COLLECT_USER_DATA_CHECKBOX_TERMS_SECTION_TAG))),
                       withId(R.id.collect_data_privacy_notice)))
                .check(matches(isDisplayed()));

        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.card_unmask_input), isCompletelyDisplayed());
        onView(withId(R.id.card_unmask_input)).perform(typeText("123"), pressImeActionButton());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed(), 6000L);
        assertThat(getElementValue(getWebContents(), "name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "card_number"), is("4111111111111111"));
        assertThat(getElementValue(getWebContents(), "cv2_number"), is("123"));
        assertThat(getElementValue(getWebContents(), "exp_month"), is("12"));
        assertThat(getElementValue(getWebContents(), "exp_year"), is("2050"));
        assertThat(getElementValue(getWebContents(), "fallback_entry"), is("12/2050"));
        assertThat(getElementValue(getWebContents(), "fallback_dropdown"), is("visa"));
        assertThat(getElementValue(getWebContents(), "js_dropdown_value"), is("2050"));
    }

    @Test
    @MediumTest
    public void testFailingAutofillSendsProperError() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setRequestPaymentMethod(true)
                                                    .setBillingAddressName("billing_address")
                                                    .addSupportedBasicCardNetworks("visa")
                                                    .setPrivacyNoticeText("3rd party privacy text")
                                                    .setShowTermsAsCheckbox(true)
                                                    .setRequestTermsAndConditions(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());

        RequiredField fallbackTextField =
                (RequiredField) RequiredField.newBuilder()
                        .setForced(true) // Make sure we fail here while trying to fill the field.
                        .setValueExpression("${-99}") // Use non-existent key to force an error.
                        .setElement(SelectorProto.newBuilder().addFilters(
                                SelectorProto.Filter.newBuilder().setCssSelector(
                                        "#fallback_entry")))
                        .setFillStrategy(KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES)
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setUseCard(
                                 UseCreditCardProto.newBuilder()
                                         .setFormFieldElement(SelectorProto.newBuilder().addFilters(
                                                 SelectorProto.Filter.newBuilder().setCssSelector(
                                                         "#card_number")))
                                         .addRequiredFields(fallbackTextField))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        testService.setNextActions(new ArrayList<>());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.card_unmask_input), isCompletelyDisplayed());
        onView(withId(R.id.card_unmask_input)).perform(typeText("123"), pressImeActionButton());
        testService.waitUntilGetNextActions(1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions, iterableWithSize(2));
        assertThat(
                processedActions.get(0).getStatus(), is(ProcessedActionStatusProto.ACTION_APPLIED));
        ProcessedActionProto processedUseCard = processedActions.get(1);
        assertThat(
                processedUseCard.getStatus(), is(ProcessedActionStatusProto.AUTOFILL_INCOMPLETE));
        assertThat(processedUseCard.hasStatusDetails(), is(true));
        assertThat(processedUseCard.getStatusDetails().hasAutofillErrorInfo(), is(true));
        assertThat(processedUseCard.getStatusDetails()
                           .getAutofillErrorInfo()
                           .getAutofillFieldErrorList(),
                iterableWithSize(1));
        assertThat(processedUseCard.getStatusDetails()
                           .getAutofillErrorInfo()
                           .getAutofillFieldError(0)
                           .getNoFallbackValue(),
                is(true));
    }

    /**
     * Showcasts an element of the webpage and checks that it can be interacted with.
     */
    @Test
    @MediumTest
    public void testTermsAndConditionsWithShowCast() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setShowCast(
                                ShowCastProto.newBuilder()
                                        .setElementToPresent(SelectorProto.newBuilder().addFilters(
                                                SelectorProto.Filter.newBuilder().setCssSelector(
                                                        "div.terms")))
                                        .setTouchableElementArea(ElementAreaProto.newBuilder().addTouchable(
                                                Rectangle.newBuilder().addElements(
                                                        SelectorProto.newBuilder().addFilters(
                                                                SelectorProto.Filter.newBuilder()
                                                                        .setCssSelector(
                                                                                "div.terms"))))))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setRequestPaymentMethod(true)
                                         .setBillingAddressName("billing_address")
                                         .addSupportedBasicCardNetworks("visa")
                                         .setPrivacyNoticeText("3rd party privacy text")
                                         .setShowTermsAsCheckbox(true)
                                         .setRequestTermsAndConditions(true)
                                         .setAcceptTermsAndConditionsText("accept terms")
                                         .setTermsAndConditionsState(
                                                 TermsAndConditionsState.ACCEPTED)
                                         .setConfirmChip(
                                                 ChipProto.newBuilder()
                                                         .setText("Custom text")
                                                         .setType(ChipType.HIGHLIGHTED_ACTION)
                                                         .setIcon(ChipIcon.ICON_REFRESH)))
                         .build());
        Choice toggle_chip =
                Choice.newBuilder()
                        .setChip(ChipProto.newBuilder().setText("Toggle"))
                        .setShowOnlyWhen(ElementConditionProto.newBuilder().setMatch(
                                SelectorProto.newBuilder()
                                        .addFilters(
                                                SelectorProto.Filter.newBuilder().setCssSelector(
                                                        "div#toggle_on"))
                                        .addFilters(
                                                SelectorProto.Filter.newBuilder().setBoundingBox(
                                                        SelectorProto.BoundingBoxFilter
                                                                .getDefaultInstance()))))
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Finish")
                                            .addChoices(Choice.newBuilder())
                                            .addChoices(toggle_chip))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Continue")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Custom text"), isDisplayed());
        tapElement(mTestRule, "button");
        onView(withText("Custom text")).perform(click());
        waitUntilViewMatchesCondition(withText("Toggle"), isDisplayed());

        // Verify that in the next step the touchable window is not present anymore.
        tapElement(mTestRule, "button");
        onView(withText("Toggle")).check(matches(isDisplayed()));
    }

    /**
     * Check that sending an empty privacy notice text removes the privacy notice section.
     */
    @Test
    @MediumTest
    public void testRemovePrivacyNotice() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setPrivacyNoticeText("")
                                                    .setRequestTermsAndConditions(true)
                                                    .setShowTermsAsCheckbox(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Continue")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        onView(allOf(isDescendantOfA(withTagValue(
                             is(AssistantTagsForTesting
                                             .COLLECT_USER_DATA_CHECKBOX_TERMS_SECTION_TAG))),
                       withId(R.id.collect_data_privacy_notice)))
                .check(matches(not(isDisplayed())));
    }

    /**
     * If the user taps away from a text field, the keyboard should become hidden
     */
    @Test
    @MediumTest
    public void testKeyboardIsHiddenOnLostFocus() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        UserFormSectionProto userFormSectionProto =
                UserFormSectionProto.newBuilder()
                        .setTitle("User form")
                        .setTextInputSection(
                                TextInputSectionProto.newBuilder()
                                        .addInputFields(TextInputProto.newBuilder()
                                                                .setHint("Field 1")
                                                                .setInputType(InputType.INPUT_TEXT)
                                                                .setClientMemoryKey("field_1"))
                                        .addInputFields(TextInputProto.newBuilder()
                                                                .setHint("Field 2")
                                                                .setInputType(InputType.INPUT_TEXT)
                                                                .setClientMemoryKey("field_2")))
                        .build();

        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setPrivacyNoticeText("3rd party privacy text")
                                         .setShowTermsAsCheckbox(true)
                                         .setRequestTermsAndConditions(true)
                                         .setAcceptTermsAndConditionsText("accept terms")
                                         .setTermsAndConditionsState(
                                                 TermsAndConditionsState.ACCEPTED)
                                         .addAdditionalPrependedSections(userFormSectionProto))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("User form"), isDisplayed());
        onView(withText("User form")).perform(click());
        waitUntilViewMatchesCondition(withContentDescription("Field 1"), isDisplayed());
        onView(withContentDescription("Field 1")).perform(click());
        waitUntilKeyboardMatchesCondition(mTestRule, true);
        onView(withContentDescription("Field 2")).perform(scrollTo(), click());
        waitUntilKeyboardMatchesCondition(mTestRule, true);
        onView(withText("User form")).perform(scrollTo(), click());
        waitUntilKeyboardMatchesCondition(mTestRule, false);
    }

    @Test
    @MediumTest
    public void testIncompleteAddressOnCompleteCard() throws Exception {
        PersonalDataManager.AutofillProfile mockProfile = new PersonalDataManager.AutofillProfile(
                /* guid= */ "",
                /* origin= */ "https://www.example.com", /* honorificPrefix= */ "", "John Doe",
                /* companyName= */ "", "Somestreet",
                /* region= */ "", "Switzerland", "", /* postalCode= */ "", /* sortingCode= */ "",
                "CH", "+41 79 123 45 67", "johndoe@google.com", /* languageCode= */ "");
        String profileId = mHelper.setProfile(mockProfile);
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setRequestPaymentMethod(true)
                                                    .setBillingAddressName("billing_address")
                                                    .addSupportedBasicCardNetworks("visa")
                                                    .setShowTermsAsCheckbox(true)
                                                    .setRequestTermsAndConditions(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        // Check our UI.
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(withContentDescription("Continue")).check(matches(not(isEnabled())));
        onView(allOf(withId(R.id.incomplete_error),
                       withParent(withId(R.id.payment_method_summary))))
                .check(matches(allOf(isDisplayed(), withText("Information missing"))));
        onView(withId(R.id.payment_method_summary)).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.payment_method_full), isDisplayed());

        // Check CardEditor UI.
        onView(withContentDescription("Edit card")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Card number*"), allOf(isDisplayed(), isEnabled()));
        onView(withText(containsString("John Doe, Somestreet")))
                .check(matches(withText(containsString("Enter a valid address"))));
    }

    @Test
    @MediumTest
    public void testDateRange() {
        // Create timeslots from 08:00 AM to 04:00 PM in 30 minute steps.
        List<DateTimeRangeProto.TimeSlot> timeSlots = new ArrayList<>();
        Calendar calendar = Calendar.getInstance();
        calendar.set(2020, 1, 1, 8, 0, 0);
        Locale locale = LocaleUtils.forLanguageTag("en-US");
        DateFormat dateFormat = new SimpleDateFormat("hh:mm a", locale);
        for (int i = 0; i <= 16; i++) {
            timeSlots.add((DateTimeRangeProto.TimeSlot) DateTimeRangeProto.TimeSlot.newBuilder()
                                  .setLabel(dateFormat.format(calendar.getTime()))
                                  .setComparisonValue(i)
                                  .build());
            calendar.add(Calendar.MINUTE, 30);
        }

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setDateTimeRange(
                                                 DateTimeRangeProto.newBuilder()
                                                         .setStartDate(DateProto.newBuilder()
                                                                               .setYear(2020)
                                                                               .setMonth(1)
                                                                               .setDay(1))
                                                         .setEndDate(DateProto.newBuilder()
                                                                             .setYear(2020)
                                                                             .setMonth(1)
                                                                             .setDay(13))
                                                         .setMinDate(DateProto.newBuilder()
                                                                             .setYear(2020)
                                                                             .setMonth(1)
                                                                             .setDay(1))
                                                         .setMaxDate(DateProto.newBuilder()
                                                                             .setYear(2020)
                                                                             .setMonth(12)
                                                                             .setDay(31))
                                                         .addAllTimeSlots(timeSlots)
                                                         .setStartTimeSlot(4)
                                                         .setEndTimeSlot(4)
                                                         .setStartDateLabel("Start date")
                                                         .setStartTimeLabel("Start time")
                                                         .setEndDateLabel("End date")
                                                         .setEndTimeLabel("End time")
                                                         .setDateNotSetError("Date not set")
                                                         .setTimeNotSetError("Time not set"))
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Autostart")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        // Set end date to same as start date. Because time slots are both the same, start time will
        // be unset.
        onView(withText("End date")).perform(click());
        onView(withClassName(equalTo(DatePicker.class.getName())))
                .inRoot(isDialog())
                .perform(setDate(2020, 1, 1));
        onView(withText(R.string.date_picker_dialog_set)).inRoot(isDialog()).perform(click());

        // Continue should be disabled, because start time is not set.
        onView(allOf(withText("Continue"),
                isDescendantOfA(allOf(instanceOf(ButtonView.class), not(isEnabled())))));

        // Set valid start time.
        onView(withText("Start time")).perform(click());
        onView(withText("09:00 AM")).inRoot(isDialog()).perform(click());

        onView(allOf(withText("Continue"),
                isDescendantOfA(allOf(instanceOf(ButtonView.class), isEnabled()))));

        // Change end date and time.
        onView(withText("End date")).perform(click());
        onView(withClassName(equalTo(DatePicker.class.getName())))
                .inRoot(isDialog())
                .perform(setDate(2020, 2, 16));
        onView(withText(R.string.date_picker_dialog_set)).inRoot(isDialog()).perform(click());
        onView(withText("End time")).perform(click());
        onView(withText("10:30 AM")).inRoot(isDialog()).perform(click());

        // Change start date and time.
        onView(withText("Start date")).perform(click());
        onView(withClassName(equalTo(DatePicker.class.getName())))
                .inRoot(isDialog())
                .perform(setDate(2020, 2, 7));
        onView(withText(R.string.date_picker_dialog_set)).inRoot(isDialog()).perform(click());
        onView(withText("Start time")).perform(click());
        onView(withText("09:30 AM")).inRoot(isDialog()).perform(scrollTo(), click());

        // Finish action, wait for response and prepare next set of actions.
        List<ActionProto> nextActions = new ArrayList<>();
        nextActions.add(
                (ActionProto) ActionProto.newBuilder()
                        .setPrompt(PromptProto.newBuilder()
                                           .setMessage("Finished")
                                           .addChoices(PromptProto.Choice.newBuilder().setChip(
                                                   ChipProto.newBuilder()
                                                           .setType(ChipType.DONE_ACTION)
                                                           .setText("End"))))
                        .build());
        testService.setNextActions(nextActions);
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withText("Continue")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        ViewMatchers.assertThat(processedActions, iterableWithSize(1));
        ViewMatchers.assertThat(processedActions.get(0).getStatus(),
                CoreMatchers.is(ProcessedActionStatusProto.ACTION_APPLIED));
        CollectUserDataResultProto result = processedActions.get(0).getCollectUserDataResult();
        assertThat(result.getDateRangeStartDate(),
                equalTo(DateProto.newBuilder().setYear(2020).setMonth(2).setDay(7).build()));
        // Index 3 == 09:30 PM.
        assertThat(result.getDateRangeStartTimeslot(), is(3));
        assertThat(result.getDateRangeEndDate(),
                equalTo(DateProto.newBuilder().setYear(2020).setMonth(2).setDay(16).build()));
        // Index 5 == 10:30 PM.
        assertThat(result.getDateRangeEndTimeslot(), is(5));
    }

    /**
     * Select an item in the popup list section
     */
    @Test
    @MediumTest
    public void testPopupListSection() {
        ArrayList<ActionProto> list = new ArrayList<>();

        UserFormSectionProto popupList =
                UserFormSectionProto.newBuilder()
                        .setPopupListSection(PopupListSectionProto.newBuilder()
                                                     .setAdditionalValueKey("id")
                                                     .addItemNames("item 1")
                                                     .addItemNames("item 2")
                                                     .setSelectionMandatory(true)
                                                     .setNoSelectionErrorMessage("Error"))
                        .setTitle("Title")
                        .setSendResultToBackend(true)
                        .build();

        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestTermsAndConditions(false)
                                                     .addAdditionalPrependedSections(popupList))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Title"), isCompletelyDisplayed());
        onView(withText("Error")).check(matches(isCompletelyDisplayed()));
        // The "Continue" button should be disabled
        onView(withContentDescription("Continue")).check(matches(not(isEnabled())));
        onView(withText("Title")).perform(click());

        waitUntilViewMatchesCondition(withText("item 2"), isCompletelyDisplayed());
        onView(withText("item 2")).perform(click());
        onView(withText("Error")).check(matches(not(isDisplayed())));
        onView(withText("item 2")).check(matches(isCompletelyDisplayed()));

        // Now "Continue" button should be enabled
        onView(withContentDescription("Continue")).check(matches(isEnabled()));
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withContentDescription("Continue")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        ViewMatchers.assertThat(processedActions, iterableWithSize(1));
        ViewMatchers.assertThat(processedActions.get(0).getStatus(),
                CoreMatchers.is(ProcessedActionStatusProto.ACTION_APPLIED));
        CollectUserDataResultProto result = processedActions.get(0).getCollectUserDataResult();
        assertThat(result.getAdditionalSectionsValuesCount(), equalTo(1));
        assertThat(result.getAdditionalSectionsValues(0),
                equalTo(ModelValue.newBuilder()
                                .setIdentifier("id")
                                .setValue(ValueProto.newBuilder().setInts(
                                        IntList.newBuilder().addValues(1)))
                                .build()));
    }

    /**
     * Verify that preselected items are correctly filled
     */
    @Test
    @MediumTest
    public void testPopupListSectionPreSelected() {
        ArrayList<ActionProto> list = new ArrayList<>();

        UserFormSectionProto popupList =
                UserFormSectionProto.newBuilder()
                        .setPopupListSection(PopupListSectionProto.newBuilder()
                                                     .setAdditionalValueKey("id")
                                                     .addItemNames("item 1")
                                                     .addItemNames("item 2")
                                                     .addInitialSelection(1))
                        .setTitle("Title")
                        .setSendResultToBackend(true)
                        .build();

        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestTermsAndConditions(false)
                                                     .addAdditionalPrependedSections(popupList))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Title"), isCompletelyDisplayed());
        onView(withText("item 2")).check(matches(isCompletelyDisplayed()));
        // Now "Continue" button should be enabled
        onView(withContentDescription("Continue")).check(matches(isEnabled()));
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(withContentDescription("Continue")).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        ViewMatchers.assertThat(processedActions, iterableWithSize(1));
        ViewMatchers.assertThat(processedActions.get(0).getStatus(),
                CoreMatchers.is(ProcessedActionStatusProto.ACTION_APPLIED));
        CollectUserDataResultProto result = processedActions.get(0).getCollectUserDataResult();
        assertThat(result.getAdditionalSectionsValuesCount(), equalTo(1));
        assertThat(result.getAdditionalSectionsValues(0),
                equalTo(ModelValue.newBuilder()
                                .setIdentifier("id")
                                .setValue(ValueProto.newBuilder().setInts(
                                        IntList.newBuilder().addValues(1)))
                                .build()));
    }

    /**
     * Verify that clicking T&C link sends partial data to backend.
     */
    @Test
    @MediumTest
    public void testTermsAndConditionsLinkClick() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        UserFormSectionProto userFormSectionProto =
                (UserFormSectionProto) UserFormSectionProto.newBuilder()
                        .setTitle("User form")
                        .setTextInputSection(TextInputSectionProto.newBuilder().addInputFields(
                                TextInputProto.newBuilder()
                                        .setHint("field 1")
                                        .setInputType(InputType.INPUT_TEXT)
                                        .setClientMemoryKey("field_1")
                                        .setValue("old value")))
                        .setSendResultToBackend(true)
                        .build();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setShowTermsAsCheckbox(true)
                                         .setRequestTermsAndConditions(true)
                                         .setAcceptTermsAndConditionsText("<link1>click me</link1>")
                                         .addAdditionalPrependedSections(userFormSectionProto))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Continue")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("User form"), isCompletelyDisplayed());
        onView(withText("User form")).perform(click());
        waitUntilViewMatchesCondition(withContentDescription("field 1"), isCompletelyDisplayed());
        onView(withContentDescription("field 1")).perform(replaceText("new value"));
        int numNextActionsCalled = testService.getNextActionsCounter();
        onView(allOf(withText("click me"), isDisplayed())).perform(click());
        testService.waitUntilGetNextActions(numNextActionsCalled + 1);

        List<ProcessedActionProto> processedActions = testService.getProcessedActions();
        assertThat(processedActions.size(), equalTo(1));
        ViewMatchers.assertThat(processedActions.get(0).getStatus(),
                CoreMatchers.is(ProcessedActionStatusProto.ACTION_APPLIED));
        CollectUserDataResultProto result = processedActions.get(0).getCollectUserDataResult();
        assertThat(result.getAdditionalSectionsValuesList().size(), equalTo(1));
        assertThat(result.getAdditionalSectionsValuesList().get(0),
                equalTo(ModelValue.newBuilder()
                                .setIdentifier("field_1")
                                .setValue(ValueProto.newBuilder().setStrings(
                                        org.chromium.chrome.browser.autofill_assistant.proto
                                                .StringList.newBuilder()
                                                .addValues("new value")))
                                .build()));
    }

    @Test
    @MediumTest
    public void testCustomSectionTitles() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                (ActionProto) ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setRequestTermsAndConditions(false)
                                        .setShippingAddressSectionTitle("Delivery address")
                                        .setShippingAddressName("SHIPPING_ADDRESS")
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setRequestPayerName(true)
                                                                   .setContactDetailsName("Profile")
                                                                   .setContactDetailsSectionTitle(
                                                                           "Custom info")))
                        .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Continue")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Delivery address"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(withText("Custom info"), isCompletelyDisplayed());
    }

    /**
     * Asks for the user email (but not the name) and then shows the details. Tests that only the
     * requested info is surfaced in the details.
     */
    @Test
    @MediumTest
    public void showContactDetailsAfterCollectingEmailOnly() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add((ActionProto) ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setContactDetails(
                                                 ContactDetailsProto.newBuilder()
                                                         .setContactDetailsName("contact_details")
                                                         .setRequestPayerName(false)
                                                         .setRequestPayerEmail(true))
                                         .setShowTermsAsCheckbox(true)
                                         .setRequestTermsAndConditions(false))
                         .build());

        list.add((ActionProto) ActionProto.newBuilder()
                         .setShowDetails(
                                 ShowDetailsProto.newBuilder().setContactDetails("contact_details"))
                         .build());
        list.add((ActionProto) ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                (SupportedScriptProto) SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true).setChip(
                                ChipProto.newBuilder().setText("Payment")))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), allOf(isDisplayed(), isEnabled()));
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed(), 6000L);
        onView(withText("John Doe")).check(doesNotExist());
        onView(allOf(withText("johndoe@gmail.com"), hasSibling(withId(R.id.details_title))))
                .check(matches(isDisplayed()));
    }
}
