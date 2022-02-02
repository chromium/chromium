// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isChecked;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.isNotChecked;
import static androidx.test.espresso.matcher.ViewMatchers.withEffectiveVisibility;
import static androidx.test.espresso.matcher.ViewMatchers.withId;

import android.os.Bundle;

import androidx.test.espresso.matcher.ViewMatchers.Visibility;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.PersonalDataManager.CreditCard;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.autofill.VirtualCardEnrollmentState;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for AutofillServerCardEditor.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class AutofillServerCardEditorTest {
    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule
    public final AutofillTestRule rule = new AutofillTestRule();
    @Rule
    public final SettingsActivityTestRule<AutofillServerCardEditor> mSettingsActivityTestRule =
            new SettingsActivityTestRule<>(AutofillServerCardEditor.class);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_ENROLLED_CARD = new CreditCard(
            /* guid= */ "1", /* origin= */ "", /* isLocal= */ false, /* isCached= */ true,
            /* name= */ "John Doe", /* number= */ "4444333322221111", /* obfuscatedNumber= */ "",
            /* month= */ "5", AutofillTestHelper.nextYear(), /* basicCardIssuerNetwork = */ "visa",
            /* issuerIconDrawableId= */ 0, /* billingAddressId= */ "", /* serverId= */ "",
            /* instrumentId= */ 0, /* cardLabel= */ "", /* nickname= */ "", /* cardArtUrl= */ null,
            /* virtualCardEnrollmentState= */ VirtualCardEnrollmentState.ENROLLED);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD =
            new CreditCard(/* guid= */ "2", /* origin= */ "", /* isLocal= */ false,
                    /* isCached= */ true, /* name= */ "John Doe", /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork = */ "visa", /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "", /* serverId= */ "", /* instrumentId= */ 0,
                    /* cardLabel= */ "", /* nickname= */ "", /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */
                    VirtualCardEnrollmentState.UNENROLLED_AND_ELIGIBLE);

    private static final CreditCard SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD =
            new CreditCard(/* guid= */ "3", /* origin= */ "", /* isLocal= */ false,
                    /* isCached= */ true, /* name= */ "John Doe", /* number= */ "4444333322221111",
                    /* obfuscatedNumber= */ "", /* month= */ "5", AutofillTestHelper.nextYear(),
                    /* basicCardIssuerNetwork = */ "visa", /* issuerIconDrawableId= */ 0,
                    /* billingAddressId= */ "", /* serverId= */ "", /* instrumentId= */ 0,
                    /* cardLabel= */ "", /* nickname= */ "", /* cardArtUrl= */ null,
                    /* virtualCardEnrollmentState= */
                    VirtualCardEnrollmentState.UNENROLLED_AND_NOT_ELIGIBLE);

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardEnrolled_virtualCardSwitchOn() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_switch)).check(matches(isChecked()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardUnenrolledAndEligible_virtualCardSwitchOff() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD);

        mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_ELIGIBLE_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui)).check(matches(isDisplayed()));
        onView(withId(R.id.virtual_card_enrollment_switch)).check(matches(isNotChecked()));
    }

    @Test
    @MediumTest
    @Features.EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void virtualCardUnenrolledAndNotEligible_virtualCardLayoutNotShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(
                SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD);

        mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_UNENROLLED_AND_NOT_ELIGIBLE_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
    }

    @Test
    @MediumTest
    @Features.DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_UPDATE_VIRTUAL_CARD_ENROLLMENT})
    public void updateEnrollmentFeatureDisabled_virtualCardLayoutNotShown() throws Exception {
        mAutofillTestHelper.addServerCreditCard(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD);

        mSettingsActivityTestRule.startSettingsActivity(
                fragmentArgs(SAMPLE_VIRTUAL_CARD_ENROLLED_CARD.getGUID()));

        onView(withId(R.id.virtual_card_ui))
                .check(matches(withEffectiveVisibility(Visibility.GONE)));
    }

    private Bundle fragmentArgs(String guid) {
        Bundle args = new Bundle();
        args.putString(AutofillEditorBase.AUTOFILL_GUID, guid);
        return args;
    }
}
