// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.clearInvocations;
import static org.mockito.Mockito.doReturn;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
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
import org.chromium.chrome.browser.safety_hub.SafetyHubAccountPasswordsDataSource.ModuleType;
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

    @Mock private SafetyHubAccountPasswordsDataSource mDataSource;
    @Mock private SafetyHubModuleMediatorDelegate mMediatorDelegateMock;
    @Mock private SafetyHubModuleDelegate mModuleDelegateMock;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        mockPasswordCounts(0, 0, 0);
        mockSignedInState(false);

        mModuleMediator =
                new SafetyHubAccountPasswordsModuleMediator(
                        mPreference, mDataSource, mMediatorDelegateMock, mModuleDelegateMock);
        mModuleMediator.setUpModule();
        clearInvocations(mMediatorDelegateMock);
    }

    public void mockSignedInState(boolean isSignedIn) {
        doReturn(isSignedIn ? TEST_EMAIL_ADDRESS : null).when(mDataSource).getAccountEmail();
    }

    private void mockPasswordCounts(int compromised, int weak, int reused) {
        doReturn(compromised).when(mDataSource).getCompromisedPasswordCount();
        doReturn(weak).when(mDataSource).getWeakPasswordCount();
        doReturn(reused).when(mDataSource).getReusedPasswordCount();
    }

    private void mockManaged(boolean isManaged) {
        doReturn(isManaged).when(mDataSource).isManaged();
    }

    @Test
    public void noCompromisedPasswords() {
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(true);
        mockManaged(false);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.NO_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_account_passwords_title);
        String expectedSummary =
                TEST_EMAIL_ADDRESS
                        + "\n"
                        + mActivity.getString(R.string.safety_hub_no_compromised_passwords_summary);
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
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(true);
        mockManaged(true);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.NO_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_account_passwords_title);
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
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.HAS_REUSED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_account_passwords_title);
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
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.HAS_REUSED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_account_passwords_title);
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
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, /* reused= */ 0);
        mockSignedInState(true);
        mockManaged(false);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.HAS_WEAK_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_account_passwords_title);
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
    public void noCompromisedPasswords_weakPasswordsExists_managed_enabled() {
        int weakPasswordsCount = 1;
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, /* reused= */ 0);
        mockSignedInState(true);
        mockManaged(true);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.HAS_WEAK_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_account_passwords_title);
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
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.NO_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_account_passwords_title);
        String expectedSummary =
                TEST_EMAIL_ADDRESS
                        + "\n"
                        + mActivity.getString(R.string.safety_hub_no_compromised_passwords_summary);
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
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.NO_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_account_passwords_title);
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
        int compromisedPasswordsCount = 5;
        int weakPasswordsCount = 6;
        int reusedPasswordsCount = 6;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.HAS_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_account_passwords_compromised_exist,
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
        int compromisedPasswordsCount = 5;
        int weakPasswordsCount = 6;
        int reusedPasswordsCount = 6;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.HAS_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_account_passwords_compromised_exist,
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

        mModuleMediator.accountPasswordsStateChanged(ModuleType.NO_SAVED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_account_passwords_title);
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

        mModuleMediator.accountPasswordsStateChanged(ModuleType.NO_SAVED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_account_passwords_title);
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
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.UNAVAILABLE_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_account_password_check_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    @Features.DisableFeatures({ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS})
    public void compromisedCountUnavailable_noWeakAndReusedPasswords_managed_disabled() {
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.UNAVAILABLE_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_account_password_check_unavailable_title);
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
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(false);

        mModuleMediator.accountPasswordsStateChanged(
                ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

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
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mockPasswordCounts(compromisedPasswordsCount, weakPasswordsCount, reusedPasswordsCount);
        mockSignedInState(true);
        mockManaged(true);

        mModuleMediator.accountPasswordsStateChanged(
                ModuleType.UNAVAILABLE_COMPROMISED_NO_WEAK_REUSED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

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

    @Test
    public void signedOut() {
        mockPasswordCounts(0, 0, 0);
        mockSignedInState(false);
        mockManaged(false);

        mModuleMediator.accountPasswordsStateChanged(ModuleType.SIGNED_OUT);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_account_password_check_unavailable_title);
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_password_check_signed_out_summary);
        String expectedSecondaryButtonText = mActivity.getString(R.string.sign_in_to_chrome);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }
}
