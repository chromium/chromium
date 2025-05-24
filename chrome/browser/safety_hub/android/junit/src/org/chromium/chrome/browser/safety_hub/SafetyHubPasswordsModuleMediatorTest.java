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
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubPasswordsModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Features.EnableFeatures({
    ChromeFeatureList.SAFETY_HUB,
    ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
    ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE,
    ChromeFeatureList.SAFETY_HUB_UNIFIED_PASSWORDS_MODULE
})
public class SafetyHubPasswordsModuleMediatorTest {
    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;
    private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private SafetyHubExpandablePreference mPreference;
    private SafetyHubPasswordsModuleMediator mModuleMediator;

    @Mock private SafetyHubAccountPasswordsDataSource mAccountDataSource;
    @Mock private SafetyHubLocalPasswordsDataSource mLocalDataSource;
    @Mock private SafetyHubModuleMediatorDelegate mMediatorDelegateMock;
    @Mock private SafetyHubModuleDelegate mModuleDelegateMock;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        mockAccountPasswordCounts(0, 0, 0);
        mockLocalPasswordCounts(0, 0, 0);

        mModuleMediator =
                new SafetyHubPasswordsModuleMediator(
                        mPreference,
                        mAccountDataSource,
                        mLocalDataSource,
                        mMediatorDelegateMock,
                        mModuleDelegateMock);
        mModuleMediator.setUpModule();
        clearInvocations(mMediatorDelegateMock);
    }

    private void mockAccountPasswordCounts(int compromised, int reused, int weak) {
        doReturn(compromised).when(mAccountDataSource).getCompromisedPasswordCount();
        doReturn(reused).when(mAccountDataSource).getReusedPasswordCount();
        doReturn(weak).when(mAccountDataSource).getWeakPasswordCount();
    }

    private void mockLocalPasswordCounts(int compromised, int reused, int weak) {
        doReturn(compromised).when(mLocalDataSource).getCompromisedPasswordCount();
        doReturn(reused).when(mLocalDataSource).getReusedPasswordCount();
        doReturn(weak).when(mLocalDataSource).getWeakPasswordCount();
    }

    private void mockAccountPasswordState(
            @SafetyHubAccountPasswordsDataSource.ModuleType int accountModuleType) {
        doReturn(accountModuleType).when(mAccountDataSource).getModuleType();
    }

    private void mockLocalPasswordState(
            @SafetyHubLocalPasswordsDataSource.ModuleType int localModuleType) {
        doReturn(localModuleType).when(mLocalDataSource).getModuleType();
    }

    @Test
    public void compromisedAccountAndLocalPasswordsExist() {
        int compromisedAccountPasswordsCount = 5;
        int compromisedLocalPasswordsCount = 4;
        mockAccountPasswordCounts(compromisedAccountPasswordsCount, /* reused= */ 2, /* weak= */ 1);
        mockLocalPasswordCounts(compromisedLocalPasswordsCount, /* reused= */ 2, /* weak= */ 1);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mockLocalPasswordState(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        int totalCompromisedPasswordsCount =
                compromisedAccountPasswordsCount + compromisedLocalPasswordsCount;
        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_passwords_compromised_title,
                                totalCompromisedPasswordsCount,
                                totalCompromisedPasswordsCount);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                totalCompromisedPasswordsCount,
                                totalCompromisedPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(WARNING_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    public void compromisedAccountPasswordsExist_weakAndReuseExist() {
        int compromisedAccountPasswordsCount = 5;
        mockAccountPasswordCounts(compromisedAccountPasswordsCount, /* reused= */ 2, /* weak= */ 1);
        mockLocalPasswordCounts(/* compromised= */ 0, /* reused= */ 2, /* weak= */ 1);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_account_passwords_compromised_exist,
                                compromisedAccountPasswordsCount,
                                compromisedAccountPasswordsCount);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                compromisedAccountPasswordsCount,
                                compromisedAccountPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(WARNING_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void compromisedAccountPasswordsExist_unavailableLocalPasswords() {
        int compromisedAccountPasswordsCount = 5;
        mockAccountPasswordCounts(compromisedAccountPasswordsCount, /* reused= */ 2, /* weak= */ 1);
        mockLocalPasswordCounts(/* compromised= */ -1, /* reused= */ -1, /* weak= */ -1);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_account_passwords_compromised_exist,
                                compromisedAccountPasswordsCount,
                                compromisedAccountPasswordsCount);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                compromisedAccountPasswordsCount,
                                compromisedAccountPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(WARNING_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void compromisedLocalPasswordsExist_weakAndReuseExist() {
        int compromisedLocalPasswordsCount = 5;
        mockAccountPasswordCounts(/* compromised= */ 0, /* reused= */ 2, /* weak= */ 1);
        mockLocalPasswordCounts(compromisedLocalPasswordsCount, /* reused= */ 2, /* weak= */ 1);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mockLocalPasswordState(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_local_passwords_compromised_title,
                                compromisedLocalPasswordsCount,
                                compromisedLocalPasswordsCount);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                compromisedLocalPasswordsCount,
                                compromisedLocalPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(WARNING_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void compromisedLocalPasswordsExist_unavailableAccountPasswords() {
        int compromisedLocalPasswordsCount = 5;
        mockAccountPasswordCounts(/* compromised= */ -1, /* reused= */ -1, /* weak= */ -1);
        mockLocalPasswordCounts(compromisedLocalPasswordsCount, /* reused= */ 2, /* weak= */ 1);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mockLocalPasswordState(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_COMPROMISED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_local_passwords_compromised_title,
                                compromisedLocalPasswordsCount,
                                compromisedLocalPasswordsCount);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
                                compromisedLocalPasswordsCount,
                                compromisedLocalPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(WARNING_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void noCompromisedPasswordsExist() {
        mockAccountPasswordCounts(0, 0, 0);
        mockLocalPasswordCounts(0, 0, 0);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mockLocalPasswordState(
                SafetyHubLocalPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_passwords_title);
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_no_compromised_passwords_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void reusedAccountAndLocalPasswordsExist() {
        int reusedAccountPasswordsCount = 5;
        int reusedLocalPasswordsCount = 4;
        mockAccountPasswordCounts(/* compromised= */ 0, reusedAccountPasswordsCount, /* weak= */ 1);
        mockLocalPasswordCounts(/* compromised= */ 0, reusedLocalPasswordsCount, /* weak= */ 1);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        int totalReusedPasswordsCount = reusedAccountPasswordsCount + reusedLocalPasswordsCount;
        String expectedTitle = mActivity.getString(R.string.safety_hub_reused_weak_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_reused_passwords_summary,
                                totalReusedPasswordsCount,
                                totalReusedPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    public void reusedAccountPasswordsExist() {
        int reusedAccountPasswordsCount = 5;
        mockAccountPasswordCounts(/* compromised= */ 0, reusedAccountPasswordsCount, /* weak= */ 1);
        mockLocalPasswordCounts(/* compromised= */ 0, /* reused= */ 0, /* weak= */ 1);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mockLocalPasswordState(
                SafetyHubLocalPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_account_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_reused_passwords_summary,
                                reusedAccountPasswordsCount,
                                reusedAccountPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void reusedLocalPasswordsExist() {
        int reusedLocalPasswordsCount = 5;
        mockAccountPasswordCounts(/* compromised= */ 0, /* reused= */ 0, /* weak= */ 1);
        mockLocalPasswordCounts(/* compromised= */ 0, reusedLocalPasswordsCount, /* weak= */ 1);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_reused_passwords_summary,
                                reusedLocalPasswordsCount,
                                reusedLocalPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void weakAccountAndLocalPasswordsExist() {
        int weakAccountPasswordsCount = 5;
        int weakLocalPasswordsCount = 4;
        mockAccountPasswordCounts(/* compromised= */ 0, /* reused= */ 0, weakAccountPasswordsCount);
        mockLocalPasswordCounts(/* compromised= */ 0, /* reused= */ 0, weakLocalPasswordsCount);

        mockAccountPasswordState(SafetyHubAccountPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        int totalWeakPasswordsCount = weakAccountPasswordsCount + weakLocalPasswordsCount;
        String expectedTitle = mActivity.getString(R.string.safety_hub_reused_weak_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                totalWeakPasswordsCount,
                                totalWeakPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    public void weakAccountPasswordsExist() {
        int weakAccountPasswordsCount = 5;
        mockAccountPasswordCounts(/* compromised= */ 0, /* reused= */ 0, weakAccountPasswordsCount);
        mockLocalPasswordCounts(/* compromised= */ 0, /* reused= */ 0, /* weak= */ 0);

        mockAccountPasswordState(SafetyHubAccountPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS);
        mockLocalPasswordState(
                SafetyHubLocalPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_account_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                weakAccountPasswordsCount,
                                weakAccountPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void weakLocalPasswordsExist() {
        int weakLocalPasswordsCount = 5;
        mockAccountPasswordCounts(/* compromised= */ 0, /* reused= */ 0, /* weak= */ 0);
        mockLocalPasswordCounts(/* compromised= */ 0, /* reused= */ 0, weakLocalPasswordsCount);

        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.NO_COMPROMISED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_WEAK_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                weakLocalPasswordsCount,
                                weakLocalPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void noAccountOrLocalPasswordsExist() {
        mockAccountPasswordState(SafetyHubAccountPasswordsDataSource.ModuleType.NO_SAVED_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.NO_SAVED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.NO_SAVED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.NO_SAVED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_passwords_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_no_passwords_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void signOutAndNoLocalPasswordsExist() {
        mockAccountPasswordState(SafetyHubAccountPasswordsDataSource.ModuleType.SIGNED_OUT);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.NO_SAVED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.SIGNED_OUT);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.NO_SAVED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_passwords_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_no_passwords_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void unavailableLocalPasswords_reusedAccountPasswordsExists() {
        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_password_check_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void unavailableAccountPasswords_reusedLocalPasswordsExists() {
        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.HAS_REUSED_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_password_check_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void unavailableAccountAndLocalPasswords() {
        mockAccountPasswordState(
                SafetyHubAccountPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mockLocalPasswordState(SafetyHubLocalPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mModuleMediator.accountPasswordsStateChanged(
                SafetyHubAccountPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);
        mModuleMediator.localPasswordsStateChanged(
                SafetyHubLocalPasswordsDataSource.ModuleType.UNAVAILABLE_PASSWORDS);

        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_password_check_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_password_subpage_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }
}
