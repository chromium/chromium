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
import org.chromium.components.content_settings.ContentSettingsType;
import org.chromium.ui.base.TestActivity;

/** Robolectric tests for {@link SafetyHubPermissionsRevocationModuleMediator}. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubPermissionsRevocationModuleMediatorTest {
    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;

    private static final String EXAMPLE_URL = "http://example1.com";
    private static final PermissionsData PERMISSIONS_DATA =
            PermissionsData.create(
                    EXAMPLE_URL,
                    new int[] {
                        ContentSettingsType.MEDIASTREAM_CAMERA,
                    },
                    0,
                    0,
                    PermissionsRevocationType.UNUSED_PERMISSIONS);

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    private Activity mActivity;
    private SafetyHubExpandablePreference mPreference;
    private SafetyHubPermissionsRevocationModuleMediator mModuleMediator;

    @Mock private UnusedSitePermissionsBridge mUnusedPermissionsBridgeMock;
    @Mock private SafetyHubModuleMediatorDelegate mDelegate;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        mPreference = new SafetyHubExpandablePreference(mActivity, null);

        mModuleMediator =
                new SafetyHubPermissionsRevocationModuleMediator(
                        mPreference, mDelegate, mUnusedPermissionsBridgeMock);
        mockUnusedSitePermissions(/* hasUnusedSitePermissions= */ false);
        mModuleMediator.setUpModule();
    }

    private void mockUnusedSitePermissions(boolean hasUnusedSitePermissions) {
        if (hasUnusedSitePermissions) {
            doReturn(new PermissionsData[] {PERMISSIONS_DATA})
                    .when(mUnusedPermissionsBridgeMock)
                    .getRevokedPermissions();
        } else {
            doReturn(new PermissionsData[] {})
                    .when(mUnusedPermissionsBridgeMock)
                    .getRevokedPermissions();
        }
    }

    @Test
    public void noSitesWithUnusedPermissions() {
        mockUnusedSitePermissions(/* hasUnusedSitePermissions= */ false);
        mModuleMediator.updateModule();

        String expectedTitle = mActivity.getString(R.string.safety_hub_permissions_ok_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_permissions_ok_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_site_settings_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertNull(mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void hasSiteWithUnusedPermissions_onInitialSetup() {
        mockUnusedSitePermissions(/* hasUnusedSitePermissions= */ true);
        mModuleMediator.updateModule();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_permissions_warning_title,
                                /* numSites= */ 1,
                                /* numSites= */ 1);
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_permissions_warning_summary);
        String expectedPrimaryButtonText = mActivity.getString(R.string.got_it);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_view_sites_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());
    }

    @Test
    public void hasSiteWithUnusedPermissions_permissionsChanged() {
        mockUnusedSitePermissions(/* hasUnusedSitePermissions= */ true);
        mModuleMediator.revokedPermissionsChanged();

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_permissions_warning_title,
                                /* numSites= */ 1,
                                /* numSites= */ 1);
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_permissions_warning_summary);
        String expectedPrimaryButtonText = mActivity.getString(R.string.got_it);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_view_sites_button);

        assertEquals(expectedTitle, mPreference.getTitle().toString());
        assertEquals(expectedSummary, mPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPreference.getSecondaryButtonText());

        verify(mDelegate, times(1)).onUpdateNeeded();
    }
}
