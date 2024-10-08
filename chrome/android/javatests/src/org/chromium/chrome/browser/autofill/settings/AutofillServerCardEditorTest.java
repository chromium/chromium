// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.RootMatchers.isDialog;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.os.Bundle;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.test.espresso.Espresso;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;
import androidx.test.runner.lifecycle.Stage;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.autofill.AutofillEditorBase;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for AutofillServerCardEditor. */
@RunWith(ChromeJUnit4ClassRunner.class)
public class AutofillServerCardEditorTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule public JniMocker mMocker = new JniMocker();
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<AutofillServerCardEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillServerCardEditor.class);

    private static final long NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE = 100L;

    private static final CreditCard SAMPLE_VIRTUAL_CARD_ENROLLED_CARD =
            new CreditCard(
                    /* guid= */ "1",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "",
                    /* instrumentId= */ 123,
                    /* cardLabel= */ "",
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* productTermsUrl= */ null);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD =
            new CreditCard(
                    /* guid= */ "2",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "",
                    /* instrumentId= */ 234,
                    /* cardLabel= */ "",
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState
                            .UNENROLLED_AND_ELIGIBLE,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* productTermsUrl= */ null);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD =
            new CreditCard(
                    /* guid= */ "3",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "",
                    /* month= */ "5",
                    AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork= */ "visa",
                    /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "",
                    /* serverId= */ "",
                    /* instrumentId= */ 345,
                    /* cardLabel= */ "",
                    /* nickname= */ "",
                    /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState
                            .UNENROLLED_AND_NOT_ELIGIBLE,
                    /* productDescription= */ "",
                    /* cardNameForAutofillDisplay= */ "",
                    /* obfuscatedLastFourDigits= */ "",
                    /* cvc= */ "",
                    /* issuerId= */ "",
                    /* productTermsUrl= */ null);

    @Mock private AutofillPaymentMethodsDelegate.Natives mNativeMock;
    @Mock private Callback<String> mServerCardEditLinkOpenerCallback;

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        reset(mNativeMock);
        mMocker.mock(AutofillPaymentMethodsDelegateJni.TEST_HOOKS, mNativeMock);
        when(mNativeMock.init(any(Profile.class)))
                .thenReturn(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
        // Ensures that the native cleanup method is called before mocks are cleaned up
        // on failed cases.
        Activity activity = mSettingsActivityTestRule.getActivity();
        if (activity != null) {
            finishAndWaitForActivity(activity);
        }
    }

    @Test
    @MediumTest
    public void virtualCardEnrolled_virtualCardRemoveButtonShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)));
        // Ensure the activity is cleaned up.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    public void virtualCardUnenrolledAndEligible_virtualCardAddButtonShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)));
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    public void virtualCardUnenrolledAndNotEligible_virtualCardLayoutNotShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(
                                SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/342333311")
    public void
            virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollAccepted_enrollmentSuccessful()
                    throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));
        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and allows enrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(isEnabled()));

        // Press the enrollment button.
        // Verify that enrollment button click is recorded.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.SettingsPage.ButtonClicked.VirtualCard.VirtualCardEnroll", true);
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());
        histogram.assertExpected();

        // Verify that the Virtual Card enrollment button still shows text for enrollment and that
        // the button is disabled.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(Matchers.not(isEnabled())));

        // Verify that the native enroll method was called with the correct parameters.
        ArgumentCaptor<Callback<VirtualCardEnrollmentFields>> callbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .initVirtualCardEnrollment(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(234L),
                        callbackArgumentCaptor.capture());
        // Return VirtualCardEnrollmentFields via the callback to show the dialog.
        Callback<VirtualCardEnrollmentFields> virtualCardEnrollmentFieldsCallback =
                callbackArgumentCaptor.getValue();
        VirtualCardEnrollmentFields fakeVirtualCardEnrollmentFields =
                VirtualCardEnrollmentFields.create("Visa", "1234", 0, new GURL(""));
        fakeVirtualCardEnrollmentFields.mGoogleLegalMessages.add(new LegalMessageLine("google"));
        fakeVirtualCardEnrollmentFields.mIssuerLegalMessages.add(new LegalMessageLine("issuer"));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        virtualCardEnrollmentFieldsCallback.onResult(
                                fakeVirtualCardEnrollmentFields));

        // Verify that the dialog was displayed.
        onView(withId(R.id.dialog_title)).inRoot(isDialog()).check(matches(isDisplayed()));

        // Click on the education link.
        // Verify that education text link click is recorded.
        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.VirtualCard.SettingsPageEnrollment.LinkClicked",
                        VirtualCardEnrollmentLinkType.VIRTUAL_CARD_ENROLLMENT_LEARN_MORE_LINK);
        onView(withId(R.id.virtual_card_education)).perform(clickLink());
        histogram.assertExpected();

        // Go back to the settings page.
        Espresso.pressBack();

        // Click positive button on enrollment dialog.
        // Verify that enrollment dialog acceptance is recorded.
        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.VirtualCard.SettingsPageEnrollment", true);
        onView(withId(R.id.positive_button)).perform(click());
        histogram.assertExpected();

        // Verify that the Virtual Card enrollment button still shows text for enrollment and that
        // the button is disabled while waiting for server response.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(Matchers.not(isEnabled())));

        // Verify that enrollment is called with the correct parameters when the user clicks the
        // positive button on the dialog.
        ArgumentCaptor<Callback<Boolean>> booleanCallbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .enrollOfferedVirtualCard(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        booleanCallbackArgumentCaptor.capture());
        // Return enrollment update status "successful" via the callback.
        Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback =
                booleanCallbackArgumentCaptor.getValue();
        ThreadUtils.runOnUiThreadBlocking(
                () -> virtualCardEnrollmentUpdateResponseCallback.onResult(true));

        // Verify that the Virtual Card enrollment button now allows unenrollment.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(isEnabled()));

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    public void
            virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollAccepted_enrollmentFailure()
                    throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));
        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and allows enrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(isEnabled()));

        // Press the enrollment button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());
        // Verify that the Virtual Card enrollment button still shows text for enrollment and that
        // the button is disabled.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(Matchers.not(isEnabled())));

        // Verify that the native enroll method was called with the correct parameters.
        ArgumentCaptor<Callback<VirtualCardEnrollmentFields>> callbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .initVirtualCardEnrollment(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(234L),
                        callbackArgumentCaptor.capture());
        // Return VirtualCardEnrollmentFields via the callback to show the dialog.
        Callback<VirtualCardEnrollmentFields> virtualCardEnrollmentFieldsCallback =
                callbackArgumentCaptor.getValue();
        VirtualCardEnrollmentFields fakeVirtualCardEnrollmentFields =
                VirtualCardEnrollmentFields.create("Visa", "1234", 0, new GURL(""));
        fakeVirtualCardEnrollmentFields.mGoogleLegalMessages.add(new LegalMessageLine("google"));
        fakeVirtualCardEnrollmentFields.mIssuerLegalMessages.add(new LegalMessageLine("issuer"));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        virtualCardEnrollmentFieldsCallback.onResult(
                                fakeVirtualCardEnrollmentFields));

        // Verify that the dialog was displayed.
        onView(withId(R.id.dialog_title)).inRoot(isDialog()).check(matches(isDisplayed()));

        // Click positive button on enrollment dialog.
        onView(withId(R.id.positive_button)).perform(click());

        // Verify that enrollment is called with the correct parameters when the user clicks the
        // positive button on the dialog.
        ArgumentCaptor<Callback<Boolean>> booleanCallbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .enrollOfferedVirtualCard(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        booleanCallbackArgumentCaptor.capture());
        // Return enrollment update status "failure" via the callback.
        Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback =
                booleanCallbackArgumentCaptor.getValue();
        ThreadUtils.runOnUiThreadBlocking(
                () -> virtualCardEnrollmentUpdateResponseCallback.onResult(false));

        // Verify that the Virtual Card enrollment button again allows enrollment.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(isEnabled()));

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    public void virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollRejected()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));
        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and allows enrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(isEnabled()));

        // Press the enrollment button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());
        // Verify that the Virtual Card enrollment button still shows text for enrollment and that
        // the button is disabled.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(Matchers.not(isEnabled())));

        // Verify that the native enroll method was called with the correct parameters.
        ArgumentCaptor<Callback<VirtualCardEnrollmentFields>> callbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .initVirtualCardEnrollment(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(234L),
                        callbackArgumentCaptor.capture());
        // Return VirtualCardEnrollmentFields via the callback to show the dialog.
        Callback<VirtualCardEnrollmentFields> virtualCardEnrollmentFieldsCallback =
                callbackArgumentCaptor.getValue();
        VirtualCardEnrollmentFields fakeVirtualCardEnrollmentFields =
                VirtualCardEnrollmentFields.create("Visa", "1234", 0, new GURL(""));
        fakeVirtualCardEnrollmentFields.mGoogleLegalMessages.add(new LegalMessageLine("google"));
        fakeVirtualCardEnrollmentFields.mIssuerLegalMessages.add(new LegalMessageLine("issuer"));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        virtualCardEnrollmentFieldsCallback.onResult(
                                fakeVirtualCardEnrollmentFields));

        // Verify that the dialog was displayed.
        onView(withId(R.id.dialog_title)).inRoot(isDialog()).check(matches(isDisplayed()));

        // Click negative button on enrollment dialog.
        // Verify that enrollment dialog rejection is recorded.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.VirtualCard.SettingsPageEnrollment", false);
        onView(withId(R.id.negative_button)).perform(click());
        histogram.assertExpected();

        // Verify that the Virtual Card enrollment button again allows enrollment.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(isEnabled()));
        // Verify that enrollment is not called when the user does not click the positive button on
        // the dialog.
        verify(mNativeMock, times(0))
                .enrollOfferedVirtualCard(
                        eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE), any(Callback.class));

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @DisabledTest(message = "https://crbug.com/1368548")
    public void
            virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollAccepted_editorExited()
                    throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));
        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and allows enrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(isEnabled()));

        // Press the enrollment button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());

        // Verify that the native enroll method was called with the correct parameters.
        ArgumentCaptor<Callback<VirtualCardEnrollmentFields>> callbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .initVirtualCardEnrollment(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(234L),
                        callbackArgumentCaptor.capture());
        // Return VirtualCardEnrollmentFields via the callback to show the dialog.
        Callback<VirtualCardEnrollmentFields> virtualCardEnrollmentFieldsCallback =
                callbackArgumentCaptor.getValue();
        VirtualCardEnrollmentFields fakeVirtualCardEnrollmentFields =
                VirtualCardEnrollmentFields.create("Visa", "1234", 0, new GURL(""));
        fakeVirtualCardEnrollmentFields.mGoogleLegalMessages.add(new LegalMessageLine("google"));
        fakeVirtualCardEnrollmentFields.mIssuerLegalMessages.add(new LegalMessageLine("issuer"));
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        virtualCardEnrollmentFieldsCallback.onResult(
                                fakeVirtualCardEnrollmentFields));

        // Verify that the dialog was displayed.
        onView(withId(R.id.dialog_title)).inRoot(isDialog()).check(matches(isDisplayed()));

        // Click positive button on enrollment dialog.
        onView(withId(R.id.positive_button)).perform(click());

        // Exit the editor.
        finishAndWaitForActivity(activity);

        // Verify that the native delegate is not cleaned up while the server call is in progress.
        verify(mNativeMock, never()).cleanup(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);

        // Verify that enrollment is called with the correct parameters even after the editor is
        // closed.
        ArgumentCaptor<Callback<Boolean>> booleanCallbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .enrollOfferedVirtualCard(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        booleanCallbackArgumentCaptor.capture());
        // Return enrollment update status "successful" via the callback.
        Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback =
                booleanCallbackArgumentCaptor.getValue();
        ThreadUtils.runOnUiThreadBlocking(
                () -> virtualCardEnrollmentUpdateResponseCallback.onResult(true));

        // Ensure that the callback is run after receiving the server response and that the native
        // delegate is cleaned up.
        verify(mNativeMock).cleanup(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
    }

    @Test
    @MediumTest
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_dialogShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the Virtual Card enrollment button is shown and allows unenrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(isEnabled()));

        // Press the unenrollment button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());
        // Verify that the unenroll dialog is shown.
        onView(withText(R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                .check(matches(isDisplayed()));
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollCancelled()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the Virtual Card enrollment button is shown and allows unenrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(isEnabled()));

        // Press the unenrollment button.
        // Verify that unenrollment button click is recorded.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.SettingsPage.ButtonClicked.VirtualCard.VirtualCardUnenroll",
                        true);
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());
        histogram.assertExpected();

        // Verify that the unenroll dialog is shown.
        onView(withText(R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                .check(matches(isDisplayed()));

        // Click the Cancel button.
        // Verify that unenrollment dialog rejection is recorded.
        histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.VirtualCard.SettingsPageUnenrollment", false);
        onView(withText(android.R.string.cancel)).perform(click());
        histogram.assertExpected();

        // Verify that the Virtual card enrollment button still allows unenrollment.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(isEnabled()));

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    public void
            virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollAccepted_unenrollmentSuccessful()
                    throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and allows unenrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(isEnabled()));

        // Press the unenrollment button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());

        // Verify that the unenroll dialog is shown.
        onView(withText(R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                .check(matches(isDisplayed()));

        // Click the positive button on unenrollment dialog.
        // Verify that unenrollment dialog acceptance is recorded.
        var histogram =
                HistogramWatcher.newSingleRecordWatcher(
                        "Autofill.VirtualCard.SettingsPageUnenrollment", true);
        onView(
                        withText(
                                R.string
                                        .autofill_credit_card_editor_virtual_card_unenroll_dialog_positive_button_label))
                .perform(click());
        histogram.assertExpected();

        // Verify that the Virtual Card enrollment button still shows text for unenrollment and that
        // the button is disabled while waiting for server response.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(Matchers.not(isEnabled())));

        // Verify that native unenroll method is called with the correct parameters when the user
        // clicks the positive button on the dialog.
        ArgumentCaptor<Callback<Boolean>> booleanCallbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .unenrollVirtualCard(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(123L),
                        booleanCallbackArgumentCaptor.capture());
        // Return enrollment update status "successful" via the callback.
        Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback =
                booleanCallbackArgumentCaptor.getValue();
        ThreadUtils.runOnUiThreadBlocking(
                () -> virtualCardEnrollmentUpdateResponseCallback.onResult(true));

        // Verify that the Virtual Card enrollment button now allows enrollment.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)))
                .check(matches(isEnabled()));

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    public void
            virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollAccepted_unenrollmentFailure()
                    throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and allows unenrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(isEnabled()));

        // Press the unenrollment button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());

        // Verify that the unenroll dialog is shown.
        onView(withText(R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                .check(matches(isDisplayed()));

        // Click the positive button on unenrollment dialog.
        onView(
                        withText(
                                R.string
                                        .autofill_credit_card_editor_virtual_card_unenroll_dialog_positive_button_label))
                .perform(click());

        // Verify that native unenroll method is called with the correct parameters when the user
        // clicks the positive button on the dialog.
        ArgumentCaptor<Callback<Boolean>> booleanCallbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .unenrollVirtualCard(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(123L),
                        booleanCallbackArgumentCaptor.capture());
        // Return enrollment update status "failure" via the callback.
        Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback =
                booleanCallbackArgumentCaptor.getValue();
        ThreadUtils.runOnUiThreadBlocking(
                () -> virtualCardEnrollmentUpdateResponseCallback.onResult(false));

        // Verify that the Virtual Card enrollment button still allows unenrollment.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(isEnabled()));

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollAccepted_editorExited()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and allows unenrollment.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)))
                .check(matches(isEnabled()));

        // Press the unenrollment button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());

        // Verify that the unenroll dialog is shown.
        onView(withText(R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                .check(matches(isDisplayed()));

        // Click the positive button on unenrollment dialog.
        onView(
                        withText(
                                R.string
                                        .autofill_credit_card_editor_virtual_card_unenroll_dialog_positive_button_label))
                .perform(click());

        // Exit the editor.
        finishAndWaitForActivity(activity);

        // Verify that the native delegate is not cleaned up while the server call is in progress.
        verify(mNativeMock, never()).cleanup(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);

        // Verify that unenrollment is called with the correct parameters even after the editor is
        // closed.
        ArgumentCaptor<Callback<Boolean>> booleanCallbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .unenrollVirtualCard(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(123L),
                        booleanCallbackArgumentCaptor.capture());
        // Return enrollment update status "successful" via the callback.
        Callback<Boolean> virtualCardEnrollmentUpdateResponseCallback =
                booleanCallbackArgumentCaptor.getValue();
        ThreadUtils.runOnUiThreadBlocking(
                () -> virtualCardEnrollmentUpdateResponseCallback.onResult(true));

        // Ensure that the callback is run after receiving the server response and that the native
        // delegate is cleaned up.
        verify(mNativeMock).cleanup(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
    }

    @Test
    @MediumTest
    public void testAutofillPaymentMethodsDelegateLifecycleEvents() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        finishAndWaitForActivity(activity);

        // Ensure that the native delegate is cleaned up when the test has finished.
        verify(mNativeMock).cleanup(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
    }

    @Test
    @MediumTest
    public void testCustomUrlForServerCardEditPage() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));
        mSettingsActivityTestRule
                .getFragment()
                .setServerCardEditLinkOpenerCallbackForTesting(mServerCardEditLinkOpenerCallback);

        // Verify that the edit card button is shown and enabled.
        onView(withId(R.id.edit_server_card)).check(matches(isDisplayed()));
        onView(withId(R.id.edit_server_card)).check(matches(isEnabled()));

        // Click the server card edit button.
        onView(withId(R.id.edit_server_card)).perform(click());

        // Verify that the callback to open the custom tab for editing card was called with the
        // expected URL.
        verify(mServerCardEditLinkOpenerCallback)
                .onResult(
                        eq(
                                "https://pay.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods&id="
                                        + SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getInstrumentId()));

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({ChromeSwitches.USE_SANDBOX_WALLET_ENVIRONMENT})
    public void testCustomUrlForServerCardEditPage_sandboxEnabled() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity =
                mSettingsActivityTestRule.startSettingsActivity(
                        fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));
        mSettingsActivityTestRule
                .getFragment()
                .setServerCardEditLinkOpenerCallbackForTesting(mServerCardEditLinkOpenerCallback);

        // Verify that the edit card button is shown and enabled.
        onView(withId(R.id.edit_server_card)).check(matches(isDisplayed()));
        onView(withId(R.id.edit_server_card)).check(matches(isEnabled()));

        // Click the server card edit button.
        onView(withId(R.id.edit_server_card)).perform(click());

        String kExpectedUrl =
                "https://pay.sandbox.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods&id=";
        // Verify that the callback to open the custom tab for editing card was called with the
        // expected URL.
        verify(mServerCardEditLinkOpenerCallback)
                .onResult(eq(kExpectedUrl + SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getInstrumentId()));

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    private void finishAndWaitForActivity(Activity activity) {
        activity.finish();
        ApplicationTestUtils.waitForActivityState(activity, Stage.DESTROYED);
    }

    private Bundle fragmentArgs(String guid) {
        Bundle args = new Bundle();
        args.putString(AutofillEditorBase.AUTOFILL_GUID, guid);
        return args;
    }

    private ViewAction clickLink() {
        return new ViewAction() {
            @Override
            public Matcher<View> getConstraints() {
                return Matchers.instanceOf(TextView.class);
            }

            @Override
            public String getDescription() {
                return "Clicks on the only link in the view.";
            }

            @Override
            public void perform(UiController uiController, View view) {
                TextView textView = (TextView) view;
                Spanned spannedString = (Spanned) textView.getText();
                ClickableSpan[] spans =
                        spannedString.getSpans(0, spannedString.length(), ClickableSpan.class);
                if (spans.length == 0) {
                    throw new NoMatchingViewException.Builder()
                            .includeViewHierarchy(true)
                            .withRootView(textView)
                            .build();
                }
                Assert.assertEquals("There should be only one clickable link", 1, spans.length);
                spans[0].onClick(view);
            }
        };
    }
}
