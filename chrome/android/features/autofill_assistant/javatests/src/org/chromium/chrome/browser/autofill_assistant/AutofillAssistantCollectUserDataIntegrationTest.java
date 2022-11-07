// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.clearText;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.replaceText;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.hasBackground;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
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
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.isNextAfterSibling;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.tapElement;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilKeyboardMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.buildRequiredDataPiece;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.buildValueExpression;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toVisibleCssSelector;

import android.os.Build;
import android.widget.RadioButton;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.matcher.ViewMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.CoreMatchers;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.AutofillEntryProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipIcon;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ChipType;
import org.chromium.chrome.browser.autofill_assistant.proto.ClickType;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto.DataSource;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto.TermsAndConditionsState;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataResultProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ContactDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DataOriginNoticeProto;
import org.chromium.chrome.browser.autofill_assistant.proto.DropdownSelectStrategy;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementAreaProto.Rectangle;
import org.chromium.chrome.browser.autofill_assistant.proto.ElementConditionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.GetUserDataResponseProto;
import org.chromium.chrome.browser.autofill_assistant.proto.IntList;
import org.chromium.chrome.browser.autofill_assistant.proto.KeyboardValueFillStrategy;
import org.chromium.chrome.browser.autofill_assistant.proto.LoginDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ModelProto.ModelValue;
import org.chromium.chrome.browser.autofill_assistant.proto.PaymentInstrumentProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PhoneNumberProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PopupListSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProcessedActionStatusProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ProfileProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto.Choice;
import org.chromium.chrome.browser.autofill_assistant.proto.RequiredFieldProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowCastProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ShowDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputProto.InputType;
import org.chromium.chrome.browser.autofill_assistant.proto.TextInputSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseAddressProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseCreditCardProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UserFormSectionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ValueProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.AssistantTagsForTesting;
import org.chromium.components.autofill_assistant.R;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;

/**
 * Integration tests for the collect user data action.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantCollectUserDataIntegrationTest {
    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, "form_target_website.html"));

    private AutofillAssistantCollectUserDataTestHelper mHelper;

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    @Before
    public void setUp() throws Exception {
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
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .addSupportedBasicCardNetworks("visa")
                                                     .setRequestTermsAndConditions(false))
                         .build());

        RequiredFieldProto fallbackTextField =
                RequiredFieldProto.newBuilder()
                        .setForced(true) // Make sure we do actual work.
                        .setValueExpression(buildValueExpression(57))
                        .setElement(toCssSelector("#fallback_entry"))
                        .setFillStrategy(KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES)
                        .build();
        RequiredFieldProto fallbackDropdownField =
                RequiredFieldProto.newBuilder()
                        .setForced(true) // Make sure we do actual work.
                        .setValueExpression(buildValueExpression(-2))
                        .setElement(toCssSelector("#fallback_dropdown"))
                        .setSelectStrategy(DropdownSelectStrategy.VALUE_MATCH)
                        .build();
        RequiredFieldProto fallbackJsDropdownField =
                RequiredFieldProto.newBuilder()
                        .setValueExpression(buildValueExpression(55))
                        .setElement(toCssSelector("#js_dropdown_value"))
                        .setOptionElementToClick(toVisibleCssSelector("#js_dropdown_options li"))
                        .setClickType(ClickType.TAP)
                        .build();
        list.add(ActionProto.newBuilder()
                         .setUseCard(UseCreditCardProto.newBuilder()
                                             .setFormFieldElement(toCssSelector("#card_number"))
                                             .addRequiredFields(fallbackTextField)
                                             .addRequiredFields(fallbackDropdownField)
                                             .addRequiredFields(fallbackJsDropdownField))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
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

    /**
     * Fill a form with a saved contact's details.
     */
    @Test
    @MediumTest
    public void testEnterContact() throws Exception {
        mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true))
                                        .setRequestTermsAndConditions(false))
                        .build());

        RequiredFieldProto nameField = RequiredFieldProto.newBuilder()
                                               .setValueExpression(buildValueExpression(7))
                                               .setElement(toCssSelector("#profile_name"))
                                               .setForced(true) // Make sure we do actual work.
                                               .build();
        RequiredFieldProto emailField = RequiredFieldProto.newBuilder()
                                                .setValueExpression(buildValueExpression(9))
                                                .setElement(toCssSelector("#email"))
                                                .setForced(true) // Make sure we do actual work.
                                                .build();
        list.add(ActionProto.newBuilder()
                         .setUseAddress(UseAddressProto.newBuilder()
                                                .setName("contact")
                                                .setFormFieldElement(toCssSelector("#profile_name"))
                                                .addRequiredFields(nameField)
                                                .addRequiredFields(emailField))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed(), 6000L);
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@gmail.com"));
    }

    @Test
    @MediumTest
    public void testFailingAutofillSendsProperError() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
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

        RequiredFieldProto fallbackTextField =
                RequiredFieldProto.newBuilder()
                        .setForced(true) // Make sure we fail here while trying to fill the field.
                        .setValueExpression(buildValueExpression(
                                -99)) // Use non-existent key to force an error.
                        .setElement(toCssSelector("#fallback_entry"))
                        .setFillStrategy(KeyboardValueFillStrategy.SIMULATE_KEY_PRESSES)
                        .build();
        list.add(ActionProto.newBuilder()
                         .setUseCard(UseCreditCardProto.newBuilder()
                                             .setFormFieldElement(toCssSelector("#card_number"))
                                             .addRequiredFields(fallbackTextField))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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
    @DisabledTest(message = "https://crbug.com/1274144")
    public void testTermsAndConditionsWithShowCast() throws Exception {
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@gmail.com");
        mHelper.addDummyCreditCard(profileId);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setShowCast(ShowCastProto.newBuilder()
                                              .setElementToPresent(toCssSelector("div.terms"))
                                              .setTouchableElementArea(
                                                      ElementAreaProto.newBuilder().addTouchable(
                                                              Rectangle.newBuilder().addElements(
                                                                      toCssSelector("div.terms")))))
                         .build());
        list.add(ActionProto.newBuilder()
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
        Choice toggle_chip = Choice.newBuilder()
                                     .setChip(ChipProto.newBuilder().setText("Toggle"))
                                     .setShowOnlyWhen(ElementConditionProto.newBuilder().setMatch(
                                             toVisibleCssSelector("div#toggle_on")))
                                     .build();
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder()
                                            .setMessage("Finish")
                                            .addChoices(Choice.newBuilder())
                                            .addChoices(toggle_chip))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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
                ActionProto.newBuilder()
                        .setCollectUserData(CollectUserDataProto.newBuilder()
                                                    .setPrivacyNoticeText("")
                                                    .setRequestTermsAndConditions(true)
                                                    .setShowTermsAsCheckbox(true)
                                                    .setAcceptTermsAndConditionsText("accept terms")
                                                    .setTermsAndConditionsState(
                                                            TermsAndConditionsState.ACCEPTED))
                        .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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

        list.add(ActionProto.newBuilder()
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
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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

    /**
     * Clicking on an input text should make the chip disappear, on focus lost the chip should
     * appear again.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "crbug.com/1381634")
    public void testChipsAreHiddenOnKeyboardAppering() throws Exception {
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

        list.add(ActionProto.newBuilder()
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
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("User form"), isDisplayed());
        onView(withText("User form")).perform(click());
        waitUntilViewMatchesCondition(withContentDescription("Field 1"), isDisplayed());
        onView(withContentDescription("Continue")).check(matches(isDisplayed()));
        onView(withContentDescription("Field 1")).perform(click());
        waitUntilKeyboardMatchesCondition(mTestRule, true);
        waitUntilViewMatchesCondition(withContentDescription("Continue"), not(isDisplayed()));
        onView(allOf(withContentDescription("Close"), isDisplayed())).perform(click());
        waitUntilKeyboardMatchesCondition(mTestRule, false);
        waitUntilViewMatchesCondition(withContentDescription("Continue"), isDisplayed());
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1274144")
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
                ActionProto.newBuilder()
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
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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

    /**
     * Select an item in the popup list section
     */
    @Test
    @MediumTest
    @DisableIf.Build(
        message = "https://crbug.com/1271300", sdk_is_less_than = Build.VERSION_CODES.Q)
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

        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestTermsAndConditions(false)
                                                     .addAdditionalPrependedSections(popupList))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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

        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestTermsAndConditions(false)
                                                     .addAdditionalPrependedSections(popupList))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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
                UserFormSectionProto.newBuilder()
                        .setTitle("User form")
                        .setTextInputSection(TextInputSectionProto.newBuilder().addInputFields(
                                TextInputProto.newBuilder()
                                        .setHint("field 1")
                                        .setInputType(InputType.INPUT_TEXT)
                                        .setClientMemoryKey("field_1")
                                        .setValue("old value")))
                        .setSendResultToBackend(true)
                        .build();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setShowTermsAsCheckbox(true)
                                         .setRequestTermsAndConditions(true)
                                         .setAcceptTermsAndConditionsText("<link1>click me</link1>")
                                         .addAdditionalPrependedSections(userFormSectionProto))
                         .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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
                ActionProto.newBuilder()
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
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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
        list.add(ActionProto.newBuilder()
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

        list.add(ActionProto.newBuilder()
                         .setShowDetails(
                                 ShowDetailsProto.newBuilder().setContactDetails("contact_details"))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
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

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1274144")
    public void highestPriorityLoginChoiceIsDefaultSelected() throws Exception {
        List<LoginDetailsProto.LoginOptionProto> loginDetails = Arrays.asList(
                LoginDetailsProto.LoginOptionProto.newBuilder()
                        .setCustom(LoginDetailsProto.LoginOptionCustomProto.newBuilder().setLabel(
                                "Guest checkout"))
                        .setPreselectionPriority(2)
                        .build(),
                LoginDetailsProto.LoginOptionProto.newBuilder()
                        .setCustom(LoginDetailsProto.LoginOptionCustomProto.newBuilder().setLabel(
                                "VIP checkout"))
                        .setPreselectionPriority(0)
                        .build(),
                LoginDetailsProto.LoginOptionProto.newBuilder()
                        .setCustom(LoginDetailsProto.LoginOptionCustomProto.newBuilder().setLabel(
                                "Full checkout"))
                        .setPreselectionPriority(1)
                        .build());

        List<ActionProto> actions = Collections.singletonList(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setLoginDetails(LoginDetailsProto.newBuilder()
                                                                 .setSectionTitle("Login options")
                                                                 .addAllLoginOptions(loginDetails))
                                        .setRequestTermsAndConditions(false))
                        .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                actions);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        onView(withText("Login options")).perform(click());
        waitUntilViewMatchesCondition(withText("Guest checkout"), isDisplayed());
        waitUntilViewMatchesCondition(withText("Full checkout"), isDisplayed());

        // "VIP checkout" is selected
        onView(allOf(withId(R.id.label),
                       withParent(allOf(withId(R.id.login_summary),
                               isNextAfterSibling(
                                       allOf(instanceOf(RadioButton.class), isChecked()))))))
                .check(matches(withText("VIP checkout")));

        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), isEnabled()));
    }

    /**
     * Fill a form with a contact from backend.
     */
    @Test
    @MediumTest
    public void testEnterBackendContact() throws Exception {
        GetUserDataResponseProto userData =
                GetUserDataResponseProto.newBuilder()
                        .setLocale("en-US")
                        .addAvailableContacts(
                                ProfileProto.newBuilder()
                                        .putValues(7,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("John Doe")
                                                        .build())
                                        .putValues(9,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("johndoe@google.com")
                                                        .build()))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setDataSource(DataSource.newBuilder())
                                         .setContactDetails(
                                                 ContactDetailsProto.newBuilder()
                                                         .setContactDetailsName("contact")
                                                         .setRequestPayerName(true)
                                                         .setRequestPayerEmail(true)
                                                         .setRequestPayerPhone(false)
                                                         .addRequiredDataPiece(
                                                                 buildRequiredDataPiece(
                                                                         "Requires first name", 3))
                                                         .addRequiredDataPiece(
                                                                 buildRequiredDataPiece(
                                                                         "Requires last name", 5))
                                                         .addRequiredDataPiece(
                                                                 buildRequiredDataPiece(
                                                                         "Requires email", 9)))
                                         .setRequestTermsAndConditions(false))
                         .build());
        list.add(
                ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder()
                                        .setName("contact")
                                        .setFormFieldElement(toCssSelector("#profile_name"))
                                        .addRequiredFields(
                                                RequiredFieldProto.newBuilder()
                                                        .setValueExpression(buildValueExpression(7))
                                                        .setElement(toCssSelector("#profile_name"))
                                                        .setForced(true))
                                        .addRequiredFields(
                                                RequiredFieldProto.newBuilder()
                                                        .setValueExpression(buildValueExpression(9))
                                                        .setElement(toCssSelector("#email"))
                                                        .setForced(true)))
                        .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.setUserData(userData);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isEnabled(), isCompletelyDisplayed()));
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.contact_full), withText(containsString("John Doe"))),
                isDisplayed());
        onView(withTagValue(is(AssistantTagsForTesting.CHOICE_LIST_EDIT_ICON)))
                .check(doesNotExist());
        onView(allOf(withText(R.string.payments_add_contact),
                       not(withParent(withId(R.id.section_title_add_button)))))
                .check(matches(isDisplayed()));
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@google.com"));
    }

    /**
     * Create a transient contact and use it.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1381245")
    public void testCreateAndEnterTransientContact() throws Exception {
        GetUserDataResponseProto userData =
                GetUserDataResponseProto.newBuilder()
                        .setLocale("en-US")
                        .addAvailableContacts(ProfileProto.newBuilder().putValues(
                                7, AutofillEntryProto.newBuilder().setValue("John Doe").build()))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setDataSource(DataSource.newBuilder())
                                         .setContactDetails(
                                                 ContactDetailsProto.newBuilder()
                                                         .setContactDetailsName("contact")
                                                         .setRequestPayerName(true)
                                                         .setRequestPayerPhone(false)
                                                         .addRequiredDataPiece(
                                                                 buildRequiredDataPiece(
                                                                         "Requires first name", 3))
                                                         .addRequiredDataPiece(
                                                                 buildRequiredDataPiece(
                                                                         "Requires last name", 5)))
                                         .setRequestTermsAndConditions(false))
                         .build());
        list.add(
                ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder()
                                        .setName("contact")
                                        .setFormFieldElement(toCssSelector("#profile_name"))
                                        .addRequiredFields(
                                                RequiredFieldProto.newBuilder()
                                                        .setValueExpression(buildValueExpression(7))
                                                        .setElement(toCssSelector("#profile_name"))
                                                        .setForced(true)))
                        .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.setUserData(userData);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        // Check initial state by opening the expander:
        // - There is a contact with "John Doe".
        // - It does not have an edit icon.
        // - The "add" button is shown.
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isEnabled(), isCompletelyDisplayed()));
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.contact_full), withText(containsString("John Doe"))),
                isDisplayed());
        onView(withTagValue(is(AssistantTagsForTesting.CHOICE_LIST_EDIT_ICON)))
                .check(doesNotExist());
        onView(allOf(withText(R.string.payments_add_contact),
                       not(withParent(withId(R.id.section_title_add_button)))))
                .check(matches(isDisplayed()));
        // Create new contact with the editor and check the state after submitting:
        // - The "John Doe" contact still exists.
        // - There is a single "Jane Doe" contact that was added.
        // - The newly added contact does have an edit icon.
        onView(allOf(withText(R.string.payments_add_contact),
                       not(withParent(withId(R.id.section_title_add_button)))))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(typeText("Jane Doe"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), isEnabled()));
        onView(allOf(withId(R.id.contact_full), withText(containsString("John Doe"))))
                .check(matches(isDisplayed())); // Must still exist.
        onView(allOf(withId(R.id.contact_full), withText(containsString("Jane Doe"))))
                .check(matches(isDisplayed())); // Must be unique.
        onView(withTagValue(is(AssistantTagsForTesting.CHOICE_LIST_EDIT_ICON)))
                .check(matches(isDisplayed()));
        // Edit existing contact with the editor and check the state after submitting:
        // - The "John Doe" contact still exists.
        // - The "Jane Doe" contact is gone.
        // - There is a single "Jeremy Doe" contact instead.
        // - The "Jeremy Doe" contact still has an edit icon.
        onView(withTagValue(is(AssistantTagsForTesting.CHOICE_LIST_EDIT_ICON))).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(clearText(), typeText("Jeremy Doe"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), isEnabled()));
        onView(allOf(withId(R.id.contact_full), withText(containsString("John Doe"))))
                .check(matches(isDisplayed())); // Must still exist.
        onView(allOf(withId(R.id.contact_full), withText(containsString("Jane Doe"))))
                .check(doesNotExist()); // Must be Gone.
        onView(allOf(withId(R.id.contact_full), withText(containsString("Jeremy Doe"))))
                .check(matches(isDisplayed())); // Must be unique.
        onView(withTagValue(is(AssistantTagsForTesting.CHOICE_LIST_EDIT_ICON)))
                .check(matches(isDisplayed()));
        // Submit and check that the correct value was filled into the website.
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "profile_name"), is("Jeremy Doe"));
    }

    /**
     * When using backend data privacy notice should have no background.
     */
    @Test
    @MediumTest
    public void testPrivacyNoticeStyleWithBackendData() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setDataSource(DataSource.newBuilder())
                                         .setPrivacyNoticeText("3rd party privacy text")
                                         .setShowTermsAsCheckbox(true)
                                         .setRequestTermsAndConditions(true)
                                         .setAcceptTermsAndConditionsText("accept terms"))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.setUserData(GetUserDataResponseProto.getDefaultInstance());
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(
                allOf(withText("3rd party privacy text"),
                        withEffectiveVisibility(ViewMatchers.Visibility.VISIBLE)),
                isDisplayed());
        onView(allOf(isDescendantOfA(withTagValue(
                             is(AssistantTagsForTesting
                                             .COLLECT_USER_DATA_CHECKBOX_TERMS_SECTION_TAG))),
                       withId(R.id.collect_data_privacy_notice)))
                .check(matches(
                        not(hasBackground(R.drawable.autofill_assistant_lightblue_rect_bg))));
    }

    /**
     * Load and show a card from backend.
     * TODO(b/214022384): Fill it into a form (requires unmasking).
     */
    @Test
    @MediumTest
    public void testShowBackendCard() throws Exception {
        GetUserDataResponseProto userData =
                GetUserDataResponseProto.newBuilder()
                        .setLocale("en-US")
                        .addAvailablePaymentInstruments(
                                PaymentInstrumentProto.newBuilder()
                                        .putCardValues(55,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("2050")
                                                        .build())
                                        .putCardValues(53,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("7")
                                                        .build())
                                        .putCardValues(51,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("John Doe")
                                                        .build())
                                        .setNetwork("visaCC")
                                        .setLastFourDigits("1111")
                                        .putAddressValues(35,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("80302")
                                                        .build())
                                        .putAddressValues(36,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("US")
                                                        .build())
                                        .putAddressValues(33,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("Boulder")
                                                        .build())
                                        .putAddressValues(30,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("123 Broadway St")
                                                        .build())
                                        .putAddressValues(34,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("CO")
                                                        .build())
                                        .putAddressValues(7,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("John Doe")
                                                        .build()))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setDataSource(DataSource.newBuilder())
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .addSupportedBasicCardNetworks("visa")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.setUserData(userData);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        onView(allOf(withText(R.string.payments_add_card),
                       not(withParent(withId(R.id.section_title_add_button)))))
                .check(matches(not(isDisplayed())));
        waitUntilViewMatchesCondition(withContentDescription("Continue"), isEnabled());
    }

    /**
     * Fill a form with a phone number from backend without the contact being requested.
     */
    @Test
    @MediumTest
    public void testEnterBackendPhoneNumber() throws Exception {
        GetUserDataResponseProto userData =
                GetUserDataResponseProto.newBuilder()
                        .setLocale("en-US")
                        .addAvailablePhoneNumbers(PhoneNumberProto.newBuilder().setValue(
                                AutofillEntryProto.newBuilder().setValue("+41234567890").build()))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setDataSource(DataSource.newBuilder())
                                         .setContactDetails(
                                                 ContactDetailsProto.newBuilder()
                                                         .setContactDetailsName("contact")
                                                         .setRequestPayerName(false)
                                                         .setRequestPayerEmail(false)
                                                         .setRequestPayerPhone(false)
                                                         .setSeparatePhoneNumberSection(true)
                                                         .setPhoneNumberSectionTitle("Phone number")
                                                         .addPhoneNumberRequiredDataPiece(
                                                                 buildRequiredDataPiece(
                                                                         "Requires phone number",
                                                                         14)))
                                         .setRequestTermsAndConditions(false))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setUseAddress(UseAddressProto.newBuilder()
                                                .setName("contact")
                                                .setFormFieldElement(toCssSelector("#profile_name"))
                                                .addRequiredFields(
                                                        RequiredFieldProto.newBuilder()
                                                                .setValueExpression(
                                                                        buildValueExpression(14))
                                                                .setElement(toCssSelector("#tel"))
                                                                .setForced(true)))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.setUserData(userData);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isEnabled(), isCompletelyDisplayed()));
        onView(withText("Phone number")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.contact_full), withText(containsString("+41 23 456 78 90"))),
                isDisplayed());
        onView(withTagValue(is(AssistantTagsForTesting.CHOICE_LIST_EDIT_ICON)))
                .check(doesNotExist());
        onView(allOf(withText(R.string.payments_add_phone_number),
                       not(withParent(withId(R.id.section_title_add_button)))))
                .check(matches(isDisplayed()));
        onView(allOf(withId(R.id.incomplete_error), withText("Requires phone number")))
                .check(doesNotExist());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "tel"), is("+41234567890"));
    }

    /**
     * Fill a form with a phone number from backend merged with a contact.
     */
    @Test
    @MediumTest
    public void testMergeBackendPhoneNumberIntoContact() throws Exception {
        GetUserDataResponseProto userData =
                GetUserDataResponseProto.newBuilder()
                        .setLocale("en-US")
                        .addAvailableContacts(
                                ProfileProto.newBuilder()
                                        .putValues(7,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("John Doe")
                                                        .build())
                                        .putValues(9,
                                                AutofillEntryProto.newBuilder()
                                                        .setValue("johndoe@google.com")
                                                        .build()))
                        .addAvailablePhoneNumbers(PhoneNumberProto.newBuilder().setValue(
                                AutofillEntryProto.newBuilder().setValue("+41234567890").build()))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setDataSource(DataSource.newBuilder())
                                        .setContactDetails(
                                                ContactDetailsProto.newBuilder()
                                                        .setContactDetailsName("contact")
                                                        .setRequestPayerName(true)
                                                        .setRequestPayerEmail(true)
                                                        .setRequestPayerPhone(false)
                                                        .setSeparatePhoneNumberSection(true)
                                                        .setPhoneNumberSectionTitle("Phone number"))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder()
                                        .setName("contact")
                                        .setFormFieldElement(toCssSelector("#profile_name"))
                                        .addRequiredFields(
                                                RequiredFieldProto.newBuilder()
                                                        .setValueExpression(buildValueExpression(7))
                                                        .setElement(toCssSelector("#profile_name"))
                                                        .setForced(true))
                                        .addRequiredFields(
                                                RequiredFieldProto.newBuilder()
                                                        .setValueExpression(buildValueExpression(9))
                                                        .setElement(toCssSelector("#email"))
                                                        .setForced(true))
                                        .addRequiredFields(
                                                RequiredFieldProto.newBuilder()
                                                        .setValueExpression(
                                                                buildValueExpression(14))
                                                        .setElement(toCssSelector("#tel"))
                                                        .setForced(true)))
                        .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.setUserData(userData);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isEnabled(), isCompletelyDisplayed()));
        onView(withText("Phone number")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.contact_full), withText(containsString("+41 23 456 78 90"))),
                isDisplayed());
        onView(withTagValue(is(AssistantTagsForTesting.CHOICE_LIST_EDIT_ICON)))
                .check(doesNotExist());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@google.com"));
        assertThat(getElementValue(getWebContents(), "tel"), is("+41234567890"));
    }

    /**
     * Load and enter an address from backend.
     */
    @Test
    @MediumTest
    public void testEnterBackendAddress() throws Exception {
        GetUserDataResponseProto userData =
                GetUserDataResponseProto.newBuilder()
                        .setLocale("en-US")
                        .addAvailableAddresses(ProfileProto.newBuilder()
                                                       .putValues(35,
                                                               AutofillEntryProto.newBuilder()
                                                                       .setValue("80302")
                                                                       .build())
                                                       .putValues(36,
                                                               AutofillEntryProto.newBuilder()
                                                                       .setValue("US")
                                                                       .build())
                                                       .putValues(33,
                                                               AutofillEntryProto.newBuilder()
                                                                       .setValue("Boulder")
                                                                       .build())
                                                       .putValues(30,
                                                               AutofillEntryProto.newBuilder()
                                                                       .setValue("123 Broadway St")
                                                                       .build())
                                                       .putValues(34,
                                                               AutofillEntryProto.newBuilder()
                                                                       .setValue("CO")
                                                                       .build())
                                                       .putValues(7,
                                                               AutofillEntryProto.newBuilder()
                                                                       .setValue("John Doe")
                                                                       .build()))
                        .build();

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setDataSource(DataSource.newBuilder())
                                                     .setShippingAddressName("shipping_address")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        list.add(
                ActionProto.newBuilder()
                        .setUseAddress(UseAddressProto.newBuilder()
                                               .setName("shipping_address")
                                               .setFormFieldElement(toCssSelector("#profile_name")))
                        .build());
        list.add(ActionProto.newBuilder()
                         .setPrompt(PromptProto.newBuilder().setMessage("Prompt").addChoices(
                                 PromptProto.Choice.newBuilder()))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.setUserData(userData);
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), isEnabled()));
        onView(withText("Shipping address")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withParent(withId(R.id.address_full)), withId(R.id.full_name)),
                allOf(withText("John Doe"), isCompletelyDisplayed()));
        onView(allOf(withText(R.string.payments_add_address),
                       not(withParent(withId(R.id.section_title_add_button)))))
                .check(matches(not(isDisplayed())));
        onView(withContentDescription("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
    }

    /**
     * Data origin notice should be shown and should open the dialog when clicked.
     */
    @Test
    @MediumTest
    public void testDataOriginNotice() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setRequestTermsAndConditions(false)
                                        .setDataOriginNotice(
                                                DataOriginNoticeProto.newBuilder()
                                                        .setLinkText("About this data")
                                                        .setDialogTitle(
                                                                "About your personal information")
                                                        .setDialogText(
                                                                "Information on the data.\n\n"
                                                                + "<link0>Manage your Google"
                                                                + " account</link0>")
                                                        .setDialogButtonText("Got it"))
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true))
                                        .setDataSource(DataSource.newBuilder()))
                        .build());

        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);

        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        testService.setUserData(GetUserDataResponseProto.newBuilder().setLocale("en-US").build());
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("About this data"), isDisplayed());
        onView(withText("About this data")).perform(click());

        waitUntilViewMatchesCondition(withText("About your personal information"), isDisplayed());
        onView(withText("Information on the data.\n\nManage your Google account"))
                .check(matches(isDisplayed()));
    }
}
