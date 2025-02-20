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

    private void mockManaged(boolean isManaged) {
        doReturn(isManaged).when(mDataSource).isManaged();
    }

    @Test
    public void countsUnavailable() {
        // TODO(crbug.com/388788969): After adding logic to the local password module, set
        // appropriate counts for the unavailable state.
        mockManaged(false);

        mModuleMediator.stateChanged(ModuleType.UNAVAILABLE_PASSWORDS);
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
        // TODO(crbug.com/388788969): After adding logic to the local password module, set
        // appropriate counts for the unavailable state.
        mockManaged(true);

        mModuleMediator.stateChanged(ModuleType.UNAVAILABLE_PASSWORDS);
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
        mockManaged(false);

        mModuleMediator.stateChanged(ModuleType.NO_SAVED_PASSWORDS);
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
        mockManaged(true);

        mModuleMediator.stateChanged(ModuleType.NO_SAVED_PASSWORDS);
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
}
