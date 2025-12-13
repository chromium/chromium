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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.content.Intent;
import android.os.Bundle;
import android.text.Spanned;
import android.text.style.ClickableSpan;
import android.view.View;
import android.widget.TextView;

import androidx.test.core.app.ActivityScenario;
import androidx.test.espresso.NoMatchingViewException;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.hamcrest.Matchers;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.ArgumentCaptor;
import org.mockito.ArgumentMatchers;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivity;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeStringConstants;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillImageFetcherFactory;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.profiles.ProfileManagerUtilsJni;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsIntentUtil;
import org.chromium.chrome.browser.settings.SettingsNavigationFactory;
import org.chromium.components.autofill.VirtualCardEnrollmentLinkType;
import org.chromium.components.autofill.VirtualCardEnrollmentState;
import org.chromium.components.autofill.payments.LegalMessageLine;
import org.chromium.components.browser_ui.settings.SettingsNavigation;
import org.chromium.url.GURL;

/** Unit tests for {@link AutofillServerCardEditor}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
public class AutofillServerCardEditorTest {

    private static final long NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE = 100L;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Mock private Profile mProfile;
    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private SettingsNavigation mSettingsNavigation;
    @Mock private ChromeBrowserInitializer mInitializer;
    @Mock private ProfileManagerUtilsJni mProfileManagerUtilsJni;
    @Mock private AutofillImageFetcher mImageFetcher;
    @Mock private Callback<String> mServerCardEditLinkOpenerCallback;
    @Mock private AutofillPaymentMethodsDelegate.Natives mNativeMock;

    private static final CreditCard SAMPLE_VIRTUAL_CARD_ENROLLED_CARD =
            new CreditCard(
                    /* guid= */ "1",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* networkAndLastFourDigits= */ "",
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
                    /* benefitSource= */ "",
                    /* productTermsUrl= */ null);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD =
            new CreditCard(
                    /* guid= */ "2",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* networkAndLastFourDigits= */ "",
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
                    /* benefitSource= */ "",
                    /* productTermsUrl= */ null);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD =
            new CreditCard(
                    /* guid= */ "3",
                    /* origin= */ "",
                    /* isLocal= */ false,
                    /* isVirtual= */ false,
                    /* name= */ "John Doe",
                    /* number= */ "4444333322221111",
                    /* networkAndLastFourDigits= */ "",
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
                    /* benefitSource= */ "",
                    /* productTermsUrl= */ null);

    private ActivityScenario<SettingsActivity> mActivityScenario;
    private SettingsActivity mSettingsActivity;
    private AutofillServerCardEditor mCardEditor;

    @Before
    public void setUp() {
        AutofillPaymentMethodsDelegateJni.setInstanceForTesting(mNativeMock);
        when(mNativeMock.init(any(Profile.class)))
                .thenReturn(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
        SettingsNavigationFactory.setInstanceForTesting(mSettingsNavigation);
        ProfileManagerUtilsJni.setInstanceForTesting(mProfileManagerUtilsJni);
        ChromeBrowserInitializer.setForTesting(mInitializer);
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        AutofillImageFetcherFactory.setInstanceForTesting(mImageFetcher);
    }

    @After
    public void tearDown() {
        if (mActivityScenario != null) {
            mActivityScenario.close();
        }
    }

    private void initEditor(CreditCard card) {
        Bundle arguments = new Bundle();
        if (card != null) {
            String guid = card.getGUID();
            arguments.putString("guid", guid);
            when(mPersonalDataManager.getCreditCard(guid)).thenReturn(card);
        }

        Intent intent =
                SettingsIntentUtil.createIntent(
                        ContextUtils.getApplicationContext(),
                        AutofillServerCardEditor.class.getName(),
                        arguments);

        mActivityScenario = ActivityScenario.launch(intent);
        mActivityScenario.onActivity(
                activity -> {
                    mSettingsActivity = activity;
                });

        mCardEditor = (AutofillServerCardEditor) mSettingsActivity.getMainFragment();
    }

    @Test
    @MediumTest
    public void virtualCardEnrolled_virtualCardRemoveButtonShown() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_off_button_label)));
    }

    @Test
    @MediumTest
    public void virtualCardUnenrolledAndEligible_virtualCardAddButtonShown() {
        initEditor(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_button))
                .check(
                        matches(
                                withText(
                                        R.string
                                                .autofill_card_editor_virtual_card_turn_on_button_label)));
    }

    @Test
    @MediumTest
    public void virtualCardUnenrolledAndNotEligible_virtualCardLayoutNotShown() {
        initEditor(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD);

        onView(withId(R.id.virtual_card_ui))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
    }

    @Test
    @MediumTest
    public void
            virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollAccepted_enrollmentSuccessful() {
        initEditor(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

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

        // Verify that the correct CCT is opened.
        ShadowActivity shadowActivity = Shadows.shadowOf(mSettingsActivity);
        Intent cctIntent = shadowActivity.getNextStartedActivity();
        assertNotNull(cctIntent);
        assertEquals(
                ChromeStringConstants.AUTOFILL_VIRTUAL_CARD_ENROLLMENT_SUPPORT_URL,
                cctIntent.getDataString());

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
    }

    @Test
    @MediumTest
    public void
            virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollAccepted_enrollmentFailure() {
        initEditor(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

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
    }

    @Test
    @MediumTest
    public void virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollRejected() {
        initEditor(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

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
    }

    @Test
    @MediumTest
    public void
            virtualCardUnenrolledAndEligible_virtualCardAddButtonClicked_enrollAccepted_editorExited() {
        initEditor(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

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
        mActivityScenario.close();

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
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_dialogShown() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

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
                .inRoot(isDialog())
                .check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollCancelled() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

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
                .inRoot(isDialog())
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
    }

    @Test
    @MediumTest
    public void
            virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollAccepted_unenrollmentSuccessful() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

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
                .inRoot(isDialog())
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
    }

    @Test
    @MediumTest
    public void
            virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollAccepted_unenrollmentFailure() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

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
                .inRoot(isDialog())
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
    }

    @Test
    @MediumTest
    public void virtualCardEnrolled_virtualCardRemoveButtonClicked_unenrollAccepted_editorExited() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

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
                .inRoot(isDialog())
                .check(matches(isDisplayed()));

        // Click the positive button on unenrollment dialog.
        onView(
                        withText(
                                R.string
                                        .autofill_credit_card_editor_virtual_card_unenroll_dialog_positive_button_label))
                .perform(click());

        // Exit the editor.
        mActivityScenario.close();

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
    public void testAutofillPaymentMethodsDelegateLifecycleEvents() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        // Verify that the native delegate was initialized properly.
        verify(mNativeMock).init(any(Profile.class));

        mActivityScenario.close();

        // Ensure that the native delegate is cleaned up when the test has finished.
        verify(mNativeMock).cleanup(NATIVE_AUTOFILL_PAYMENTS_METHODS_DELEGATE);
    }

    @Test
    @MediumTest
    public void testCustomUrlForServerCardEditPage() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        mCardEditor.setServerCardEditLinkOpenerCallbackForTesting(
                mServerCardEditLinkOpenerCallback);

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
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({ChromeSwitches.USE_SANDBOX_WALLET_ENVIRONMENT})
    public void testCustomUrlForServerCardEditPage_sandboxEnabled() {
        initEditor(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        mCardEditor.setServerCardEditLinkOpenerCallbackForTesting(
                mServerCardEditLinkOpenerCallback);

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
                assertEquals("There should be only one clickable link", 1, spans.length);
                spans[0].onClick(view);
            }
        };
    }
}
