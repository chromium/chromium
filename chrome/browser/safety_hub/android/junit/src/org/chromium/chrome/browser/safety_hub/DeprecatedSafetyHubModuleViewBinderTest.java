// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;

import androidx.annotation.DrawableRes;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.chrome.browser.safe_browsing.SafeBrowsingState;
import org.chromium.components.browser_ui.settings.CardPreference;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class DeprecatedSafetyHubModuleViewBinderTest {
    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;
    private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;
    private static final @DrawableRes int MANAGED_ICON = R.drawable.ic_business;
    private static final String TEST_ACCOUNT_EMAIL = "test@gmail.com";
    private Activity mActivity;
    private PropertyModel mBrowserStatePropertyModel;
    private CardPreference mBrowserStatePreference;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        // Set up browser state preference.
        mBrowserStatePreference = new CardPreference(mActivity, null);
        mBrowserStatePropertyModel =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties.BROWSER_STATE_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mBrowserStatePropertyModel,
                mBrowserStatePreference,
                DeprecatedSafetyHubModuleViewBinder::bindBrowserStateProperties);
    }

    @Test
    public void testBrowserStateModule_OneUnSafeState() {
        @SafeBrowsingState int safeBrowsingState = SafeBrowsingState.ENHANCED_PROTECTION;
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE;
        int totalPasswordsCount = 0;
        int compromisedPasswordsCount = 0;
        int sitesWithUnusedPermissionsCount = 3;
        int notificationPermissionsForReviewCount = 5;

        mBrowserStatePropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;
        updateStatus.latestVersion = "1.1.1.1";
        safeBrowsingState = SafeBrowsingState.NO_SAFE_BROWSING;

        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        safeBrowsingState = SafeBrowsingState.STANDARD_PROTECTION;
        totalPasswordsCount = 5;
        compromisedPasswordsCount = 1;

        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

        assertFalse(mBrowserStatePreference.isVisible());
    }

    @Test
    public void testBrowserStateModule_MultipleUnSafeState() {
        @SafeBrowsingState int safeBrowsingState = SafeBrowsingState.NO_SAFE_BROWSING;
        UpdateStatusProvider.UpdateStatus updateStatus = null;
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = 5;
        int sitesWithUnusedPermissionsCount = 3;
        int notificationPermissionsForReviewCount = 5;

        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        assertFalse(mBrowserStatePreference.isVisible());
    }

    @Test
    public void testBrowserStateModule_SafeState() {
        @SafeBrowsingState int safeBrowsingState = SafeBrowsingState.STANDARD_PROTECTION;
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;
        updateStatus.latestVersion = "1.1.1.1";
        int totalPasswordsCount = 0;
        int compromisedPasswordsCount = 0;
        int sitesWithUnusedPermissionsCount = 3;
        int notificationPermissionsForReviewCount = 0;

        mBrowserStatePropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        mBrowserStatePropertyModel.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        assertTrue(mBrowserStatePreference.isVisible());
    }
}
