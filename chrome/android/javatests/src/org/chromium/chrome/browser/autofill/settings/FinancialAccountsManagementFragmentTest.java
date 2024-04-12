// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.os.Bundle;

import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSize;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.CardIconSpecs;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.image_fetcher.test.TestImageFetcher;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for FinancialAccountsManagementFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
@EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SYNCING_OF_PIX_BANK_ACCOUNTS})
public class FinancialAccountsManagementFragmentTest {
    @Rule public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<FinancialAccountsManagementFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(FinancialAccountsManagementFragment.class);

    private static final GURL PIX_BANK_ACCOUNT_DISPLAY_ICON_URL = new GURL("http://example.com");
    private static final BankAccount PIX_BANK_ACCOUNT =
            new BankAccount.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100L)
                                    .setNickname("nickname")
                                    .setDisplayIconUrl(PIX_BANK_ACCOUNT_DISPLAY_ICON_URL)
                                    .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                    .build())
                    .setBankName("bank_name")
                    .setAccountNumberSuffix("account_number_suffix")
                    .setAccountType(AccountType.CHECKING)
                    .build();
    private static final Bitmap PIX_BANK_ACCOUNT_DISPLAY_ICON_BITMAP =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    PersonalDataManager personalDataManager =
                            AutofillTestHelper.getPersonalDataManagerForLastUsedProfile();
                    personalDataManager.setImageFetcherForTesting(
                            new TestImageFetcher(PIX_BANK_ACCOUNT_DISPLAY_ICON_BITMAP));
                    // Cache the test image in AutofillImageFetcher. Only cached images are returned
                    // immediately by the AutofillImageFetcher. If the image is not cached, it'll
                    // trigger an async fetch from the above TestImageFetcher and cache it for the
                    // next time.
                    personalDataManager
                            .getImageFetcherForTesting()
                            .addImageToCacheForTesting(
                                    PIX_BANK_ACCOUNT_DISPLAY_ICON_URL,
                                    PIX_BANK_ACCOUNT_DISPLAY_ICON_BITMAP,
                                    CardIconSpecs.create(
                                            ContextUtils.getApplicationContext(),
                                            CardIconSize.LARGE));
                });
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()
                            .getImageFetcherForTesting()
                            .clearCachedImagesForTesting();
                });
    }

    @Test
    @MediumTest
    public void testPixAccountAvailable_PixPrefShown() throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the switch preference for Pix is displayed.
        ChromeSwitchPreference pixSwitch =
                (ChromeSwitchPreference)
                        getPreferenceScreen(activity)
                                .findPreference(
                                        FinancialAccountsManagementFragment.PREFERENCE_KEY_PIX);
        assertThat(pixSwitch).isNotNull();
    }

    @Test
    @MediumTest
    public void testPixAccountNotAvailable_PixPrefNotShown() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the switch preference for Pix is not displayed.
        ChromeSwitchPreference pixSwitch =
                (ChromeSwitchPreference)
                        getPreferenceScreen(activity)
                                .findPreference(
                                        FinancialAccountsManagementFragment.PREFERENCE_KEY_PIX);
        assertThat(pixSwitch).isNull();
    }

    @Test
    @MediumTest
    public void testPixAccountShown() {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        String bankAccountPrefKey =
                String.format(
                        FinancialAccountsManagementFragment.PREFERENCE_KEY_PIX_BANK_ACCOUNT,
                        PIX_BANK_ACCOUNT.getInstrumentId());

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        String expectedPrefSummary =
                String.format(
                        "Pix  •  %s •• %s",
                        activity.getString(R.string.bank_account_type_checking),
                        PIX_BANK_ACCOUNT.getAccountNumberSuffix());
        Preference bankAccountPref =
                getPreferenceScreen(activity).findPreference(bankAccountPrefKey);
        assertThat(bankAccountPref.getTitle()).isEqualTo(PIX_BANK_ACCOUNT.getBankName());
        assertThat(bankAccountPref.getSummary()).isEqualTo(expectedPrefSummary);
        assertThat(bankAccountPref.getWidgetLayoutResource())
                .isEqualTo(R.layout.autofill_server_data_label);
        assertThat(((BitmapDrawable) bankAccountPref.getIcon()).getBitmap())
                .isEqualTo(PIX_BANK_ACCOUNT_DISPLAY_ICON_BITMAP);
    }

    @Test
    @MediumTest
    public void testPixAccountDisplayIconUrlAbsent_preferenceIconNotShown() {
        AutofillTestHelper.addMaskedBankAccount(
                new BankAccount.Builder()
                        .setPaymentInstrument(
                                new PaymentInstrument.Builder()
                                        .setInstrumentId(100L)
                                        .setNickname("nickname")
                                        .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                        .build())
                        .setBankName("bank_name")
                        .setAccountNumberSuffix("account_number_suffix")
                        .setAccountType(AccountType.CHECKING)
                        .build());
        String bankAccountPrefKey =
                String.format(
                        FinancialAccountsManagementFragment.PREFERENCE_KEY_PIX_BANK_ACCOUNT,
                        PIX_BANK_ACCOUNT.getInstrumentId());

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference bankAccountPref =
                getPreferenceScreen(activity).findPreference(bankAccountPrefKey);
        assertThat(bankAccountPref.getIcon()).isNull();
    }

    @Test
    @MediumTest
    public void testActivityTriggered_noArgs_emptyTitle() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        assertThat(activity.getTitle().toString()).isEmpty();
    }

    @Test
    @MediumTest
    public void testActivityTriggered_titlePresentInArgs_titleSet() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(FinancialAccountsManagementFragment.TITLE_KEY, "Title");

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);

        assertThat(activity.getTitle().toString()).isEqualTo("Title");
    }

    @Test
    @MediumTest
    public void testActivityTriggered_titleNotPresentInArgs_emptyTitle() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());

        assertThat(activity.getTitle().toString()).isEmpty();
    }

    // Test that the icon for the Pix bank account preference is not set if the icon is not already
    // cached in memory.
    @Test
    @MediumTest
    public void testPixAccountDisplayIconNotCached_prefIconNotSet() {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillTestHelper.getPersonalDataManagerForLastUsedProfile()
                            .getImageFetcherForTesting()
                            .clearCachedImagesForTesting();
                });
        String bankAccountPrefKey =
                String.format(
                        FinancialAccountsManagementFragment.PREFERENCE_KEY_PIX_BANK_ACCOUNT,
                        PIX_BANK_ACCOUNT.getInstrumentId());

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference bankAccountPref =
                getPreferenceScreen(activity).findPreference(bankAccountPrefKey);
        assertThat(bankAccountPref.getIcon()).isNull();
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((FinancialAccountsManagementFragment) activity.getMainFragment())
                .getPreferenceScreen();
    }
}
