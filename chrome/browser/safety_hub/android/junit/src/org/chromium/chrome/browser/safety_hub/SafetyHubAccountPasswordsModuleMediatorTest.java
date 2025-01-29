// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.ArgumentMatchers.any;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;

import androidx.annotation.DrawableRes;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.signin.services.IdentityServicesProvider;
import org.chromium.chrome.browser.signin.services.SigninManager;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.signin.base.CoreAccountInfo;
import org.chromium.components.signin.base.GaiaId;
import org.chromium.components.signin.identitymanager.ConsentLevel;
import org.chromium.components.signin.identitymanager.IdentityManager;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubAccountPasswordsModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubAccountPasswordsModuleMediatorTest {
    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;
    private static final @DrawableRes int MANAGED_ICON = R.drawable.ic_business;
    private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;

    private static final String TEST_EMAIL_ADDRESS = "test@email.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private SafetyHubExpandablePreference mPreference;
    private SafetyHubAccountPasswordsModuleMediator mModuleMediator;

    @Mock private SafetyHubModuleMediatorDelegate mMediatorDelegateMock;
    @Mock private SafetyHubModuleDelegate mModuleDelegateMock;
    @Mock private PrefService mPrefServiceMock;
    @Mock private SafetyHubFetchService mSafetyHubFetchServiceMock;
    @Mock private SigninManager mSigninManagerMock;
    @Mock private Profile mProfile;
    @Mock private IdentityServicesProvider mIdentityServicesProviderMock;
    @Mock private IdentityManager mIdentityManager;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        doReturn(mIdentityManager).when(mIdentityServicesProviderMock).getIdentityManager(mProfile);
        IdentityServicesProvider.setInstanceForTests(mIdentityServicesProviderMock);
        mockPasswordCounts(0, 0, 0);
        mockTotalPasswordsCount(0);
        mockSignedInState(false);

        mModuleMediator =
                new SafetyHubAccountPasswordsModuleMediator(
                        mPreference,
                        mMediatorDelegateMock,
                        mModuleDelegateMock,
                        mPrefServiceMock,
                        mSafetyHubFetchServiceMock,
                        mSigninManagerMock,
                        mProfile);
        mModuleMediator.setUpModule();
    }

    public void mockSignedInState(boolean isSignedIn) {
        when(mIdentityManager.hasPrimaryAccount(ConsentLevel.SIGNIN)).thenReturn(isSignedIn);
        when(mIdentityManager.getPrimaryAccountInfo(ConsentLevel.SIGNIN))
                .thenReturn(
                        isSignedIn
                                ? CoreAccountInfo.createFromEmailAndGaiaId(
                                        TEST_EMAIL_ADDRESS, new GaiaId("0"))
                                : null);
        if (!isSignedIn) {
            doReturn(-1).when(mPrefServiceMock).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        }
    }

    public void mockTotalPasswordsCount(int totalPasswordsCount) {
        doReturn(totalPasswordsCount).when(mModuleDelegateMock).getAccountPasswordsCount(any());
    }

    private void mockPasswordCounts(int compromised, int weak, int reused) {
        doReturn(compromised).when(mPrefServiceMock).getInteger(Pref.BREACHED_CREDENTIALS_COUNT);
        doReturn(weak).when(mPrefServiceMock).getInteger(Pref.WEAK_CREDENTIALS_COUNT);
        doReturn(reused).when(mPrefServiceMock).getInteger(Pref.REUSED_CREDENTIALS_COUNT);
    }

    private void mockManaged(boolean isManaged) {
        doReturn(!isManaged).when(mPrefServiceMock).getBoolean(Pref.CREDENTIALS_ENABLE_SERVICE);
        doReturn(isManaged)
                .when(mPrefServiceMock)
                .isManagedPreference(Pref.CREDENTIALS_ENABLE_SERVICE);
    }

    @Test
    public void noCompromisedPasswords() {
        int totalPasswordsCount = 5;
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(true);
        mockManaged(false);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_passwords_title);
        String expectedSummary =
                mActivity.getString(
                        R.string.safety_hub_password_check_time_recently, TEST_EMAIL_ADDRESS);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void noCompromisedPasswords_managed() {
        int totalPasswordsCount = 5;
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(true);
        mockManaged(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_passwords_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void noCompromisedPasswords_weakAndReusedPasswordsExists_enabled() {
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        int totalPasswordsCount = 5;
        mockPasswordCounts(
                /* compromised= */ 0,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_reused_weak_passwords_title);
        // Reused passwords take priority over weak passwords in the UI.
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_reused_passwords_summary,
                                reusedPasswordsCount,
                                reusedPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void noCompromisedPasswords_weakAndReusedPasswordsExists_managed_enabled() {
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        int totalPasswordsCount = 5;
        mockPasswordCounts(
                /* compromised= */ 0,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_reused_weak_passwords_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void noCompromisedPasswords_weakPasswordsExists_enabled() {
        int weakPasswordsCount = 1;
        int totalPasswordsCount = 5;
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ weakPasswordsCount, /* reused= */ 0);
        mockSignedInState(true);
        mockManaged(false);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_reused_weak_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                weakPasswordsCount,
                                weakPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void noCompromisedPasswords_weakPasswordsExists__managed_enabled() {
        int weakPasswordsCount = 1;
        int totalPasswordsCount = 5;
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ weakPasswordsCount, /* reused= */ 0);
        mockSignedInState(true);
        mockManaged(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_reused_weak_passwords_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void noCompromisedPasswords_weakAndReusedPasswordsExists_disabled() {
        int totalPasswordsCount = 5;
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        mockPasswordCounts(
                /* compromised= */ 0,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_passwords_title);
        String expectedSummary =
                mActivity.getString(
                        R.string.safety_hub_password_check_time_recently, TEST_EMAIL_ADDRESS);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void noCompromisedPasswords_weakAndReusedPasswordsExists_managed_disabled() {
        int totalPasswordsCount = 5;
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        mockPasswordCounts(
                /* compromised= */ 0,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_passwords_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedPasswordsExist() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = 5;
        int weakPasswordsCount = 6;
        int reusedPasswordsCount = 6;
        mockPasswordCounts(
                /* compromised= */ compromisedPasswordsCount,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_check_passwords_compromised_exist,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(WARNING_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedPasswordsExist_managed() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = 5;
        int weakPasswordsCount = 6;
        int reusedPasswordsCount = 6;
        mockPasswordCounts(
                /* compromised= */ compromisedPasswordsCount,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_check_passwords_compromised_exist,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void noPasswordsSaved() {
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(true);
        mockManaged(false);
        mockTotalPasswordsCount(0);

        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_passwords_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_no_passwords_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void noPasswordsSaved_managed() {
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(true);
        mockManaged(true);
        mockTotalPasswordsCount(0);

        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_passwords_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedCountUnavailable_noWeakAndReusedPasswords_disabled() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(
                /* compromised= */ compromisedPasswordsCount,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_password_check_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());

        // Verify the signed out state.
        mockSignedInState(false);
        mModuleMediator.onSignedOut();

        String expectedSignedOutSummary =
                mActivity.getString(R.string.safety_hub_password_check_signed_out_summary);
        expectedSecondaryButtonText = mActivity.getString(R.string.sign_in_to_chrome);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSignedOutSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedCountUnavailable_noWeakAndReusedPasswords_managed_disabled() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(
                /* compromised= */ compromisedPasswordsCount,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_password_check_unavailable_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedCountUnavailable_noWeakAndReusedPasswords_enabled() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(
                /* compromised= */ compromisedPasswordsCount,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_reused_weak_passwords_title);
        String expectedSummary =
                mActivity.getString(
                        R.string
                                .safety_hub_unavailable_compromised_no_reused_weak_passwords_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
        assertNull(mPreference.getPrimaryButtonText());
    }

    @Test
    @Features.EnableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedCountUnavailable_noWeakAndReusedPasswords_managed_enabled() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(
                /* compromised= */ compromisedPasswordsCount,
                /* weak= */ weakPasswordsCount,
                /* reused= */ reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);
        mockTotalPasswordsCount(totalPasswordsCount);

        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_reused_weak_passwords_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }
}
