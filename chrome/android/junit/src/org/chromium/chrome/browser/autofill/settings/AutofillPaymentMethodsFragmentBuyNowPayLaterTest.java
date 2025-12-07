// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.settings;

import static com.google.common.truth.Truth.assertThat;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.any;
import static org.mockito.Mockito.when;

import android.content.pm.ActivityInfo;
import android.content.pm.ResolveInfo;
import android.os.Bundle;

import androidx.fragment.app.Fragment;
import androidx.fragment.app.FragmentFactory;
import androidx.fragment.app.testing.FragmentScenario;
import androidx.preference.Preference;

import org.jspecify.annotations.NonNull;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Features.DisableFeatures;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.autofill.PersonalDataManager;
import org.chromium.chrome.browser.autofill.PersonalDataManager.Iban;
import org.chromium.chrome.browser.autofill.PersonalDataManagerFactory;
import org.chromium.chrome.browser.device_reauth.BiometricStatus;
import org.chromium.chrome.browser.device_reauth.ReauthenticatorBridge;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.components.autofill.payments.BankAccount;
import org.chromium.components.payments.AndroidPaymentAppFactory;
import org.chromium.components.payments.PackageManagerDelegate;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.components.user_prefs.UserPrefsJni;

import java.util.Collections;
import java.util.List;

/** JUnit tests of the class {@link AutofillPaymentMethodsFragment: Buy Now Pay Later} */
@RunWith(BaseRobolectricTestRunner.class)
@DisableFeatures({
    ChromeFeatureList.AUTOFILL_ENABLE_SEPARATE_PIX_PREFERENCE_ITEM,
    ChromeFeatureList.AUTOFILL_SYNC_EWALLET_ACCOUNTS,
    ChromeFeatureList.AUTOFILL_ENABLE_CVC_STORAGE,
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_AMERICAN_EXPRESS,
    ChromeFeatureList.AUTOFILL_ENABLE_CARD_BENEFITS_FOR_BMO,
    ChromeFeatureList.AUTOFILL_ENABLE_FLAT_RATE_CARD_BENEFITS_FROM_CURINOS,
    ChromeFeatureList.AUTOFILL_ENABLE_LOYALTY_CARDS_FILLING,
    ChromeFeatureList.THIRD_PARTY_DISABLE_CHROME_AUTOFILL_SETTINGS_SCREEN
})
public class AutofillPaymentMethodsFragmentBuyNowPayLaterTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private static final String ALICEPAY_ACTIVITY_PACKAGE_NAME = "com.alicepay.app";
    private static final String ALICEPAY_ACTIVITY_NAME = "com.alicepay.app.WebPaymentActivity";

    private FragmentScenario<AutofillPaymentMethodsFragment> mScenario;
    private Preference mBnplPreference;

    @Mock private PersonalDataManager mPersonalDataManager;
    @Mock private Profile mProfile;
    @Mock private UserPrefs.Natives mUserPrefsNativesMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private ReauthenticatorBridge mReauthenticatorMock;
    @Mock private PackageManagerDelegate mPackageManagerDelegate;

    @Before
    public void setUp() {
        ProfileManager.setLastUsedProfileForTesting(mProfile);
        PersonalDataManagerFactory.setInstanceForTesting(mPersonalDataManager);
        UserPrefsJni.setInstanceForTesting(mUserPrefsNativesMock);
        when(mUserPrefsNativesMock.get(mProfile)).thenReturn(mPrefServiceMock);
        ReauthenticatorBridge.setInstanceForTesting(mReauthenticatorMock);
        when(mReauthenticatorMock.getBiometricAvailabilityStatus())
                .thenReturn(BiometricStatus.BIOMETRICS_AVAILABLE);
        when(mPersonalDataManager.getIbansForSettings()).thenReturn(new Iban[0]);
        when(mPersonalDataManager.shouldShowAddIbanButtonOnSettingsPage()).thenReturn(false);
        when(mPersonalDataManager.getCreditCardsForSettings()).thenReturn(Collections.emptyList());
        when(mPersonalDataManager.getMaskedBankAccounts()).thenReturn(new BankAccount[0]);
        ResolveInfo alicePay = new ResolveInfo();
        alicePay.activityInfo = new ActivityInfo();
        alicePay.activityInfo.packageName = ALICEPAY_ACTIVITY_PACKAGE_NAME;
        alicePay.activityInfo.name = ALICEPAY_ACTIVITY_NAME;
        when(mPackageManagerDelegate.getActivitiesThatCanRespondToIntent(any()))
                .thenReturn(List.of(alicePay));
        AndroidPaymentAppFactory.setPackageManagerDelegateForTest(mPackageManagerDelegate);
    }

    @After
    public void tearDown() {
        if (mScenario != null) {
            mScenario.close();
        }
    }

    private void launchAutofillPaymentMethodsFragment() {
        mScenario =
                FragmentScenario.launchInContainer(
                        AutofillPaymentMethodsFragment.class,
                        Bundle.EMPTY,
                        R.style.Theme_Chromium_Settings,
                        new FragmentFactory() {
                            @NonNull
                            @Override
                            public Fragment instantiate(
                                    @NonNull ClassLoader classLoader, @NonNull String className) {
                                Fragment fragment = super.instantiate(classLoader, className);
                                if (fragment instanceof AutofillPaymentMethodsFragment) {
                                    ((AutofillPaymentMethodsFragment) fragment)
                                            .setProfile(mProfile);
                                }
                                return fragment;
                            }
                        });
        mScenario.onFragment(
                fragment -> {
                    mBnplPreference =
                            (Preference)
                                    fragment.findPreference(
                                            AutofillPaymentMethodsFragment.PREF_BUY_NOW_PAY_LATER);
                });
    }

    // Test to verify that the buy now pay later preference is not displayed when autofill payment
    // method is disabled.
    @Test
    public void testBuyNowPayLaterPref_whenAutofillIsDisabled_notShown() {
        when(mPersonalDataManager.isAutofillPaymentMethodsEnabled()).thenReturn(false);
        when(mPersonalDataManager.shouldShowBnplSettings()).thenReturn(true);

        launchAutofillPaymentMethodsFragment();

        assertThat(mBnplPreference).isNull();
    }

    // Test to verify that the buy now pay later preference is not displayed when
    // `shouldShowBnplSettings` returns `false`.
    @Test
    public void testBuyNowPayLaterPref_whenShouldNotShowBnplSettings_notShown() {
        when(mPersonalDataManager.isAutofillPaymentMethodsEnabled()).thenReturn(true);
        when(mPersonalDataManager.shouldShowBnplSettings()).thenReturn(false);

        launchAutofillPaymentMethodsFragment();

        assertThat(mBnplPreference).isNull();
    }

    // Test to verify that the buy now pay later preference is displayed when autofill payment
    // method is enabled and `shouldShowBnplSettings` returns `true`.
    @Test
    public void testBuyNowPayLaterPref_whenShouldShowBnplSettingsAndAutofillIsEnabled_shown() {
        when(mPersonalDataManager.isAutofillPaymentMethodsEnabled()).thenReturn(true);
        when(mPersonalDataManager.shouldShowBnplSettings()).thenReturn(true);

        launchAutofillPaymentMethodsFragment();

        assertEquals(
                ContextUtils.getApplicationContext()
                        .getString(R.string.autofill_bnpl_settings_label),
                mBnplPreference.getTitle());
        assertEquals(
                AutofillPaymentMethodsFragment.PREF_BUY_NOW_PAY_LATER, mBnplPreference.getKey());
        assertEquals(AutofillBuyNowPayLaterFragment.class.getName(), mBnplPreference.getFragment());
    }
}
