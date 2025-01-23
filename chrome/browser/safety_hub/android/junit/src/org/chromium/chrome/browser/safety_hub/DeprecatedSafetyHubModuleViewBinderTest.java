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
import org.chromium.base.test.util.Features;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
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
public class DeprecatedSafetyHubModuleViewBinderTest {
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
    private PropertyModel mNotificationsReviewPropertyModel;
    private SafetyHubExpandablePreference mNotificationsReviewPreference;
    private PropertyModel mBrowserStatePropertyModel;
    private CardPreference mBrowserStatePreference;

    @Before
    public void setUp() {
        mActivity = Robolectric.buildActivity(TestActivity.class).get();

        // Set up password check preference.
        mPasswordCheckPreference = new SafetyHubExpandablePreference(mActivity, null);

        mPasswordCheckPropertyModel =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties
                                        .PASSWORD_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mPasswordCheckPropertyModel,
                mPasswordCheckPreference,
                DeprecatedSafetyHubModuleViewBinder::bindPasswordCheckProperties);

        // Set up update check preference.
        mUpdateCheckPreference = new SafetyHubExpandablePreference(mActivity, null);

        mUpdateCheckPropertyModel =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties
                                        .UPDATE_CHECK_SAFETY_HUB_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mUpdateCheckPropertyModel,
                mUpdateCheckPreference,
                DeprecatedSafetyHubModuleViewBinder::bindUpdateCheckProperties);

        // Set up notifications review preference.
        mNotificationsReviewPreference = new SafetyHubExpandablePreference(mActivity, null);
        mNotificationsReviewPropertyModel =
                new PropertyModel.Builder(
                                DeprecatedSafetyHubModuleProperties
                                        .NOTIFICATIONS_REVIEW_MODULE_KEYS)
                        .build();
        PropertyModelChangeProcessor.create(
                mNotificationsReviewPropertyModel,
                mNotificationsReviewPreference,
                DeprecatedSafetyHubModuleViewBinder::bindNotificationsReviewProperties);

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
    public void testPasswordCheckModule_NoCompromisedPasswords() {
        int totalPasswordsCount = 5;
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, 0);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.ACCOUNT_EMAIL, TEST_ACCOUNT_EMAIL);

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
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
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
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void
            testPasswordCheckModule_NoCompromisedPasswords_WeakAndReusedPasswordsExists_Enabled() {
        int totalPasswordsCount = 5;
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, 0);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT, weakPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT, reusedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.ACCOUNT_EMAIL, TEST_ACCOUNT_EMAIL);

        String expectedTitle = mActivity.getString(R.string.safety_hub_reused_weak_passwords_title);
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

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPasswordCheckPreference.getPrimaryButtonText());
        assertNull(mPasswordCheckPreference.getSecondaryButtonText());

        // Verify the managed state.
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
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
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void testPasswordCheckModule_NoCompromisedPasswords_WeakPasswordsExists_Enabled() {
        int totalPasswordsCount = 5;
        int weakPasswordsCount = 1;
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, 0);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT, weakPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT, 0);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.ACCOUNT_EMAIL, TEST_ACCOUNT_EMAIL);

        String expectedTitle = mActivity.getString(R.string.safety_hub_reused_weak_passwords_title);
        String expectedSummary =
                mActivity
                        .getResources()
                        .getQuantityString(
                                R.plurals.safety_hub_weak_passwords_summary,
                                weakPasswordsCount,
                                weakPasswordsCount);
        String expectedPrimaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertEquals(expectedPrimaryButtonText, mPasswordCheckPreference.getPrimaryButtonText());
        assertNull(mPasswordCheckPreference.getSecondaryButtonText());

        // Verify the managed state.
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
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
    @Features.DisableFeatures(ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS)
    public void
            testPasswordCheckModule_NoCompromisedPasswords_WeakAndReusedPasswordsExists_Disabled() {
        int totalPasswordsCount = 5;
        int weakPasswordsCount = 1;
        int reusedPasswordsCount = 2;
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT, 0);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT, weakPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT, reusedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.ACCOUNT_EMAIL, TEST_ACCOUNT_EMAIL);

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
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
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
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void testPasswordCheckModule_CompromisedPasswordsExist() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = 5;
        int weakPasswordsCount = 6;
        int reusedPasswordsCount = 6;
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        // Compromised passwords take priority over any other type, hence, the weak or reused
        // passwords counts should be ignored.
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT, weakPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT, reusedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

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

        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
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
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, 0);

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
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
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
    @Features.DisableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void
            testPasswordCheckModule_CompromisedCountUnavailable_NoWeakAndReusedPasswords_Disabled() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT, weakPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT, reusedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

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
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, false);
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
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
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
    @Features.EnableFeatures({
        ChromeFeatureList.SAFETY_HUB,
        ChromeFeatureList.SAFETY_HUB_WEAK_AND_REUSED_PASSWORDS
    })
    public void
            testPasswordCheckModule_CompromisedCountUnavailable_NoWeakAndReusedPasswords_Enabled() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = -1;
        int weakPasswordsCount = 0;
        int reusedPasswordsCount = 0;
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.WEAK_PASSWORDS_COUNT, weakPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.REUSED_PASSWORDS_COUNT, reusedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

        String expectedTitle =
                mActivity.getString(R.string.safety_hub_no_reused_weak_passwords_title);
        String expectedSummary =
                mActivity.getString(
                        R.string
                                .safety_hub_unavailable_compromised_no_reused_weak_passwords_summary);
        String expectedSecondaryButtonText =
                mActivity.getString(R.string.safety_hub_passwords_navigation_button);

        assertEquals(expectedTitle, mPasswordCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mPasswordCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mPasswordCheckPreference.getIcon()).getCreatedFromResId());
        assertEquals(
                expectedSecondaryButtonText, mPasswordCheckPreference.getSecondaryButtonText());
        assertNull(mPasswordCheckPreference.getPrimaryButtonText());

        // Verify the managed state.
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.IS_CONTROLLED_BY_POLICY, true);
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

        mUpdateCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

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

        mUpdateCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

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

        mUpdateCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);

        String expectedTitle = mActivity.getString(R.string.menu_update_unsupported);
        String expectedSummary =
                mActivity.getString(R.string.menu_update_unsupported_summary_default);

        assertEquals(expectedTitle, mUpdateCheckPreference.getTitle().toString());
        assertEquals(expectedSummary, mUpdateCheckPreference.getSummary().toString());
        assertEquals(INFO_ICON, shadowOf(mUpdateCheckPreference.getIcon()).getCreatedFromResId());
    }

    @Test
    public void testUpdateCheckModule_StatusNotReady() {
        mUpdateCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, null);

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
    public void testNotificationsReviewModule_NoNotificationPermissions() {
        mNotificationsReviewPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT, 0);

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
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
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

    @Test
    public void testModuleOrder_AllSafeStates() {
        UpdateStatusProvider.UpdateStatus updateStatus = new UpdateStatusProvider.UpdateStatus();
        updateStatus.updateState = UpdateStatusProvider.UpdateState.NONE;
        updateStatus.latestVersion = "1.1.1.1";
        int totalPasswordsCount = 1;
        int compromisedPasswordsCount = 0;
        int notificationPermissionsForReviewCount = 0;

        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mUpdateCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, updateStatus);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);
        mNotificationsReviewPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        List<Integer> actualOrder =
                Arrays.asList(
                        mUpdateCheckPreference.getOrder(),
                        mPasswordCheckPreference.getOrder(),
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
                        mNotificationsReviewPreference.getOrder()));
    }

    @Test
    public void testModuleOrder_MixedStates() {
        int totalPasswordsCount = 10;
        int compromisedPasswordsCount = 6;
        int notificationPermissionsForReviewCount = 5;

        // Unmanaged warning state should rank first.
        mPasswordCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.IS_SIGNED_IN, true);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.COMPROMISED_PASSWORDS_COUNT,
                compromisedPasswordsCount);
        mPasswordCheckPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.TOTAL_PASSWORDS_COUNT, totalPasswordsCount);

        // Unavailable state should rank after warning states.
        mUpdateCheckPropertyModel.set(DeprecatedSafetyHubModuleProperties.UPDATE_STATUS, null);

        // Info state should rank above safe.
        mNotificationsReviewPropertyModel.set(
                DeprecatedSafetyHubModuleProperties.NOTIFICATION_PERMISSIONS_FOR_REVIEW_COUNT,
                notificationPermissionsForReviewCount);

        List<Integer> actualOrder =
                Arrays.asList(
                        mUpdateCheckPreference.getOrder(),
                        mPasswordCheckPreference.getOrder(),
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
                        mNotificationsReviewPreference.getOrder()));
    }
}
