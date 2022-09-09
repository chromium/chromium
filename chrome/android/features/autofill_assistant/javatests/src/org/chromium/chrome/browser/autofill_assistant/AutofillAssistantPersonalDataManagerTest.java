// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.clearText;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.pressImeActionButton;
import static androidx.test.espresso.action.ViewActions.scrollTo;
import static androidx.test.espresso.action.ViewActions.typeText;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.withDecorView;
import static androidx.test.espresso.matcher.ViewMatchers.hasDescendant;
import static androidx.test.espresso.matcher.ViewMatchers.hasSibling;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isCompletelyDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDescendantOfA;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayingAtLeast;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withChild;
import static androidx.test.espresso.matcher.ViewMatchers.withClassName;
import static androidx.test.espresso.matcher.ViewMatchers.withContentDescription;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withParent;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.not;
import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anyOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.containsString;
import static org.hamcrest.Matchers.instanceOf;
import static org.hamcrest.Matchers.is;

import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.getElementValue;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.isNextAfterSibling;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.startAutofillAssistant;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewInRootMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.waitUntilViewMatchesCondition;
import static org.chromium.chrome.browser.autofill_assistant.AutofillAssistantUiTestUtil.withTextId;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.buildRequiredDataPiece;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.buildValueExpression;
import static org.chromium.chrome.browser.autofill_assistant.ProtoTestUtil.toCssSelector;

import android.os.Build;
import android.widget.RadioButton;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.FlakyTest;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill_assistant.proto.ActionProto;
import org.chromium.chrome.browser.autofill_assistant.proto.CollectUserDataProto;
import org.chromium.chrome.browser.autofill_assistant.proto.ContactDetailsProto;
import org.chromium.chrome.browser.autofill_assistant.proto.PromptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.RequiredFieldProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto;
import org.chromium.chrome.browser.autofill_assistant.proto.SupportedScriptProto.PresentationProto;
import org.chromium.chrome.browser.autofill_assistant.proto.UseAddressProto;
import org.chromium.chrome.browser.customtabs.CustomTabActivityTestRule;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.autofill_assistant.R;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.ArrayList;
import java.util.Calendar;
import java.util.Collections;

/**
 * Tests autofill assistant's interaction with the PersonalDataManager.
 */
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillAssistantPersonalDataManagerTest {
    private final CustomTabActivityTestRule mTestRule = new CustomTabActivityTestRule();

    @Rule
    public final TestRule mRulesChain = RuleChain.outerRule(mTestRule).around(
            new AutofillAssistantCustomTabTestRule(mTestRule, "form_target_website.html"));

    private AutofillAssistantCollectUserDataTestHelper mHelper;
    private AutofillTestHelper mAutofillHelper;

    private WebContents getWebContents() {
        return mTestRule.getWebContents();
    }

    @Before
    public void setUp() throws Exception {
        mHelper = new AutofillAssistantCollectUserDataTestHelper();
        mAutofillHelper = new AutofillTestHelper();
    }

    /**
     * Add a contact with Autofill Assistant UI and fill it into the form.
     */
    public void testCreateAndEnterContact(boolean pdmEnabled) throws Exception {
        TestThreadUtils.runOnUiThreadBlocking(
                () -> PersonalDataManager.setAutofillProfileEnabled(pdmEnabled));

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
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
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(typeText("John"));
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*")).perform(typeText("johndoe@google.com"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        // First round: Missing name piece.
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), not(isEnabled())));
        onView(allOf(isDisplayed(), hasSibling(withId(R.id.contact_summary)),
                       withId(R.id.incomplete_error)))
                .check(matches(withText(mTestRule.getActivity().getString(
                        R.string.autofill_assistant_payment_information_missing))));
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(allOf(isDisplayed(), hasSibling(withId(R.id.contact_full)),
                                              withId(R.id.incomplete_error)),
                anyOf(withText("Requires first name"), withText("Requires last name")));
        onView(withContentDescription("Edit contact info")).perform(scrollTo(), click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(typeText(" Doe"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        // Second round: Complete.
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), isEnabled()));
        waitUntilViewMatchesCondition(allOf(isDisplayed(), withId(R.id.contact_full)),
                allOf(withText(containsString("John Doe")),
                        withText(containsString("johndoe@google.com"))));
        waitUntilViewMatchesCondition(
                allOf(hasSibling(withId(R.id.contact_full)), withId(R.id.incomplete_error)),
                not(isDisplayed()));
        onView(withContentDescription("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@google.com"));
    }

    @Test
    @MediumTest
    public void testCreateAndEnterContactProfileWithPDMEnabled() throws Exception {
        testCreateAndEnterContact(/* pdmEnabled= */ true);
    }

    @Test
    @MediumTest
    public void testCreateAndEnterContactProfileWithPDMDisabled() throws Exception {
        testCreateAndEnterContact(/* pdmEnabled= */ false);
    }

    /**
     * Add a contact with Autofill Assistant UI, then edit the profile multiple times (see
     * b/153139772).
     */
    @Test
    @MediumTest
    public void testCreateAndEditProfileMultipleTimes() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(typeText("John Doe"));
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*")).perform(typeText("doe@google.com"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());

        // First edit: no changes.
        onView(withText("Contact info")).perform(click());
        onView(withContentDescription("Edit contact info")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());

        // Second edit: change name from John Doe to Jane Doe.
        onView(withContentDescription("Edit contact info")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(clearText(), typeText("Jane Doe"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());

        // There used to be a bug where consecutive edits of the same profile would create a
        // duplicate profile, which would break the following checks.
        onView(withText(containsString("John Doe"))).check(doesNotExist());
        onView(withText(containsString("Jane Doe"))).check(matches(isDisplayed()));
    }

    /**
     * Catch the insert of a profile added outside of the Autofill Assistant, e.g. with the Chrome
     * settings UI, and fill it into the form.
     */
    @Test
    @MediumTest
    public void testExternalAddAndEnterProfile() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        toCssSelector("#profile_name")))
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
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        mHelper.addDummyProfile("John Doe", "johndoe@google.com");
        waitUntilViewMatchesCondition(
                allOf(isDisplayed(), withId(R.id.contact_summary)), withText("johndoe@google.com"));
        waitUntilViewMatchesCondition(withContentDescription("Continue"), isEnabled());
        onView(withContentDescription("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@google.com"));
    }

    /**
     * A new profile added outside of the Autofill Assistant, e.g. with the Chrome settings UI,
     * should not overwrite the current selection.
     */
    @Test
    @MediumTest
    public void testExternalAddNewProfile() throws Exception {
        mHelper.addDummyProfile("John Doe", "johndoe@google.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        toCssSelector("#profile_name")))
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
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(isDisplayed(), withId(R.id.contact_summary)), withText("johndoe@google.com"));
        onView(withContentDescription("Continue")).check(matches(isEnabled()));
        // Add new entry that is not supposed to be selected.
        mHelper.addDummyProfile("Adam West", "adamwest@google.com");
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(withText(containsString("Adam West")), isDisplayed());
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(isDisplayed(), withId(R.id.contact_summary)), withText("johndoe@google.com"));
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        // Make sure it's not Adam West that was selected.
        assertThat(getElementValue(getWebContents(), "profile_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("johndoe@google.com"));
    }

    /**
     * An existing profile deleted outside of the Autofill Assistant, e.g. with the Chrome settings
     * UI, should be removed from the current list.
     */
    @Test
    @MediumTest
    public void testExternalDeleteProfile() throws Exception {
        String profileIdA = mHelper.addDummyProfile("Adam Doe", "adamdoe@google.com");
        mAutofillHelper.setProfileUseStatsForTesting(profileIdA, /* count= */ 1, /* date= */ 1000);
        String profileIdB = mHelper.addDummyProfile("Berta Doe", "bertadoe@google.com");
        mAutofillHelper.setProfileUseStatsForTesting(profileIdB, /* count= */ 1, /* date= */ 0);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(isDisplayed(), withId(R.id.contact_summary)), withText("adamdoe@google.com"));
        // Delete first profile, expect second to be selected.
        mHelper.deleteProfile(profileIdA);
        waitUntilViewMatchesCondition(allOf(isDisplayed(), withId(R.id.contact_summary)),
                withText("bertadoe@google.com"));
        // Delete second profile, expect nothing to be selected.
        mHelper.deleteProfile(profileIdB);
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
    }

    /**
     * Editing the currently selected contact in the Assistant Autofill UI should keep selection.
     */
    @Test
    @MediumTest
    public void testEditOfSelectedProfile() throws Exception {
        String profileIdA = mHelper.addDummyProfile("Adam West", "adamwest@google.com");
        mAutofillHelper.setProfileUseStatsForTesting(profileIdA, /* count= */ 1, /* date= */ 1000);
        String profileIdB = mHelper.addDummyProfile("John Doe", "johndoe@google.com");
        mAutofillHelper.setProfileUseStatsForTesting(profileIdB, /* count= */ 1, /* date= */ 0);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerName(true)
                                                                   .setRequestPayerEmail(true)
                                                                   .setRequestPayerPhone(false))
                                        .setRequestTermsAndConditions(false))
                        .build());
        list.add(
                ActionProto.newBuilder()
                        .setUseAddress(
                                UseAddressProto.newBuilder().setName("contact").setFormFieldElement(
                                        toCssSelector("#profile_name")))
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
        runScript(script);

        waitUntilViewMatchesCondition(allOf(isDisplayed(), withId(R.id.contact_summary)),
                withText("adamwest@google.com"));
        onView(withContentDescription("Continue")).check(matches(isEnabled()));
        // Select John Doe.
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(
                withText(containsString("John Doe")), isDisplayingAtLeast(90));
        onView(withText(containsString("John Doe"))).perform(click());
        waitUntilViewMatchesCondition(
                allOf(isDisplayed(), withId(R.id.contact_summary)), withText("johndoe@google.com"));
        // Edit John Doe to Jane Doe (does not collapse the list after editing).
        onView(withText("Contact info")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withContentDescription("Edit contact info"),
                        isNextAfterSibling(hasDescendant(withText(containsString("John Doe"))))),
                isDisplayingAtLeast(90));
        onView(allOf(withContentDescription("Edit contact info"),
                       isNextAfterSibling(hasDescendant(withText(containsString("John Doe"))))))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(clearText()).perform(typeText("Jane Doe"));
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*"))
                .perform(clearText())
                .perform(typeText("janedoe@google.com"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        waitUntilViewMatchesCondition(withText("Contact info"), isDisplayed());
        // Continue.
        waitUntilViewMatchesCondition(withText("Continue"), isEnabled());
        onView(withText("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        // Make sure it's now Jane Doe.
        assertThat(getElementValue(getWebContents(), "profile_name"), is("Jane Doe"));
        assertThat(getElementValue(getWebContents(), "email"), is("janedoe@google.com"));
    }

    /**
     * Add a credit card with Autofill Assistant UI and fill it into the form.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1247243, https://crbug.com/1249164#c2")
    public void testCreateAndEnterCard() throws Exception {
        // Add a profile for easier address selection.
        mHelper.addDummyProfile("Adam West", "adamwest@google.com");

        // The Current year is the default selection for expiration year of the credit card.
        int year = Calendar.getInstance().get(Calendar.YEAR);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        list.add(ActionProto.newBuilder()
                         .setUseCard(org.chromium.chrome.browser.autofill_assistant.proto
                                             .UseCreditCardProto.newBuilder()
                                             .setFormFieldElement(toCssSelector("#card_number")))
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

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add card")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Card number*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Card number*")).perform(typeText("4111111111111111"));
        waitUntilViewMatchesCondition(
                withContentDescription("Name on card*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name on card*")).perform(typeText("John Doe"));
        Espresso.closeSoftKeyboard(); // Close keyboard, not to hide the Spinners.
        onView(allOf(withId(org.chromium.chrome.R.id.spinner),
                       withChild(withText(String.valueOf(year)))))
                .perform(click());
        onData(anything())
                .atPosition(2 /* select 2 years in the future, 0 is the current year */)
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        onView(allOf(withId(org.chromium.chrome.R.id.spinner), withChild(withText("Select"))))
                .perform(scrollTo(), click());
        onData(anything())
                .atPosition(1 /* address of Adam, 0 is SELECT (empty) */)
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        onView(withContentDescription("Continue")).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.card_unmask_input), isCompletelyDisplayed());
        onView(withId(R.id.card_unmask_input)).perform(typeText("123"), pressImeActionButton());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "card_number"), is("4111111111111111"));
        assertThat(getElementValue(getWebContents(), "cv2_number"), is("123"));
        assertThat(getElementValue(getWebContents(), "exp_month"), is("01"));
        assertThat(getElementValue(getWebContents(), "exp_year"), is(String.valueOf(year + 2)));
    }

    /**
     * Catch the insert of a credit card added outside of the Autofill Assistant, e.g. with the
     * Chrome settings UI, and fill it into the form.
     */
    @Test
    @MediumTest
    public void testExternalAddCreditCard() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        // No UseCreditCardAction, that is tested in PaymentTest.
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@google.com");
        mHelper.addDummyCreditCard(profileId);
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        waitUntilViewMatchesCondition(withContentDescription("Continue"), isEnabled());
    }

    /**
     * Catch the insert of a credit card with a billing address missing the postal code added
     * outside of the Autofill Assistant, e.g. with the Chrome settings UI and verify the error
     * message.
     */
    @Test
    @MediumTest
    public void testExternalAddCreditCardWithoutBillingZip() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setRequestPaymentMethod(true)
                                         .setBillingAddressName("billing_address")
                                         .addRequiredBillingAddressDataPiece(
                                                 buildRequiredDataPiece("Requires Billing Zip", 35))
                                         .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        String profileId =
                mHelper.addDummyProfile("John Doe", "johndoe@google.com", /* postcode= */ "");
        mHelper.addDummyCreditCard(profileId);
        waitUntilViewMatchesCondition(
                allOf(withText(mTestRule.getActivity().getString(
                              R.string.autofill_assistant_payment_information_missing)),
                        isDescendantOfA(withId(R.id.payment_method_summary))),
                isDisplayed());
        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(allOf(withText("Requires Billing Zip"),
                                              isDescendantOfA(withId(R.id.payment_method_full))),
                isDisplayed());
        onView(withContentDescription("Continue")).check(matches(not(isEnabled())));

        // TODO(b/154068342): Update billing zip, verify that error message is gone and Continue
        //  is enabled.
    }

    /**
     * Catch the insert of a credit card with an invalid expiration date added outside of the
     * Autofill Assistant, e.g. with the Chrome settings UI and verify the error message.
     */
    @Test
    @MediumTest
    @FlakyTest(message = "https://crbug.com/1183594")
    public void testExternalAddExpiredCreditCard() throws Exception {
        // Add address for card.
        String profileId = mHelper.addDummyProfile("John Doe", "johndoe@google.com");

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setCreditCardExpiredText("Card is expired")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        CreditCard card = new CreditCard("", "https://example.com", /* isLocal= */ true,
                /* isCached= */ true, "John Doe", "4111111111111111", "1111", "12", "2000", "visa",
                org.chromium.components.autofill_assistant.R.drawable.visa_card, profileId,
                /* serverId= */ "");
        mHelper.setCreditCard(card);
        waitUntilViewMatchesCondition(
                allOf(withText(mTestRule.getActivity().getString(
                              R.string.autofill_assistant_payment_information_missing)),
                        isDescendantOfA(withId(R.id.payment_method_summary))),
                isDisplayed());
        onView(withContentDescription("Continue")).check(matches(not(isEnabled())));

        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withContentDescription("Edit card"),
                        isNextAfterSibling(hasDescendant(withText(containsString("John Doe"))))),
                isDisplayed());
        onView(allOf(withContentDescription("Edit card"),
                       isNextAfterSibling(hasDescendant(withText(containsString("John Doe"))))))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Card number*"), allOf(isDisplayed(), isEnabled()));
        onView(allOf(withId(org.chromium.chrome.R.id.spinner), withChild(withText("2000"))))
                .perform(click());
        // Select 2 years in the future, 0 is the currently invalid year, 1 is the current year.
        onData(anything())
                .atPosition(3)
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        // Updating the card does not collapse the card section.
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_full))),
                allOf(withText(containsString("1111")), isDisplayed()));
        waitUntilViewMatchesCondition(allOf(withId(R.id.incomplete_error),
                                              isDescendantOfA(withId(R.id.payment_method_full))),
                not(isDisplayed()));
        onView(withContentDescription("Continue")).check(matches(isEnabled()));
    }

    /**
     * An existing credit card deleted outside of the Autofill Assistant, e.g. with the Chrome
     * settings UI, should be removed from the current list.
     */
    @Test
    @MediumTest
    public void testExternalDeleteCreditCard() throws Exception {
        String profileIdA = mHelper.addDummyProfile("Adam Doe", "adamdoe@google.com");
        String cardIdA = mHelper.addDummyCreditCard(profileIdA, "4111111111111111");
        mAutofillHelper.setCreditCardUseStatsForTesting(cardIdA, /* count= */ 1, /* date= */ 1000);

        String profileIdB = mHelper.addDummyProfile("Berta Doe", "bertadoe@google.com");
        String cardIdB = mHelper.addDummyCreditCard(profileIdB, "5555555555554444");
        mAutofillHelper.setCreditCardUseStatsForTesting(cardIdB, /* count= */ 1, /* date= */ 0);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setRequestTermsAndConditions(false))
                         .build());
        AutofillAssistantTestScript script = new AutofillAssistantTestScript(
                SupportedScriptProto.newBuilder()
                        .setPath("form_target_website.html")
                        .setPresentation(PresentationProto.newBuilder().setAutostart(true))
                        .build(),
                list);
        runScript(script);

        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("1111")), isDisplayed()));
        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.credit_card_name), withText("Adam Doe")), isDisplayed());
        onView(allOf(withId(R.id.credit_card_name),
                       withParent(allOf(withId(R.id.payment_method_full),
                               isNextAfterSibling(
                                       allOf(instanceOf(RadioButton.class), isChecked()))))))
                .check(matches(withText("Adam Doe")));
        onView(withText("Payment method")).perform(click());
        // Delete first card, expect second card to be selected.
        mHelper.deleteCreditCard(cardIdA);
        waitUntilViewMatchesCondition(allOf(withId(R.id.credit_card_number),
                                              isDescendantOfA(withId(R.id.payment_method_summary))),
                allOf(withText(containsString("4444")), isDisplayed()));
        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.credit_card_name), withText("Berta Doe")), isDisplayed());
        onView(allOf(withId(R.id.credit_card_name),
                       withParent(allOf(withId(R.id.payment_method_full),
                               isNextAfterSibling(
                                       allOf(instanceOf(RadioButton.class), isChecked()))))))
                .check(matches(withText("Berta Doe")));
        onView(withText("Payment method")).perform(click());
        // Delete second card, expect nothing to be selected.
        mHelper.deleteCreditCard(cardIdB);
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
    }

    /**
     * Opens the edit dialog for a server card.
     */
    @Test
    @MediumTest
    public void testEditOfServerCard() throws Exception {
        String profileId = mHelper.addDummyProfile("Adam West", "adamwest@google.com");
        mHelper.addServerCreditCard(mHelper.createDummyCreditCard(
                profileId, "4111111111111111", /* isLocal = */ false));

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
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
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());

        onView(withText("Payment method")).perform(click());
        waitUntilViewMatchesCondition(withContentDescription("Edit card"), isCompletelyDisplayed());
        onView(withContentDescription("Edit card")).perform(click());
        waitUntilViewMatchesCondition(withText("Billing address*"), isDisplayed());
        // TODO(b/155624806) edit billing address and fill/check values on test website.
        onView(withId(org.chromium.chrome.R.id.payments_edit_cancel_button)).perform(click());
    }

    /**
     * Adds a new shipping address and checks that it is available when adding a new credit card
     * with Autofill Assistant UI and fill it into the form.
     */
    @Test
    @MediumTest
    @DisableIf.Build(supported_abis_includes = "x86", sdk_is_greater_than = Build.VERSION_CODES.O,
            message = "Times out on Android P+ emulators: https://crbug.com/1219046")
    public void
    testCreateShippingAddressAndCreditCard() {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(CollectUserDataProto.newBuilder()
                                                     .setRequestPaymentMethod(true)
                                                     .setBillingAddressName("billing_address")
                                                     .setShippingAddressName("shipping_address")
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
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(withText("Shipping address"), isCompletelyDisplayed());
        waitUntilViewMatchesCondition(
                allOf(withText("Add address"), withEffectiveVisibility(Visibility.VISIBLE)),
                isEnabled());
        onView(allOf(withText("Add address"), withEffectiveVisibility(Visibility.VISIBLE)))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(scrollTo(), typeText("John Doe"));
        onView(withContentDescription("Street address*"))
                .perform(scrollTo(), typeText("123 Main St"));
        onView(withContentDescription("City*")).perform(scrollTo(), typeText("Mountain View"));
        onView(withContentDescription("State*")).perform(scrollTo(), typeText("California"));
        onView(withContentDescription("ZIP code*")).perform(scrollTo(), typeText("1234"));
        onView(withText("Done")).perform(scrollTo(), click());
        waitUntilViewMatchesCondition(withText("Continue"), isCompletelyDisplayed());
        // Wait for the address to appear in the UI. From this point on it should be available in
        // the CreditCard editor.
        waitUntilViewMatchesCondition(
                allOf(withParent(withId(R.id.address_summary)), withId(R.id.full_name)),
                allOf(withText("John Doe"), isCompletelyDisplayed()));
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add card")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Card number*"), allOf(isDisplayed(), isEnabled()));
        Espresso.closeSoftKeyboard();
        onView(allOf(withId(org.chromium.chrome.R.id.spinner), withChild(withText("Select"))))
                .perform(scrollTo(), click());
        waitUntilViewInRootMatchesCondition(withText(containsString("John Doe")),
                withDecorView(withClassName(containsString("Popup"))), isDisplayed());
    }

    /**
     * Add a shipping address with Autofill Assistant UI and fill it into the form.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1272997")
    public void testCreateAndEnterShippingAddress() throws Exception {
        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(ActionProto.newBuilder()
                         .setCollectUserData(
                                 CollectUserDataProto.newBuilder()
                                         .setShippingAddressName("shipping")
                                         .addRequiredShippingAddressDataPiece(
                                                 buildRequiredDataPiece("Requires valid state", 34))
                                         .setRequestTermsAndConditions(false))
                         .build());
        list.add(
                ActionProto.newBuilder()
                        .setUseAddress(UseAddressProto.newBuilder()
                                               .setName("shipping")
                                               .setFormFieldElement(toCssSelector("#address_name")))
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
        runScript(script);

        waitUntilViewMatchesCondition(allOf(withId(R.id.section_title_add_button_label),
                                              withTextId(R.string.payments_add_address)),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label),
                       withTextId(R.string.payments_add_address)))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name*")).perform(scrollTo(), typeText("John Doe"));
        onView(withContentDescription("Street address*"))
                .perform(scrollTo(), typeText("123 Main St"));
        onView(withContentDescription("City*")).perform(scrollTo(), typeText("Mountain View"));
        onView(withContentDescription("State*")).perform(scrollTo(), typeText("Invalid"));
        onView(withContentDescription("ZIP code*")).perform(scrollTo(), typeText("1234"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        // First round: Invalid state.
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), not(isEnabled())));
        waitUntilViewMatchesCondition(
                allOf(withParent(withId(R.id.address_summary)), withId(R.id.incomplete_error)),
                allOf(withText(mTestRule.getActivity().getString(
                              R.string.autofill_assistant_payment_information_missing)),
                        isDisplayed()));
        onView(withText("Shipping address")).perform(click());
        waitUntilViewMatchesCondition(withId(R.id.address_full), isDisplayed());
        waitUntilViewMatchesCondition(
                allOf(withParent(withId(R.id.address_full)), withId(R.id.incomplete_error)),
                allOf(withText("Requires valid state"), isDisplayed()));
        onView(withContentDescription("Edit address")).perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Name*"), allOf(isDisplayed(), isEnabled()));
        Espresso.closeSoftKeyboard();
        onView(withContentDescription("State*"))
                .perform(scrollTo(), clearText(), typeText("California"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        // Second round: Complete.
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), isEnabled()));
        waitUntilViewMatchesCondition(
                allOf(withParent(withId(R.id.address_full)), withId(R.id.full_name)),
                allOf(withText("John Doe"), isCompletelyDisplayed()));
        onView(withContentDescription("Continue")).perform(click());
        waitUntilViewMatchesCondition(withText("Prompt"), isCompletelyDisplayed());
        assertThat(getElementValue(getWebContents(), "address_name"), is("John Doe"));
        assertThat(getElementValue(getWebContents(), "street"), is("123 Main St"));
        assertThat(getElementValue(getWebContents(), "zip"), is("1234"));
        assertThat(getElementValue(getWebContents(), "state"), is("California"));
    }

    /**
     * Tests a form requiring contact, billing and shipping. Creating a contact will add an entry
     * to shipping which is selected. Creating a credit card + billing address will add a new entry
     * to shipping address which should not be selected. Select the billing address in shipping.
     */
    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1272997")
    public void testCreateContactAndCreditCard() {
        // The Current year is the default selection for expiration year of the credit card.
        int year = Calendar.getInstance().get(Calendar.YEAR);

        ArrayList<ActionProto> list = new ArrayList<>();
        list.add(
                ActionProto.newBuilder()
                        .setCollectUserData(
                                CollectUserDataProto.newBuilder()
                                        .setContactDetails(ContactDetailsProto.newBuilder()
                                                                   .setContactDetailsName("contact")
                                                                   .setRequestPayerEmail(true))
                                        .setRequestPaymentMethod(true)
                                        .setBillingAddressName("billing_address")
                                        .setShippingAddressName("shipping_address")
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
        startAutofillAssistant(mTestRule.getActivity(), testService);

        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add contact info")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Email*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Email*")).perform(typeText("johndoe@google.com"));
        Espresso.closeSoftKeyboard();
        onView(withId(org.chromium.chrome.R.id.editor_dialog_done_button))
                .perform(scrollTo(), click());
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.section_title_add_button_label), withText("Add card")),
                isCompletelyDisplayed());
        onView(allOf(withId(R.id.section_title_add_button_label), withText("Add card")))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Card number*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Card number*")).perform(typeText("4111111111111111"));
        waitUntilViewMatchesCondition(
                withContentDescription("Name on card*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Name on card*")).perform(typeText("John Doe"));
        Espresso.closeSoftKeyboard(); // Close keyboard, not to hide the Spinners.
        onView(allOf(withId(org.chromium.chrome.R.id.spinner),
                       withChild(withText(String.valueOf(year)))))
                .perform(click());
        onData(anything())
                .atPosition(2 /* select 2 years in the future, 0 is the current year */)
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        onView(allOf(withId(org.chromium.chrome.R.id.spinner), withChild(withText("Select"))))
                .perform(scrollTo(), click());
        onData(anything())
                .atPosition(1 /* Add Address, 0 is Select (empty) */)
                .inRoot(withDecorView(withClassName(containsString("Popup"))))
                .perform(click());
        Espresso.closeSoftKeyboard();
        waitUntilViewMatchesCondition(
                withContentDescription("Street address*"), allOf(isDisplayed(), isEnabled()));
        onView(withContentDescription("Street address*"))
                .perform(scrollTo(), typeText("123 Main St"));
        onView(withContentDescription("City*")).perform(scrollTo(), typeText("Mountain View"));
        onView(withContentDescription("State*")).perform(scrollTo(), typeText("CA"));
        onView(withContentDescription("ZIP code*")).perform(scrollTo(), typeText("94043"));
        Espresso.closeSoftKeyboard();
        onView(withText("Done")).perform(scrollTo(), click());
        waitUntilViewMatchesCondition(withText("Billing address*"), isDisplayed());
        onView(withText("Done")).perform(scrollTo(), click());
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), not(isEnabled())));
        waitUntilViewMatchesCondition(withText("Shipping address"), isDisplayed());
        onView(withText("Shipping address")).perform(scrollTo(), click());
        onView(allOf(withId(R.id.full_address),
                       withParent(allOf(withId(R.id.address_full),
                               isNextAfterSibling(
                                       allOf(instanceOf(RadioButton.class), isChecked()))))))
                .check(matches(withText("United States")));
        waitUntilViewMatchesCondition(
                allOf(withId(R.id.full_name), withText("John Doe"),
                        withParent(isNextAfterSibling(instanceOf(RadioButton.class)))),
                isDisplayingAtLeast(90));
        onView(allOf(withId(R.id.full_name), withText("John Doe"),
                       withParent(isNextAfterSibling(instanceOf(RadioButton.class)))))
                .perform(click());
        waitUntilViewMatchesCondition(
                withContentDescription("Continue"), allOf(isDisplayed(), isEnabled()));
    }

    private void runScript(AutofillAssistantTestScript script) {
        AutofillAssistantTestService testService =
                new AutofillAssistantTestService(Collections.singletonList(script));
        startAutofillAssistant(mTestRule.getActivity(), testService);
    }
}
