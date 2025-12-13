// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.mockito.ArgumentMatchers.eq;
import static org.mockito.Mockito.verify;

import android.graphics.Bitmap;
import android.graphics.Bitmap.Config;
import android.graphics.Canvas;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.os.Bundle;

import androidx.core.content.res.ResourcesCompat;
import androidx.preference.Preference;
import androidx.preference.PreferenceScreen;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillImageFetcherUtils;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.IconSpecs;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.ImageType;
import org.chromium.components.autofill.payments.AccountType;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for FinancialAccountsManagementFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS})
@DisableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
@Batch(Batch.PER_CLASS)
public class FinancialAccountsManagementFragmentTest {
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<FinancialAccountsManagementFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(FinancialAccountsManagementFragment.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    @Mock private Callback<String> mFinancialAccountManageLinkOpenerCallback;

    private static final GURL FINANCIAL_ACCOUNT_DISPLAY_ICON_URL = new GURL("http://example.com");
    private static final BankAccount PIX_BANK_ACCOUNT =
            new BankAccount.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100L)
                                    .setNickname("nickname")
                                    .setDisplayIconUrl(FINANCIAL_ACCOUNT_DISPLAY_ICON_URL)
                                    .setSupportedPaymentRails(new int[] {PaymentRail.PIX})
                                    .build())
                    .setBankName("bank_name")
                    .setAccountNumberSuffix("account_number_suffix")
                    .setAccountType(AccountType.CHECKING)
                    .build();
    private static final Bitmap FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP =
            Bitmap.createBitmap(100, 200, Bitmap.Config.ARGB_8888);

    private static final Ewallet EWALLET_ACCOUNT =
            new Ewallet.Builder()
                    .setPaymentInstrument(
                            new PaymentInstrument.Builder()
                                    .setInstrumentId(100)
                                    .setNickname("nickname")
                                    .setDisplayIconUrl(FINANCIAL_ACCOUNT_DISPLAY_ICON_URL)
                                    .setSupportedPaymentRails(
                                            new int[] {PaymentRail.PAYMENT_HYPERLINK})
                                    .setIsFidoEnrolled(true)
                                    .build())
                    .setEwalletName("eWallet name")
                    .setAccountDisplayName("Ewallet account display name")
                    .build();

    private AutofillTestHelper mAutofillTestHelper;

    @Before
    public void setUp() {
        mAutofillTestHelper = new AutofillTestHelper();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillImageFetcher imageFetcher =
                            AutofillTestHelper.getAutofillImageFetcherForLastUsedProfile();
                    IconSpecs specs =
                            IconSpecs.create(
                                    ContextUtils.getApplicationContext(),
                                    ImageType.CREDIT_CARD_ART_IMAGE,
                                    ImageSize.LARGE);
                    // Cache the test image in AutofillImageFetcher. Only cached images are returned
                    // immediately by the AutofillImageFetcher.
                    imageFetcher.addImageToCacheForTesting(
                            specs.getResolvedIconUrl(FINANCIAL_ACCOUNT_DISPLAY_ICON_URL),
                            FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP);
                    imageFetcher.addImageToCacheForTesting(
                            AutofillImageFetcherUtils.getPixAccountImageUrlWithParams(
                                    FINANCIAL_ACCOUNT_DISPLAY_ICON_URL),
                            FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP);
                    // Set the eWallet and Pix pref to true.
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_EWALLET, true);
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_PIX, true);
                });
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
    }

    // Test that when both eWallet and Pix are available, and the
    // AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM is on, only the Pix preference toggle is shown.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void separatePixPreferenceItem_testEwalletAndPixAccountAvailable_onlyPixSwitchShown()
            throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(eWalletSwitch).isNull();
        assertThat(pixSwitch).isNotNull();
    }

    // Test that when both eWallet and Pix are available the eWallet and Pix
    // preference toggles are shown.
    @Test
    @MediumTest
    public void testEwalletAndPixAccountAvailable_bothSwitchesShown() throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(eWalletSwitch).isNotNull();
        assertThat(pixSwitch).isNotNull();
    }

    // Test that when eWallet accounts are available and Pix accounts are not, and the
    // AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM is on, no toggles are shown.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void separatePixPreferenceItem_testEwalletAccountAvailable_nothingShown()
            throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(eWalletSwitch).isNull();
        assertThat(pixSwitch).isNull();
    }

    // Test that when eWallet accounts are available and Pix accounts are not, only
    // the eWallet preference toggle is shown.
    @Test
    @MediumTest
    public void testEwalletAccountAvailable_eWalletSwitchShown() throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(eWalletSwitch).isNotNull();
        assertThat(pixSwitch).isNull();
    }

    // Test that when eWallet accounts are not available the eWallet preference toggle is not shown.
    @Test
    @MediumTest
    public void testEwalletAccountNotAvailable_eWalletSwitchNotShown() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the switch preference for eWallet is not displayed.
        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        assertThat(eWalletSwitch).isNull();
    }

    // Test that when eWallet preference is set to true, the eWallet toggle is checked.
    @Test
    @MediumTest
    public void testEwalletPrefEnabled_eWalletSwitchEnabled() throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_EWALLET, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        assertThat(eWalletSwitch.isChecked()).isTrue();
    }

    // Test that when the eWallet preference is set to false, the eWallet toggle is not
    // checked.
    @Test
    @MediumTest
    public void testEwalletPrefDisabled_eWalletSwitchDisabled() throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_EWALLET, false);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        assertThat(eWalletSwitch.isChecked()).isFalse();
    }

    // Test that when both Pix and eWallet accounts are available, and the
    // AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM is on, only Pix accounts are shown.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void separatePixPreferenceItem_testEwalletAndPixAccountAvailable_onlyPixAccountShown()
            throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference eWalletPref = getEwalletPreference(activity, EWALLET_ACCOUNT);
        assertThat(eWalletPref).isNull();

        String expectedPixItemSummary =
                String.format(
                        "Pix  •  %s ••••%s",
                        activity.getString(R.string.bank_account_type_checking),
                        PIX_BANK_ACCOUNT.getAccountNumberSuffix());
        Preference bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);
        assertThat(bankAccountPref.getTitle()).isEqualTo(PIX_BANK_ACCOUNT.getBankName());
        assertThat(bankAccountPref.getSummary()).isEqualTo(expectedPixItemSummary);
        assertThat(bankAccountPref.getWidgetLayoutResource())
                .isEqualTo(R.layout.autofill_server_data_label);
        assertThat(((BitmapDrawable) bankAccountPref.getIcon()).getBitmap())
                .isEqualTo(FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP);
    }

    @Test
    @MediumTest
    public void testEwalletAndPixAccountShown() throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        String expectedEwalletItemSummary =
                String.format("eWallet  •  %s", EWALLET_ACCOUNT.getAccountDisplayName());
        Preference eWalletPref = getEwalletPreference(activity, EWALLET_ACCOUNT);
        assertThat(eWalletPref.getTitle()).isEqualTo(EWALLET_ACCOUNT.getEwalletName());
        assertThat(eWalletPref.getSummary()).isEqualTo(expectedEwalletItemSummary);
        assertThat(eWalletPref.getWidgetLayoutResource())
                .isEqualTo(R.layout.autofill_server_data_label);
        assertThat(((BitmapDrawable) eWalletPref.getIcon()).getBitmap())
                .isEqualTo(FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP);

        String expectedPixItemSummary =
                String.format(
                        "Pix  •  %s ••••%s",
                        activity.getString(R.string.bank_account_type_checking),
                        PIX_BANK_ACCOUNT.getAccountNumberSuffix());
        Preference bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);
        assertThat(bankAccountPref.getTitle()).isEqualTo(PIX_BANK_ACCOUNT.getBankName());
        assertThat(bankAccountPref.getSummary()).isEqualTo(expectedPixItemSummary);
        assertThat(bankAccountPref.getWidgetLayoutResource())
                .isEqualTo(R.layout.autofill_server_data_label);
        assertThat(((BitmapDrawable) bankAccountPref.getIcon()).getBitmap())
                .isEqualTo(FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP);
    }

    // Test that when only eWallet accounts are available, and the
    // AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM is on, no accounts are shown.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void separatePixPreferenceItem_eWalletAccountAvailable_nothingShown() {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference eWalletPref = getEwalletPreference(activity, EWALLET_ACCOUNT);
        assertThat(eWalletPref).isNull();
    }

    @Test
    @MediumTest
    public void testEwalletAccountShown() {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        String expectedPrefSummary =
                String.format("eWallet  •  %s", EWALLET_ACCOUNT.getAccountDisplayName());
        Preference eWalletPref = getEwalletPreference(activity, EWALLET_ACCOUNT);
        assertThat(eWalletPref.getTitle()).isEqualTo(EWALLET_ACCOUNT.getEwalletName());
        assertThat(eWalletPref.getSummary()).isEqualTo(expectedPrefSummary);
        assertThat(eWalletPref.getWidgetLayoutResource())
                .isEqualTo(R.layout.autofill_server_data_label);
        assertThat(((BitmapDrawable) eWalletPref.getIcon()).getBitmap())
                .isEqualTo(FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP);
    }

    // Test that when Pix accounts are available and eWallet accounts are not, and the
    // AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM is on, only the Pix preference toggle is
    // shown.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void separatePixPreferenceItem_testPixAccountAvailable_pixSwitchShown()
            throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(eWalletSwitch).isNull();
        assertThat(pixSwitch).isNotNull();
    }

    // Test that when Pix accounts are available and eWallets accounts are not, only
    // the Pix preference toggle is shown.
    @Test
    @MediumTest
    public void testPixAccountAvailable_pixSwitchShown() throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the Pix switch preference is displayed and the eWallet switch
        // is not..
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        assertThat(pixSwitch).isNotNull();
        assertThat(eWalletSwitch).isNull();
    }

    // Test that when Pix accounts are not available the Pix preference toggle is not shown.
    @Test
    @MediumTest
    public void testPixAccountNotAvailable_pixSwitchNotShown() throws Exception {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the switch preference for Pix is not displayed.
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(pixSwitch).isNull();
    }

    // Test that when Pix profile preference is set to true, the Pix toggle is checked.
    @Test
    @MediumTest
    public void testPixPrefEnabled_pixSwitchEnabled() throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_PIX, true);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the switch preference for Pix is displayed and is checked.
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(pixSwitch.isChecked()).isTrue();
    }

    // Test that when the Pix profile preference is set to false, the Pix toggle is not checked.
    @Test
    @MediumTest
    public void testPixPrefDisabled_pixSwitchDisabled() throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_PIX, false);
                });

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        // Verify that the switch preference for Pix is displayed the is not checked.
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(pixSwitch.isChecked()).isFalse();
    }

    // Test that when only Pix accounts are available, and the
    // AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM is on, Pix accounts are shown.
    @Test
    @MediumTest
    @EnableFeatures({ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM})
    public void separatePixPreferenceItem_pixAccountAvailable_pixAccountShown() {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        String expectedPrefSummary =
                String.format(
                        "Pix  •  %s ••••%s",
                        activity.getString(R.string.bank_account_type_checking),
                        PIX_BANK_ACCOUNT.getAccountNumberSuffix());
        Preference bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);
        assertThat(bankAccountPref.getTitle()).isEqualTo(PIX_BANK_ACCOUNT.getBankName());
        assertThat(bankAccountPref.getSummary()).isEqualTo(expectedPrefSummary);
        assertThat(bankAccountPref.getWidgetLayoutResource())
                .isEqualTo(R.layout.autofill_server_data_label);
        assertThat(((BitmapDrawable) bankAccountPref.getIcon()).getBitmap())
                .isEqualTo(FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP);
    }

    @Test
    @MediumTest
    public void testPixAccountShown() {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        String expectedPrefSummary =
                String.format(
                        "Pix  •  %s ••••%s",
                        activity.getString(R.string.bank_account_type_checking),
                        PIX_BANK_ACCOUNT.getAccountNumberSuffix());
        Preference bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);
        assertThat(bankAccountPref.getTitle()).isEqualTo(PIX_BANK_ACCOUNT.getBankName());
        assertThat(bankAccountPref.getSummary()).isEqualTo(expectedPrefSummary);
        assertThat(bankAccountPref.getWidgetLayoutResource())
                .isEqualTo(R.layout.autofill_server_data_label);
        assertThat(((BitmapDrawable) bankAccountPref.getIcon()).getBitmap())
                .isEqualTo(FINANCIAL_ACCOUNT_DISPLAY_ICON_BITMAP);
    }

    @Test
    @MediumTest
    public void testPixAccountDisplayIconUrlAbsent_defaulAccountBalanceIconShown() {
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
        assertThat(
                        convertDrawableToBitmap(bankAccountPref.getIcon())
                                .sameAs(
                                        convertDrawableToBitmap(
                                                ResourcesCompat.getDrawable(
                                                        activity.getResources(),
                                                        R.drawable.ic_account_balance,
                                                        activity.getTheme()))))
                .isTrue();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testActivityTriggered_noArgs_emptyTitle() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        assertThat(activity.getTitle().toString()).isEmpty();
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testActivityTriggered_titlePresentInArgs_titleSet() {
        Bundle fragmentArgs = new Bundle();
        fragmentArgs.putString(FinancialAccountsManagementFragment.TITLE_KEY, "Title");

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(fragmentArgs);

        assertThat(activity.getTitle().toString()).isEqualTo("Title");
    }

    @Test
    @MediumTest
    @DisableFeatures(ChromeFeatureList.SETTINGS_MULTI_COLUMN)
    public void testActivityTriggered_titleNotPresentInArgs_emptyTitle() {
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());

        assertThat(activity.getTitle().toString()).isEmpty();
    }

    // Test that the icon for the Pix bank account preference is not set if the icon is not already
    // cached in memory.
    @Test
    @MediumTest
    public void testPixAccountDisplayIconNotCached_prefIconSetToDefault() {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    AutofillTestHelper.getAutofillImageFetcherForLastUsedProfile()
                            .clearCachedImagesForTesting();
                });
        String bankAccountPrefKey =
                String.format(
                        FinancialAccountsManagementFragment.PREFERENCE_KEY_PIX_BANK_ACCOUNT,
                        PIX_BANK_ACCOUNT.getInstrumentId());

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();

        Preference bankAccountPref =
                getPreferenceScreen(activity).findPreference(bankAccountPrefKey);
        assertThat(
                        convertDrawableToBitmap(bankAccountPref.getIcon())
                                .sameAs(
                                        convertDrawableToBitmap(
                                                ResourcesCompat.getDrawable(
                                                        activity.getResources(),
                                                        R.drawable.ic_account_balance,
                                                        activity.getTheme()))))
                .isTrue();
    }

    // Test that eWallet accounts are removed when the eWallet toggle is turned
    // off.
    @Test
    @MediumTest
    public void testEwalletSwitchDisabled_eWalletRowItemsRemoved() throws TimeoutException {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        Preference eWalletPref = getEwalletPreference(activity, EWALLET_ACCOUNT);
        assertThat(eWalletPref).isNotNull();

        // Set the eWallet toggle to off.
        rule.clickOnPreferenceAndWait(eWalletSwitch);

        // Verify that the eWallet preference is now null.
        eWalletPref = getEwalletPreference(activity, EWALLET_ACCOUNT);
        assertThat(eWalletPref).isNull();
    }

    // Test that eWallet accounts are added when the eWallet toggle is turned on.
    @Test
    @MediumTest
    public void testEwalletSwitchEnabled_eWalletRowItemsAdded() throws TimeoutException {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_EWALLET, false);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        Preference eWalletPref = getEwalletPreference(activity, EWALLET_ACCOUNT);
        assertThat(eWalletPref).isNull();

        // Set the eWallet toggle to on.
        rule.clickOnPreferenceAndWait(eWalletSwitch);
        // Verify that the eWallet account preference is now not null.
        eWalletPref = getEwalletPreference(activity, EWALLET_ACCOUNT);
        assertThat(eWalletPref).isNotNull();
    }

    // Test that Pix bank accounts are removed when the Pix toggle is turned off.
    @Test
    @MediumTest
    @RequiresRestart("crbug.com/344671557")
    public void testPixSwitchDisabled_bankAccountPrefsRemoved() throws TimeoutException {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        Preference bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);
        assertThat(bankAccountPref).isNotNull();

        // Set the Pix toggle to off.
        rule.clickOnPreferenceAndWait(pixSwitch);

        // Verify that the bank account preference is now null.
        bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);
        assertThat(bankAccountPref).isNull();
    }

    // Test that Pix bank accounts are added when the Pix toggle is turned on.
    @Test
    @MediumTest
    public void testPixSwitchEnabled_bankAccountPrefsAdded() throws TimeoutException {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_PIX, false);
                });
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);

        Preference bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);
        assertThat(bankAccountPref).isNull();

        // Set the Pix toggle to on.
        rule.clickOnPreferenceAndWait(pixSwitch);

        // Verify that the bank account preference is now not null.
        bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);
        assertThat(bankAccountPref).isNotNull();
    }

    @Test
    @MediumTest
    public void testFragmentShown_histogramLogged() {
        var fragmentShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FinancialAccountsManagementFragment.FRAGMENT_SHOWN_HISTOGRAM, true);

        mSettingsActivityTestRule.startSettingsActivity(new Bundle());

        fragmentShownHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @RequiresRestart("crbug.com/344671557")
    public void testPixToggleTurnedOn_histogramLogged() {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_PIX, false);
                });
        var pixToggleEnabledHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FinancialAccountsManagementFragment
                                .FACILITATED_PAYMENTS_PIX_TOGGLE_UPDATED_HISTOGRAM,
                        true);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(pixSwitch.isChecked()).isFalse();

        // Set the Pix toggle to on.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    pixSwitch.performClick();
                });

        pixToggleEnabledHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testPixToggleTurnedOff_histogramLogged() throws TimeoutException {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);
        var pixToggleDisabledHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FinancialAccountsManagementFragment
                                .FACILITATED_PAYMENTS_PIX_TOGGLE_UPDATED_HISTOGRAM,
                        false);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference pixSwitch = getPixSwitchPreference(activity);
        assertThat(pixSwitch.isChecked()).isTrue();

        // Set the Pix toggle to off.
        rule.clickOnPreferenceAndWait(pixSwitch);

        pixToggleDisabledHistogram.assertExpected();
    }

    @Test
    @MediumTest
    @RequiresRestart("crbug.com/344671557")
    public void testEwalletToggleTurnedOn_histogramLogged() {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_EWALLET, false);
                });
        var eWalletToggleEnabledHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FinancialAccountsManagementFragment
                                .FACILITATED_PAYMENTS_EWALLET_TOGGLE_UPDATED_HISTOGRAM,
                        true);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        assertThat(eWalletSwitch.isChecked()).isFalse();

        // Set the eWallet toggle to on.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    eWalletSwitch.performClick();
                });

        eWalletToggleEnabledHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testEwalletToggleTurnedOff_histogramLogged() throws TimeoutException {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        var eWalletToggleEnabledHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        FinancialAccountsManagementFragment
                                .FACILITATED_PAYMENTS_EWALLET_TOGGLE_UPDATED_HISTOGRAM,
                        false);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        assertThat(eWalletSwitch.isChecked()).isTrue();

        // Set the eWallet toggle to off.
        rule.clickOnPreferenceAndWait(eWalletSwitch);

        eWalletToggleEnabledHistogram.assertExpected();
    }

    @Test
    @MediumTest
    public void testPixAccountPrefClicked_triggersOpeningManagePaymentMethodPage()
            throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        mSettingsActivityTestRule
                .getFragment()
                .setFinancialAccountManageLinkOpenerCallbackForTesting(
                        mFinancialAccountManageLinkOpenerCallback);
        Preference bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);

        ThreadUtils.runOnUiThreadBlocking(bankAccountPref::performClick);

        verify(mFinancialAccountManageLinkOpenerCallback)
                .onResult(
                        eq(
                                "https://pay.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods&id="
                                        + PIX_BANK_ACCOUNT.getInstrumentId()));
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add({ChromeSwitches.USE_SANDBOX_WALLET_ENVIRONMENT})
    public void testSandboxPixAccountPrefClicked_triggersOpeningManagePaymentMethodPage()
            throws Exception {
        AutofillTestHelper.addMaskedBankAccount(PIX_BANK_ACCOUNT);

        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        mSettingsActivityTestRule
                .getFragment()
                .setFinancialAccountManageLinkOpenerCallbackForTesting(
                        mFinancialAccountManageLinkOpenerCallback);
        Preference bankAccountPref = getBankAccountPreference(activity, PIX_BANK_ACCOUNT);

        ThreadUtils.runOnUiThreadBlocking(bankAccountPref::performClick);

        verify(mFinancialAccountManageLinkOpenerCallback)
                .onResult(
                        eq(
                                "https://pay.sandbox.google.com/pay?p=paymentmethods&utm_source=chrome&utm_medium=settings&utm_campaign=payment_methods&id="
                                        + PIX_BANK_ACCOUNT.getInstrumentId()));
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((FinancialAccountsManagementFragment) activity.getMainFragment())
                .getPreferenceScreen();
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    private static ChromeSwitchPreference getEwalletSwitchPreference(SettingsActivity activity) {
        return (ChromeSwitchPreference)
                getPreferenceScreen(activity)
                        .findPreference(FinancialAccountsManagementFragment.PREFERENCE_KEY_EWALLET);
    }

    private static ChromeSwitchPreference getPixSwitchPreference(SettingsActivity activity) {
        return (ChromeSwitchPreference)
                getPreferenceScreen(activity)
                        .findPreference(FinancialAccountsManagementFragment.PREFERENCE_KEY_PIX);
    }

    private static Preference getEwalletPreference(SettingsActivity activity, Ewallet eWallet) {
        String eWalletPrefKey =
                String.format(
                        FinancialAccountsManagementFragment.PREFERENCE_KEY_EWALLET_ACCOUNT,
                        eWallet.getInstrumentId());
        return getPreferenceScreen(activity).findPreference(eWalletPrefKey);
    }

    private static Preference getBankAccountPreference(
            SettingsActivity activity, BankAccount bankAccount) {
        String bankAccountPrefKey =
                String.format(
                        FinancialAccountsManagementFragment.PREFERENCE_KEY_PIX_BANK_ACCOUNT,
                        bankAccount.getInstrumentId());
        return getPreferenceScreen(activity).findPreference(bankAccountPrefKey);
    }

    private static Bitmap convertDrawableToBitmap(Drawable drawable) {
        Bitmap bitmap =
                Bitmap.createBitmap(
                        drawable.getIntrinsicWidth(),
                        drawable.getIntrinsicHeight(),
                        Config.ARGB_8888);
        Canvas canvas = new Canvas(bitmap);
        drawable.setBounds(0, 0, canvas.getWidth(), canvas.getHeight());
        drawable.draw(canvas);
        return bitmap;
    }
}
