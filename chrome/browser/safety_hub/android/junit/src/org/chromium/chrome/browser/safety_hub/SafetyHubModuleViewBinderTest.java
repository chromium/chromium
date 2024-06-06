// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNull;
import static org.robolectric.Shadows.shadowOf;

import android.app.Activity;

import androidx.annotation.DrawableRes;
import androidx.preference.Preference;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider;
import org.chromium.ui.base.TestActivity;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubModuleViewBinderTest {
    private static final @DrawableRes int OK_ICON = R.drawable.ic_checkmark_24dp;
    private static final @DrawableRes int ERROR_ICON = R.drawable.ic_error;
    private static final @DrawableRes int WARNING_ICON = R.drawable.btn_info;
    private Activity mActivity;
    private PropertyModel mPasswordCheckPropertyModel;
    private Preference mPasswordCheckPreference;
    private PropertyModel mUpdateCheckPropertyModel;
    private Preference mUpdateCheckPreference;
    private PropertyModel mPermissionsPropertyModel;
    private Preference mPermissionsPreference;
    private PropertyModel mNotificationsReviewPropertyModel;
    private Preference mNotificationsReviewPreference;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        // Set up password check preference.
        mPasswordCheckPreference = new Preference(mActivity);

        mPasswordCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mPasswordCheckPropertyModel,
                mPasswordCheckPreference,
                SafetyHubModuleViewBinder::bindPasswordCheckProperties);

        // Set up update check preference.
        mUpdateCheckPreference = new Preference(mActivity);

        mUpdateCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mUpdateCheckPropertyModel,
                mUpdateCheckPreference,
                SafetyHubModuleViewBinder::bindUpdateCheckProperties);

        // Set up permissions preference.
        mPermissionsPreference = new Preference(mActivity);
        mPermissionsPropertyModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.PERMISSIONS_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mPermissionsPropertyModel,
                mPermissionsPreference,
                SafetyHubModuleViewBinder::bindPermissionsProperties);

        // Set up notifications review preference.
        mNotificationsReviewPreference = new Preference(mActivity);
        mNotificationsReviewPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.NOTIFICATIONS_REVIEW_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mNotificationsReviewPropertyModel,
                mNotificationsReviewPreference,
                SafetyHubModuleViewBinder::bindNotificationsReviewProperties);
    }

    @Test
    public void testPasswordCheckModule_NoCompromisedPasswords() {
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, 0);

        String expectedTitle = mActivity.getString(R.string.safety_check_passwords_safe);
        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(OK_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testPasswordCheckModule_CompromisedPasswordsExist() {
        int compromisedPasswordsCount = 5;
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);

        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_check_passwords_compromised_exist,
                                compromisedPasswordsCount,
                                compromisedPasswordsCount);
        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(
                ERROR_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testUpdateCheckModule_UpToDate() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;
        updateStatus.latestVersion = "1.1.1.1";

        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        String expectedTitle = mActivity.getString(R.string.safety_check_updates_updated);
        String expectedSummary = updateStatus.latestVersion;

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mUpdateCheckPreference.getSummary().toString());
        assertEquals(OK_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testUpdateCheckModule_UpdateAvailable() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE;

        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        String expectedTitle = mActivity.getString(R.string.safety_check_updates_outdated);

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertEquals(ERROR_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testUpdateCheckModule_UnsupportedOsVersion() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION;
        updateStatus.latestUnsupportedVersion = "1.1.1.1";

        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        String expectedTitle =
                mActivity.getString(R.string.menu_update_unsupported_summary_default);
        String expectedSummary = updateStatus.latestUnsupportedVersion;

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mUpdateCheckPreference.getSummary().toString());
        assertEquals(ERROR_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testUpdateCheckModule_StatusNotReady() {
        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, null);

        String expectedTitle = mActivity.getString(R.string.safety_check_updates_updated);

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertNull(mUpdateCheckPreference.getSummary());
        assertEquals(OK_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testPermissionsModule_NoSitesWithUnusedPermissions() {
        mPermissionsPropertyModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT, 0);

        String expectedTitle = mActivity.getString(R.string.safety_hub_permissions_ok_title);

        assertEquals(expectedTitle, mPermissionsPreference.getTitle().toString());
        assertNull(mPermissionsPreference.getSummary());
        assertEquals(OK_ICON, shadowOf(mPermissionsPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testPermissionsModule_WithSitesWithUnusedPermissions() {
        int sitesWithUnusedPermissionsCount = 3;
        mPermissionsPropertyModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_permissions_warning_title,
                                sitesWithUnusedPermissionsCount,
                                sitesWithUnusedPermissionsCount);

        assertEquals(expectedTitle, mPermissionsPreference.getTitle().toString());
        assertNull(mPermissionsPreference.getSummary());
        assertEquals(
                WARNING_ICON, shadowOf(mPermissionsPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testNotificationsReviewModule_NoNotificationPermissions() {
        mNotificationsReviewPropertyModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT, 0);

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_notifications_review_ok_title);

        assertEquals(expectedTitle, mNotificationsReviewPreference.getTitle().toString());
        assertNull(mNotificationsReviewPreference.getSummary());
        assertEquals(
                OK_ICON, shadowOf(mNotificationsReviewPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testNotificationsReviewModule_NotificationPermissionsExist() {
        int notificationPermissionsForReviewCount = 5;
        mNotificationsReviewPropertyModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);
        String expectedTitle =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_notifications_review_warning_title,
                                notificationPermissionsForReviewCount,
                                notificationPermissionsForReviewCount);

        assertEquals(expectedTitle, mNotificationsReviewPreference.getTitle().toString());
        assertNull(mNotificationsReviewPreference.getSummary());
        assertEquals(
                WARNING_ICON,
                shadowOf(mNotificationsReviewPreference.getIcon()).getCreatedFromResId());
    }
}
