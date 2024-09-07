// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.safety_hub;

import static org.hamcrest.Matchers.contains;
import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertThat;
import static org.junit.Assert.assertTrue;
import static org.robolectric.Shadows.shadowOf;

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

import java.util.Arrays;
import java.util.Collections;
import java.util.HashSet;
import java.util.List;

@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
public class SafetyHubModuleViewBinderTest {
    private static final @DrawableRes int SAFE_ICON = R.drawable.material_ic_check_24dp;
    private static final @DrawableRes int WARNING_ICON = R.drawable.ic_error;
    private static final @DrawableRes int INFO_ICON = R.drawable.btn_info;
    private static final @DrawableRes int MANAGED_ICON = R.drawable.ic_business;
    private static final String TEST_ACCOUNT_EMAIL = "test@gmail.com";
    private Activity mActivity;
    private PropertyModel mPasswordCheckPropertyModel;
    private SafetyHubExpandablePreference mPasswordCheckPreference;
    private PropertyModel mUpdateCheckPropertyModel;
    private SafetyHubExpandablePreference mUpdateCheckPreference;
    private PropertyModel mPermissionsPropertyModel;
    private SafetyHubExpandablePreference mPermissionsPreference;
    private PropertyModel mNotificationsReviewPropertyModel;
    private SafetyHubExpandablePreference mNotificationsReviewPreference;
    private PropertyModel mSafeBrowsingPropertyModel;
    private SafetyHubExpandablePreference mSafeBrowsingPreference;
    private PropertyModel mBrowserStatePropertyModel;
    private CardPreference mBrowserStatePreference;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        // Set up password check preference.
        mPasswordCheckPreference = new SafetyHubExpandablePreference(mActivity, null);

        mPasswordCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mPasswordCheckPropertyModel,
                mPasswordCheckPreference,
                SafetyHubModuleViewBinder::bindPasswordCheckProperties);

        // Set up update check preference.
        mUpdateCheckPreference = new SafetyHubExpandablePreference(mActivity, null);

        mUpdateCheckPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mUpdateCheckPropertyModel,
                mUpdateCheckPreference,
                SafetyHubModuleViewBinder::bindUpdateCheckProperties);

        // Set up permissions preference.
        mPermissionsPreference = new SafetyHubExpandablePreference(mActivity, null);
        mPermissionsPropertyModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.PERMISSIONS_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mPermissionsPropertyModel,
                mPermissionsPreference,
                SafetyHubModuleViewBinder::bindPermissionsProperties);

        // Set up notifications review preference.
        mNotificationsReviewPreference = new SafetyHubExpandablePreference(mActivity, null);
        mNotificationsReviewPropertyModel =
                new PropertyModel.Builder(
                                SafetyHubModuleProperties.NOTIFICATIONS_REVIEW_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mNotificationsReviewPropertyModel,
                mNotificationsReviewPreference,
                SafetyHubModuleViewBinder::bindNotificationsReviewProperties);

        // Set up safe browsing preference.
        mSafeBrowsingPreference = new SafetyHubExpandablePreference(mActivity, null);
        mSafeBrowsingPropertyModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.SAFE_BROWSING_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mSafeBrowsingPropertyModel,
                mSafeBrowsingPreference,
                SafetyHubModuleViewBinder::bindSafeBrowsingProperties);

        // Set up browser state preference.
        mBrowserStatePreference = new CardPreference(mActivity, null);
        mBrowserStatePropertyModel =
                new PropertyModel.Builder(SafetyHubModuleProperties.BROWSER_STATE_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mBrowserStatePropertyModel,
                mBrowserStatePreference,
                SafetyHubModuleViewBinder::bindBrowserStateProperties);
    }

    @Test
    public void testPasswordCheckModule_NoCompromisedPasswords() {
        int totalPasswordsCount = 5;
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, 0);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.ACCOUNT_EMAIL, TEST_ACCOUNT_EMAIL);

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_compromised_passwords_title);
        String expectedSummary =
                mActivity.getString(
                        R.string.safety_hub_password_check_time_recently, TEST_ACCOUNT_EMAIL);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());

        // Verify the managed state.
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());
    }

    @Test
    public void testPasswordCheckModule_CompromisedPasswordsExist() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = 5;
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_SIGNED_IN, true);

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

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(
                WARNING_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPasswordCheckPreference.getPrimaryButtonText());
        assertNull(mPasswordCheckPreference.getSecondaryButtonText());

        // Verify the managed state.
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(
                MANAGED_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());
    }

    @Test
    public void testPasswordCheckModule_NoPasswordsSaved() {
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, 0);
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_SIGNED_IN, true);

        String expectedTitle = mActivity.getString(R.string.safety_hub_no_passwords_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_no_passwords_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());

        // Verify the managed state.
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(
                MANAGED_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());
    }

    @Test
    public void testPasswordCheckModule_CompromisedCountUnavailable() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_SIGNED_IN, true);

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_password_check_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());

        // Verify the signed out state.
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_SIGNED_IN, false);
        String expectedSignedOutSummary =
                mActivity.getString(R.string.safety_hub_password_check_signed_out_summary);
        expectedSecondaryButtonText = mActivity.getString(R.string.sign_in_to_chrome);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedSignedOutSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());

        // Verify the managed state.
        mPasswordCheckPropertyModel.set(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_no_passwords_summary_managed);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(
                MANAGED_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());
    }

    @Test
    public void testUpdateCheckModule_UpToDate() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;

        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        String expectedTitle = mActivity.getString(R.string.safety_check_updates_updated);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_google_play_button);

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertEquals(SAFE_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mUpdateCheckPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mUpdateCheckPreference.getSecondaryButtonText());
    }

    @Test
    public void testUpdateCheckModule_UpdateAvailable() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.UPDATE_AVAILABLE;

        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        String expectedTitle = mActivity.getString(R.string.safety_check_updates_outdated);
        String expectedPrimaryButtonText = mActivity.getString(R.string.menu_update);

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertEquals(
                WARNING_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mUpdateCheckPreference.getPrimaryButtonText());
        assertNull(mUpdateCheckPreference.getSecondaryButtonText());
    }

    @Test
    public void testUpdateCheckModule_UnsupportedOsVersion() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.UNSUPPORTED_OS_VERSION;
        updateStatus.latestUnsupportedVersion = "1.1.1.1";

        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        String expectedTitle = mActivity.getString(R.string.menu_update_unsupported);
        String expectedSummary =
                mActivity.getString(R.string.menu_update_unsupported_summary_default);

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mUpdateCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testUpdateCheckModule_StatusNotReady() {
        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, null);

        String expectedTitle = mActivity.getString(R.string.safety_hub_update_unavailable_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_unavailable_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_google_play_button);

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mUpdateCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
        assertNull(mUpdateCheckPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mUpdateCheckPreference.getSecondaryButtonText());
    }

    @Test
    public void testPermissionsModule_NoSitesWithUnusedPermissions() {
        mPermissionsPropertyModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT, 0);

        String expectedTitle = mActivity.getString(R.string.safety_hub_permissions_ok_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_permissions_ok_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_site_settings_button);

        assertEquals(expectedTitle, mPermissionsPreference.getTitle().toString());
        assertEquals(expectedSummary, mPermissionsPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mPermissionsPreference.getIcon()).getCreatedFromResId());
        assertNull(mPermissionsPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPermissionsPreference.getSecondaryButtonText());
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
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_permissions_warning_summary);
        String expectedPrimaryButtonText = mActivity.getString(R.string.got_it);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_view_sites_button);

        assertEquals(expectedTitle, mPermissionsPreference.getTitle().toString());
        assertEquals(expectedSummary, mPermissionsPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPermissionsPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPermissionsPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mPermissionsPreference.getSecondaryButtonText());
    }

    @Test
    public void testNotificationsReviewModule_NoNotificationPermissions() {
        mNotificationsReviewPropertyModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT, 0);

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_notifications_review_ok_title);
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_notifications_review_ok_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_notification_settings_button);

        assertEquals(expectedTitle, mNotificationsReviewPreference.getTitle().toString());
        assertEquals(expectedSummary, mNotificationsReviewPreference.getSummary().toString());
        assertEquals(
                SAFE_ICON,
                shadowOf(mNotificationsReviewPreference.getIcon()).getCreatedFromResId());
        assertNull(mNotificationsReviewPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText,
                mNotificationsReviewPreference.getSecondaryButtonText());
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
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_notifications_review_warning_summary);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_notifications_reset_all_button);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_view_sites_button);

        assertEquals(expectedTitle, mNotificationsReviewPreference.getTitle().toString());
        assertEquals(expectedSummary, mNotificationsReviewPreference.getSummary().toString());
        assertEquals(
                INFO_ICON,
                shadowOf(mNotificationsReviewPreference.getIcon()).getCreatedFromResId());
        assertEquals(
                expectedPrimaryButtonText, mNotificationsReviewPreference.getPrimaryButtonText());
        assertEquals(
                expectedSecondaryButtonText,
                mNotificationsReviewPreference.getSecondaryButtonText());
    }

    @Test
    public void testSafeBrowsingModule_StandardSafeBrowsing() {
        @SafeBrowsingState int safeBrowsingState = SafeBrowsingState.STANDARD_PROTECTION;

        mSafeBrowsingPropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        String expectedTitle = mActivity.getString(R.string.safety_hub_safe_browsing_on_title);
        String expectedSummary = mActivity.getString(R.string.safety_hub_safe_browsing_on_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_security_settings_button);

        assertEquals(expectedTitle, mSafeBrowsingPreference.getTitle().toString());
        assertEquals(expectedSummary, mSafeBrowsingPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mSafeBrowsingPreference.getIcon()).getCreatedFromResId());
        assertNull(mSafeBrowsingPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mSafeBrowsingPreference.getSecondaryButtonText());

        // Verify the managed state.
        mSafeBrowsingPropertyModel.set(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_safe_browsing_on_summary_managed);
        assertEquals(expectedTitle, mSafeBrowsingPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mSafeBrowsingPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mSafeBrowsingPreference.getIcon()).getCreatedFromResId());
        assertNull(mSafeBrowsingPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mSafeBrowsingPreference.getSecondaryButtonText());
    }

    @Test
    public void testSafeBrowsingModule_EnhancedSafeBrowsing() {
        @SafeBrowsingState int safeBrowsingState = SafeBrowsingState.ENHANCED_PROTECTION;

        mSafeBrowsingPropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        String expectedTitle =
                mActivity.getString(R.string.safety_hub_safe_browsing_enhanced_title);
        String expectedSummary =
                mActivity.getString(R.string.safety_hub_safe_browsing_enhanced_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_security_settings_button);

        assertEquals(expectedTitle, mSafeBrowsingPreference.getTitle().toString());
        assertEquals(expectedSummary, mSafeBrowsingPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mSafeBrowsingPreference.getIcon()).getCreatedFromResId());
        assertNull(mSafeBrowsingPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mSafeBrowsingPreference.getSecondaryButtonText());

        // Verify the managed state.
        mSafeBrowsingPropertyModel.set(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_safe_browsing_enhanced_summary_managed);
        assertEquals(expectedTitle, mSafeBrowsingPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mSafeBrowsingPreference.getSummary().toString());
        assertEquals(SAFE_ICON, shadowOf(mSafeBrowsingPreference.getIcon()).getCreatedFromResId());
        assertNull(mSafeBrowsingPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mSafeBrowsingPreference.getSecondaryButtonText());
    }

    @Test
    public void testSafeBrowsingModule_SafeBrowsingOff() {
        @SafeBrowsingState int safeBrowsingState = SafeBrowsingState.NO_SAFE_BROWSING;

        mSafeBrowsingPropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        String expectedTitle =
                mActivity.getString(R.string.prefs_safe_browsing_no_protection_summary);
        String expectedSummary = mActivity.getString(R.string.safety_hub_safe_browsing_off_summary);
        String expectedPrimaryButtonText = mActivity.getString(R.string.safety_hub_turn_on_button);

        assertEquals(expectedTitle, mSafeBrowsingPreference.getTitle().toString());
        assertEquals(expectedSummary, mSafeBrowsingPreference.getSummary().toString());
        assertEquals(
                WARNING_ICON, shadowOf(mSafeBrowsingPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mSafeBrowsingPreference.getPrimaryButtonText());
        assertNull(mSafeBrowsingPreference.getSecondaryButtonText());

        // Verify the managed state.
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_go_to_security_settings_button);

        mSafeBrowsingPropertyModel.set(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
        String expectedManagedSummary =
                mActivity.getString(R.string.safety_hub_safe_browsing_off_summary_managed);
        assertEquals(expectedTitle, mSafeBrowsingPreference.getTitle().toString());
        assertEquals(expectedManagedSummary, mSafeBrowsingPreference.getSummary().toString());
        assertEquals(
                MANAGED_ICON, shadowOf(mSafeBrowsingPreference.getIcon()).getCreatedFromResId());
        assertNull(mSafeBrowsingPreference.getPrimaryButtonText());
        assertEquals(expectedSecondaryButtonText, mSafeBrowsingPreference.getSecondaryButtonText());
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

        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;
        updateStatus.latestVersion = "1.1.1.1";
        safeBrowsingState = SafeBrowsingState.NO_SAFE_BROWSING;

        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        safeBrowsingState = SafeBrowsingState.STANDARD_PROTECTION;
        totalPasswordsCount = 5;
        compromisedPasswordsCount = 1;

        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

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
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
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

        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mBrowserStatePropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        mBrowserStatePropertyModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        assertTrue(mBrowserStatePreference.isVisible());
    }

    @Test
    public void testModuleOrder_AllSafeStates() {
        @SafeBrowsingState int safeBrowsingState = SafeBrowsingState.STANDARD_PROTECTION;
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;
        updateStatus.latestVersion = "1.1.1.1";
        int totalPasswordsCount = 1;
        int compromisedPasswordsCount = 0;
        int sitesWithUnusedPermissionsCount = 0;
        int notificationPermissionsForReviewCount = 0;

        mSafeBrowsingPropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPermissionsPropertyModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);
        mNotificationsReviewPropertyModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        List<Integer> actualOrder =
                Arrays.asList(
                        mUpdateCheckPreference.getOrder(),
                        mPasswordCheckPreference.getOrder(),
                        mSafeBrowsingPreference.getOrder(),
                        mPermissionsPreference.getOrder(),
                        mNotificationsReviewPreference.getOrder());
        Collections.sort(actualOrder);

        // Verify that there are no duplicate orders.
        assertEquals(actualOrder.size(), new HashSet<>(actualOrder).size());
        // Verify the actual order of modules reflects the expected order.
        assertThat(
                actualOrder,
                contains(
                        mUpdateCheckPreference.getOrder(),
                        mPasswordCheckPreference.getOrder(),
                        mSafeBrowsingPreference.getOrder(),
                        mPermissionsPreference.getOrder(),
                        mNotificationsReviewPreference.getOrder()));
    }

    @Test
    public void testModuleOrder_MixedStates() {
        @SafeBrowsingState int safeBrowsingState = SafeBrowsingState.NO_SAFE_BROWSING;
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = 6;
        int sitesWithUnusedPermissionsCount = 0;
        int notificationPermissionsForReviewCount = 5;

        // Unmanaged warning state should rank first.
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                SafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

        // Unavailable state should rank after warning states.
        mUpdateCheckPropertyModel.set(SafetyHubModuleProperties.UPDATE_STATUS, null);

        // Managed warning state should follow the same order as info state.
        mSafeBrowsingPropertyModel.set(
                SafetyHubModuleProperties.SAFE_BROWSING_STATE, safeBrowsingState);
        mSafeBrowsingPropertyModel.set(SafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);

        // Info state should rank above safe.
        mNotificationsReviewPropertyModel.set(
                SafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        // Safe state should rank last.
        mPermissionsPropertyModel.set(
                SafetyHubModuleProperties.SITES_WITH_UNUSED_PERMISSIONS_COUNT,
                sitesWithUnusedPermissionsCount);

        List<Integer> actualOrder =
                Arrays.asList(
                        mUpdateCheckPreference.getOrder(),
                        mPasswordCheckPreference.getOrder(),
                        mSafeBrowsingPreference.getOrder(),
                        mPermissionsPreference.getOrder(),
                        mNotificationsReviewPreference.getOrder());
        Collections.sort(actualOrder);

        // Verify that there are no duplicate orders.
        assertEquals(actualOrder.size(), new HashSet<>(actualOrder).size());
        // Verify the actual order of modules reflects the expected order.
        assertThat(
                actualOrder,
                contains(
                        mPasswordCheckPreference.getOrder(),
                        mUpdateCheckPreference.getOrder(),
                        mSafeBrowsingPreference.getOrder(),
                        mNotificationsReviewPreference.getOrder(),
                        mPermissionsPreference.getOrder()));
    }
}
