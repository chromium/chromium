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
import org.chromium.chrome.browser.safety_hub.SafetyHubLocalPasswordsDataSource.ModuleType;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubLocalPasswordsModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Features.EnableFeatures({
    ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS,
    ChromeFeatureList.SAFETY_HUB_LOCAL_PASSWORDS_MODULE
})
public class SafetyHubLocalPasswordsModuleMediatorTest {
    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;
    private static final @DrawableRes int MANAGED_ICON = R.drawable.ic_business;
    private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;

    private static final String TEST_EMAIL_ADDRESS = "test@email.com";

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private SafetyHubExpandablePreference mPreference;
    private SafetyHubLocalPasswordsModuleMediator mModuleMediator;

    @Mock private SafetyHubLocalPasswordsDataSource mDataSource;
    @Mock private SafetyHubModuleMediatorDelegate mMediatorDelegateMock;
    @Mock private SafetyHubModuleDelegate mModuleDelegateMock;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        mModuleMediator =
                new SafetyHubLocalPasswordsModuleMediator(
                        mPreference, mDataSource, mMediatorDelegateMock, mModuleDelegateMock);
        mModuleMediator.setUpModule();
        clearInvocations(mMediatorDelegateMock);
    }

    // TODO(crbug.com/388788969): Mock reused passwords.
    private void mockPasswordCounts(int compromised, int weak, int reused) {
        doReturn(compromised).when(mDataSource).getCompromisedPasswordCount();
        doReturn(weak).when(mDataSource).getWeakPasswordCount();
        doReturn(reused).when(mDataSource).getReusedPasswordCount();
    }

    private void mockManaged(boolean isManaged) {
        doReturn(isManaged).when(mDataSource).isManaged();
    }

    private void mockTriggerCheckup(boolean willTriggerCheckup) {
        doReturn(willTriggerCheckup).when(mDataSource).maybeTriggerPasswordCheckup();
    }

    @Test
    public void countsUnavailable() {
        mockPasswordCounts(/* compromised= */ -1, /* weak= */ -1, /* reused= */ -1);
        mockManaged(false);

        mModuleMediator.localPasswordsStateChanged(ModuleType.UNAVAILABLE_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_local_password_check_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
        assertNull(mPreference.getPrimaryButtonText());
    }

    @Test
    public void countsUnavailable_managed() {
        mockPasswordCounts(/* compromised= */ -1, /* weak= */ -1, /* reused= */ -1);
        mockManaged(true);

        mModuleMediator.localPasswordsStateChanged(ModuleType.UNAVAILABLE_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_local_password_check_unavailable_title);
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
    public void noPasswords() {
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 0, /* reused= */ 0);
        mockManaged(false);

        mModuleMediator.localPasswordsStateChanged(ModuleType.NO_SAVED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_local_passwords_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_no_passwords_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
        assertNull(mPreference.getPrimaryButtonText());
    }

    @Test
    public void noPasswords_managed() {
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 0, /* reused= */ 0);
        mockManaged(true);

        mModuleMediator.localPasswordsStateChanged(ModuleType.NO_SAVED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_local_passwords_title);
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
    public void hasCompromisedPasswords() {
        int compromisedPasswordsCount = 1;
        mockPasswordCounts(compromisedPasswordsCount, /* weak= */ 1, /* reused= */ 2);
        mockManaged(false);

        mModuleMediator.localPasswordsStateChanged(ModuleType.HAS_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_local_passwords_compromised_title,
                                compromisedPasswordsCount);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_compromised_passwords_summary,
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
    public void hasCompromisedPasswords_managed() {
        int compromisedPasswordsCount = 1;
        mockPasswordCounts(compromisedPasswordsCount, /* weak= */ 1, /* reused= */ 2);
        mockManaged(true);

        mModuleMediator.localPasswordsStateChanged(ModuleType.HAS_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_local_passwords_compromised_title,
                                compromisedPasswordsCount);
        String expectedManagedSummary =
                mActivity
                        .getResources()
                        .getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void hasWeakAndReusedPasswords() {
        int reusedPasswordsCount = 2;
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 1, /* reused= */ reusedPasswordsCount);
        mockManaged(false);

        mModuleMediator.localPasswordsStateChanged(ModuleType.HAS_REUSED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_local_passwords_title);
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
    public void hasWeakAndReusedPasswords_managed() {
        mockPasswordCounts(/* compromised= */ 0, /* weak= */ 1, /* reused= */ 2);
        mockManaged(true);

        mModuleMediator.localPasswordsStateChanged(ModuleType.HAS_REUSED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        String expectedManagedSummary =
                mActivity
                        .getResources()
                        .getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void hasWeakPasswords() {
        int weakPasswordsCount = 1;
        mockPasswordCounts(/* compromised= */ 0, weakPasswordsCount, /* reused= */ 0);
        mockManaged(false);

        mModuleMediator.localPasswordsStateChanged(ModuleType.HAS_WEAK_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary, weakPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    public void hasWeakPasswords_managed() {
        mockPasswordCounts(/* compromised= */ 0, 1, /* reused= */ 0);
        mockManaged(true);

        mModuleMediator.localPasswordsStateChanged(ModuleType.HAS_WEAK_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_reused_weak_local_passwords_title);
        String expectedManagedSummary =
                mActivity
                        .getResources()
                        .getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void noCompromisedPasswords() {
        mockPasswordCounts(0, 0, 0);
        mockManaged(false);

        mModuleMediator.localPasswordsStateChanged(ModuleType.NO_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_local_passwords_title);
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_no_compromised_passwords_summary);
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
        mockManaged(true);

        mModuleMediator.localPasswordsStateChanged(ModuleType.NO_COMPROMISED_PASSWORDS);
        verify(mMediatorDelegateMock, times(1)).onUpdateNeeded();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_local_passwords_title);
        String expectedManagedSummary =
                mActivity
                        .getResources()
                        .getString(R.string.safety_hub_no_passwords_summary_managed);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void loadingState() {
        mockTriggerCheckup(true);

        SafetyHubLocalPasswordsModuleMediator moduleMediator =
                new SafetyHubLocalPasswordsModuleMediator(
                        mPreference, mDataSource, mMediatorDelegateMock, mModuleDelegateMock);

        moduleMediator.setUpModule();
        moduleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_local_passwords_checking_title);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertNull(mPreference.getSummary());
        assertNull(mPreference.getIcon());
        assertNull(mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }
}
