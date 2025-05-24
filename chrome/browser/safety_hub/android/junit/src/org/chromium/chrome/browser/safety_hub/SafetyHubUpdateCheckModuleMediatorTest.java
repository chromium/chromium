// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
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
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubUpdateCheckModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubUpdateCheckModuleMediatorTest {
    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;
    private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private SafetyHubExpandablePreference mPreference;
    private SafetyHubUpdateCheckModuleMediator mModuleMediator;

    @Mock private SafetyHubFetchService mSafetyHubFetchService;
    @Mock private SafetyHubModuleMediatorDelegate mDelegate;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        mModuleMediator =
                new SafetyHubUpdateCheckModuleMediator(
                        mPreference, mDelegate, null, mSafetyHubFetchService);
        mModuleMediator.setUpModule();
    }

    private void mockUpdateStatus(UpdateStatusProvider.UpdateStatus updateStatus) {
        doReturn(updateStatus).when(mSafetyHubFetchService).getUpdateStatus();
    }

    @Test
    public void statusUpToDate() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;
        mockUpdateStatus(updateStatus);
        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_check_updates_updated);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_google_play_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void statusUpdateAvailable() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE;
        mockUpdateStatus(updateStatus);
        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_check_updates_outdated);
        String expectedPrimaryButtonText = mActivity.getString(R.string.menu_update);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(WARNING_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertNull(mPreference.getSecondaryButtonText());
    }

    @Test
    public void statusUnsupportedOsVersion() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION;
        updateStatus.latestUnsupportedVersion = "1.1.1.1";
        mockUpdateStatus(updateStatus);
        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.menu_update_unsupported);
        String expectedSummary =
                mActivity.getString(R.string.menu_update_unsupported_summary_default);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void statusNotReady() {
        mockUpdateStatus(null);
        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_update_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_google_play_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void statusNotReady_changed() {
        mockUpdateStatus(null);
        mModuleMediator.updateStatusChanged();

        String expectedTitle = mActivity.getString(R.string.safety_hub_update_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_google_play_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());

        verify(mDelegate, times(1)).onUpdateNeeded();
    }
}
