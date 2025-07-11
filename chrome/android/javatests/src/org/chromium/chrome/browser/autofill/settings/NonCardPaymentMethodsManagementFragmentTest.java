// Copyright 2025 The Chromium Authors
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
import org.junit.runner.RunWith;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;

import org.chromium.base.ContextUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features.EnableFeatures;
import org.chromium.base.test.util.HistogramWatcher;
import org.chromium.base.test.util.RequiresRestart;
import org.chromium.chrome.browser.autofill.AutofillImageFetcher;
import org.chromium.chrome.browser.autofill.AutofillTestHelper;
import org.chromium.chrome.browser.autofill.AutofillUiUtils.IconSpecs;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsActivityTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.components.autofill.ImageSize;
import org.chromium.components.autofill.ImageType;
import org.chromium.components.autofill.payments.Ewallet;
import org.chromium.components.autofill.payments.PaymentInstrument;
import org.chromium.components.autofill.payments.PaymentRail;
import org.chromium.components.browser_ui.settings.ChromeSwitchPreference;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.url.GURL;

import java.util.concurrent.TimeoutException;

/** Instrumentation tests for NonCardPaymentMethodsManagementFragment. */
@RunWith(ChromeJUnit4ClassRunner.class)
@EnableFeatures({ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS})
@Batch(Batch.PER_CLASS)
public class NonCardPaymentMethodsManagementFragmentTest {
    @Rule public final AutofillTestRule rule = new AutofillTestRule();

    @Rule
    public final SettingsActivityTestRule<NonCardPaymentMethodsManagementFragment>
            mSettingsActivityTestRule =
                    new SettingsActivityTestRule<>(NonCardPaymentMethodsManagementFragment.class);

    @Rule
    public final MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.LENIENT);

    private static final GURL FINANCIAL_ACCOUNT_DISPLAY_ICON_URL = new GURL("http://example.com");
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
                    getPrefService().setBoolean(Pref.FACILITATED_PAYMENTS_EWALLET, true);
                });
    }

    @After
    public void tearDown() throws TimeoutException {
        mAutofillTestHelper.clearAllDataForTesting();
    }

    // Test that when eWallet accounts are available the eWallet preference toggle is shown.
    @Test
    @MediumTest
    public void testEwalletAccountAvailable_eWalletSwitchShown() throws Exception {
        AutofillTestHelper.addEwallet(EWALLET_ACCOUNT);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity();
        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        assertThat(eWalletSwitch).isNotNull();
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

    // Test that available eWallet accounts are listed as preference items.
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

    @Test
    @MediumTest
    public void testFragmentShown_histogramLogged() {
        var fragmentShownHistogram =
                HistogramWatcher.newSingleRecordWatcher(
                        NonCardPaymentMethodsManagementFragment.FRAGMENT_SHOWN_HISTOGRAM, true);
        mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        fragmentShownHistogram.assertExpected();
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
                        NonCardPaymentMethodsManagementFragment
                                .NON_CARD_PAYMENT_METHODS_EWALLET_TOGGLE_UPDATED_HISTOGRAM,
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
                        NonCardPaymentMethodsManagementFragment
                                .NON_CARD_PAYMENT_METHODS_EWALLET_TOGGLE_UPDATED_HISTOGRAM,
                        false);
        SettingsActivity activity = mSettingsActivityTestRule.startSettingsActivity(new Bundle());
        ChromeSwitchPreference eWalletSwitch = getEwalletSwitchPreference(activity);
        assertThat(eWalletSwitch.isChecked()).isTrue();
        // Set the eWallet toggle to off.
        rule.clickOnPreferenceAndWait(eWalletSwitch);
        eWalletToggleEnabledHistogram.assertExpected();
    }

    private static PreferenceScreen getPreferenceScreen(SettingsActivity activity) {
        return ((NonCardPaymentMethodsManagementFragment) activity.getMainFragment())
                .getPreferenceScreen();
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(ProfileManager.getLastUsedRegularProfile());
    }

    private static ChromeSwitchPreference getEwalletSwitchPreference(SettingsActivity activity) {
        return (ChromeSwitchPreference)
                getPreferenceScreen(activity)
                        .findPreference(
                                NonCardPaymentMethodsManagementFragment.PREFERENCE_KEY_EWALLET);
    }

    private static Preference getEwalletPreference(SettingsActivity activity, Ewallet eWallet) {
        String eWalletPrefKey =
                String.format(
                        NonCardPaymentMethodsManagementFragment.PREFERENCE_KEY_EWALLET_ACCOUNT,
                        eWallet.getInstrumentId());
        return getPreferenceScreen(activity).findPreference(eWalletPrefKey);
    }
}
