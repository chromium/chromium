// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.reset;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.app.Activity;
import android.graphics.Bitmap;
import android.os.Bundle;
import android.support.test.runner.lifecycle.Stage;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.Callback;
import org.chromium.base.test.util.ApplicationTestUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.LegalMessageLine;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for AutofillServerCardEditor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AutofillServerCardEditorTest {
    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();
    @Rule
    public JniMocker mMocker = new JniMocker();
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();
    @Rule
    public final SettingsActivityTestRule<AutofillServerCardEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillServerCardEditor.class);

    private static final long NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE = 100L;

    private static final CreditCard SAMPLE_VIRTUAL_CARD_ENROLLED_CARD = new CreditCard(
            /* guid= */ "1", /* origin= */ "", /* isLocal= */ false, /* isCached= */ true,
            /* name= */ "John Doe", /* number= */ "4444333322221111", /* obfuscatedNumber= */ "",
            /* month= */ "5", AutofillTestHelper.nextYear(), /* basicCardIssuerNetwork = */ "visa",
            /* issuerIconDrawableId= */ 0, /* billingAddressId= */ "", /* serverId= */ "",
            /* instrumentId= */ 123, /* cardLabel= */ "", /* nickname= */ "",
            /* cardArtUrl= */ null,
            /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD =
            new CreditCard(/* guid= */ "2", /* origin= */ "", /* isLocal= */ false,
                    /* isCached= */ true, /* name= */ "John Doe", /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork = */ "visa", /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "", /* serverId= */ "", /* instrumentId= */ 234,
                    /* cardLabel= */ "", /* nickname= */ "", /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */
                    VirtualCardEnrollmentState.UNENROLLED_AND_ELIGIBLE);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD =
            new CreditCard(/* guid= */ "3", /* origin= */ "", /* isLocal= */ false,
                    /* isCached= */ true, /* name= */ "John Doe", /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork = */ "visa", /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "", /* serverId= */ "", /* instrumentId= */ 345,
                    /* cardLabel= */ "", /* nickname= */ "", /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */
                    VirtualCardEnrollmentState.UNENROLLED_AND_NOT_ELIGIBLE);

    @Mock
    private AutofillPaymentMethodsDelegate.Natives mNativeMock;
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
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardEnrolled_virtualCardRemoveButtonShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.remove)));
        // Ensure the activity is cleaned up.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardUnenrolledAndEligible_virtualCardAddButtonShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button)).check(matches(withText(R.string.add)));
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardUnenrolledAndNotEligible_virtualCardLayoutNotShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void updateEnrollmentFeatureDisabled_virtualCardLayoutNotShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollAccepted()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));
        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and shows "Add".
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button)).check(matches(withText(R.string.add)));

        // Press the Add button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());

        // Verify that the Virtual Card enrollment button still shows "Add" and that the button is
        // disabled.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.add)))
                .check(matches(Matchers.not(isEnabled())));

        // Verify that the native enroll method was called with the correct parameters.
        ArgumentCaptor<Callback<VirtualCardEnrollmentFields>> callbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .offerVirtualCardEnrollment(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(234L), callbackArgumentCaptor.capture());

        // Return VirtualCardEnrollmentFields via the callback to show the dialog.
        Callback<VirtualCardEnrollmentFields> virtualCardEnrollmentFieldsCallback =
                callbackArgumentCaptor.getValue();
        VirtualCardEnrollmentFields fakeVirtualCardEnrollmentFields =
                VirtualCardEnrollmentFields.create(
                        "Visa 1234", Bitmap.createBitmap(10, 20, Bitmap.Config.ALPHA_8));
        fakeVirtualCardEnrollmentFields.mGoogleLegalMessages.add(new LegalMessageLine("google"));
        fakeVirtualCardEnrollmentFields.mIssuerLegalMessages.add(new LegalMessageLine("issuer"));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> virtualCardEnrollmentFieldsCallback.onResult(
                                fakeVirtualCardEnrollmentFields));

        // Verify that the dialog was displayed.
        onView(withId(R.id.dialog_title)).check(matches(isDisplayed()));

        // Click positive button on enrollment dialog.
        onView(withId(R.id.positive_button)).perform(click());

        // Verify that the Virtual Card enrollment button shows "Remove" and that the button is
        // enabled.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.remove)))
                .check(matches(isEnabled()));
        // Verify that enrollment is called when the user clicks the positive button on the dialog.
        verify(mNativeMock).enrollOfferedVirtualCard(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollRejected()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));
        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button is shown and shows "Add".
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button)).check(matches(withText(R.string.add)));

        // Press the Add button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());

        // Verify that the Virtual Card enrollment button still shows "Add" and that the button is
        // disabled.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.add)))
                .check(matches(Matchers.not(isEnabled())));

        // Verify that the native enroll method was called with the correct parameters.
        ArgumentCaptor<Callback<VirtualCardEnrollmentFields>> callbackArgumentCaptor =
                ArgumentCaptor.forClass(Callback.class);
        verify(mNativeMock)
                .offerVirtualCardEnrollment(
                        ArgumentMatchers.eq(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE),
                        ArgumentMatchers.eq(234L), callbackArgumentCaptor.capture());

        // Return VirtualCardEnrollmentFields via the callback to show the dialog.
        Callback<VirtualCardEnrollmentFields> virtualCardEnrollmentFieldsCallback =
                callbackArgumentCaptor.getValue();
        VirtualCardEnrollmentFields fakeVirtualCardEnrollmentFields =
                VirtualCardEnrollmentFields.create(
                        "Visa 1234", Bitmap.createBitmap(10, 20, Bitmap.Config.ALPHA_8));
        fakeVirtualCardEnrollmentFields.mGoogleLegalMessages.add(new LegalMessageLine("google"));
        fakeVirtualCardEnrollmentFields.mIssuerLegalMessages.add(new LegalMessageLine("issuer"));
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> virtualCardEnrollmentFieldsCallback.onResult(
                                fakeVirtualCardEnrollmentFields));

        // Verify that the dialog was displayed.
        onView(withId(R.id.dialog_title)).check(matches(isDisplayed()));

        // Click negative button on enrollment dialog.
        onView(withId(R.id.negative_button)).perform(click());

        // Verify that the Virtual Card enrollment button still shows "Add" and that the button is
        // now enabled.
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.add)))
                .check(matches(isEnabled()));
        // Verify that enrollment is called when the user clicks the positive button on the dialog.
        verify(mNativeMock, times(0))
                .enrollOfferedVirtualCard(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_dialogShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the Virtual Card enrollment button is shown and shows "Remove".
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.remove)));

        // Press the Remove button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());
        // Verify that the unenroll dialog is shown.
        onView(withText(R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                .check(matches(isDisplayed()));
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollCancelled()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the Virtual Card enrollment button shows Remove.
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.remove)));

        // Press the Remove button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());
        // Verify that the unenroll dialog is shown.
        onView(withText(R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                .check(matches(isDisplayed()));

        // Click the Cancel button.
        onView(withText(android.R.string.cancel)).perform(click());
        // Verify that the button label has not changed from "Remove".
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.remove)));
        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollAccepted()
            throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        // Verify that the Virtual Card enrollment button shows "Remove".
        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(matches(withText(R.string.remove)));

        // Press the Remove button.
        onView(withId(R.id.virtual_card_enrollment_button)).perform(click());
        // Verify that the unenroll dialog is shown.
        onView(withText(R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_title))
                .check(matches(isDisplayed()));
        // Click the Remove button.
        onView(withText(
                       R.string.autofill_credit_card_editor_virtual_card_unenroll_dialog_positive_button_label))
                .perform(click());
        // Verify that the Virtual Card enrollment button now shows "Add".
        onView(withId(R.id.virtual_card_enrollment_button)).check(matches(withText(R.string.add)));
        // Verify that the native unenroll method was called with the correct parameters.
        verify(mNativeMock).unenrollVirtualCard(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE, 123);

        // Ensure that the native delegate is cleaned up when the test has finished.
        finishAndWaitForActivity(activity);
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void testAutofillPaymentMethodsDelegateLifecycleEvents() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        finishAndWaitForActivity(activity);

        // Ensure that the native delegate is cleaned up when the test has finished.
        verify(mNativeMock).cleanup(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
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
}
