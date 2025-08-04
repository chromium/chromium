// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.doReturn;
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
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridge;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingBridgeJni;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubSafeBrowsingModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubSafeBrowsingModuleMediatorTest {
    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int MANAGED_ICON = R.drawable.ic_business;
    private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private SafetyHubExpandablePreference mPreference;
    private SafetyHubSafeBrowsingModuleMediator mModuleMediator;

    @Mock private SafeBrowsingBridge.Natives mSafeBrowsingBridgeNativeMock;
    @Mock private Profile mProfile;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        SafeBrowsingBridgeJni.setInstanceForTesting(mSafeBrowsingBridgeNativeMock);

        mModuleMediator = new SafetyHubSafeBrowsingModuleMediator(mPreference, null, mProfile);
        mModuleMediator.setUpModule();
    }

    private void mockSafeBrowsingState(@SafeBrowsingState int safeBrowsingState) {
        doReturn(safeBrowsingState)
                .when(mSafeBrowsingBridgeNativeMock)
                .getSafeBrowsingState(mProfile);
    }

    private void mockSafeBrowsingManaged(boolean managed) {
        doReturn(managed).when(mSafeBrowsingBridgeNativeMock).isSafeBrowsingManaged(mProfile);
    }

    @Test
    public void standardSafeBrowsing() {
        mockSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        mockSafeBrowsingManaged(false);
        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_safe_browsing_on_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_safe_browsing_on_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_security_settings_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void standardSafeBrowsing_managed() {
        mockSafeBrowsingState(SafeBrowsingState.STANDARD_PROTECTION);
        mockSafeBrowsingManaged(true);
        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_safe_browsing_on_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_safe_browsing_on_summary_managed);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    public void enhancedSafeBrowsing() {
        mockSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        mockSafeBrowsingManaged(false);
        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_safe_browsing_enhanced_title);
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_safe_browsing_enhanced_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_security_settings_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void enhancedSafeBrowsing_managed() {
        mockSafeBrowsingState(SafeBrowsingState.ENHANCED_PROTECTION);
        mockSafeBrowsingManaged(true);
        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_safe_browsing_enhanced_title);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_safe_browsing_enhanced_summary_managed);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    public void safeBrowsingOff() {
        mockSafeBrowsingState(SafeBrowsingState.NO_SAFE_BROWSING);
        mockSafeBrowsingManaged(false);
        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.prefs_safe_browsing_no_protection_summary);
        String expectedSummary = mActivity.getString(R.string.safety_hub_safe_browsing_off_summary);
        String expectedPrimaryButtonText = mActivity.getString(R.string.safety_hub_turn_on_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());

        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(WARNING_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    public void safeBrowsingOff_managed() {
        mockSafeBrowsingState(SafeBrowsingState.NO_SAFE_BROWSING);
        mockSafeBrowsingManaged(true);
        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity.getString(R.string.prefs_safe_browsing_no_protection_summary);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_safe_browsing_off_summary_managed);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPreference.getSummary().toString());
        assertEquals(MANAGED_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }
}
