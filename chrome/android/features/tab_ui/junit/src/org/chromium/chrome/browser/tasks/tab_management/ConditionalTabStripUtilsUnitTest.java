// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.hamcrest.CoreMatchers.equalTo;
import static org.junit.Assert.assertThat;

import android.content.SharedPreferences;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.MockitoAnnotations;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils.FeatureStatus;
import org.chromium.chrome.browser.tasks.ConditionalTabStripUtils.UserStatus;
import org.chromium.chrome.test.util.browser.Features;

/**
 * Tests for {@link org.chromium.chrome.browser.tasks.ConditionalTabStripUtils}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ConditionalTabStripUtilsUnitTest {
    @Rule
    public TestRule mProcessor = new Features.JUnitProcessor();

    private SharedPreferences mSharedPreference;

    @Before
    public void setUp() {
        MockitoAnnotations.initMocks(this);

        mSharedPreference = ContextUtils.getAppSharedPreferences();

        // Initialize the feature status.
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.DEFAULT);
        ConditionalTabStripUtils.setContinuousDismissCount(0);
        ConditionalTabStripUtils.setOptOutIndicator(false);
    }

    @Test
    public void testSetGetFeatureStatus() {
        assertThat(mSharedPreference.getInt(ConditionalTabStripUtils.FEATURE_STATUS, -1),
                equalTo(FeatureStatus.DEFAULT));

        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.ACTIVATED);

        assertThat(mSharedPreference.getInt(ConditionalTabStripUtils.FEATURE_STATUS, -1),
                equalTo(FeatureStatus.ACTIVATED));
        assertThat(ConditionalTabStripUtils.getFeatureStatus(), equalTo(FeatureStatus.ACTIVATED));
    }

    @Test
    public void testSetGetContinuousDismissCount() {
        assertThat(
                mSharedPreference.getInt(ConditionalTabStripUtils.CONTINUOUS_DISMISS_COUNTER, -1),
                equalTo(0));

        ConditionalTabStripUtils.setContinuousDismissCount(2);

        assertThat(
                mSharedPreference.getInt(ConditionalTabStripUtils.FEATURE_STATUS, -1), equalTo(2));
        assertThat(ConditionalTabStripUtils.getContinuousDismissCount(), equalTo(2));
    }

    @Test
    public void testSetGetOptOutIndicator() {
        assertThat(mSharedPreference.getBoolean(ConditionalTabStripUtils.OPT_OUT_INDICATOR, true),
                equalTo(false));

        ConditionalTabStripUtils.setOptOutIndicator(true);

        assertThat(mSharedPreference.getBoolean(ConditionalTabStripUtils.OPT_OUT_INDICATOR, false),
                equalTo(true));
        assertThat(ConditionalTabStripUtils.getOptOutIndicator(), equalTo(true));
    }

    @Test
    public void testUpdateDismissCounter_plusOne() {
        ConditionalTabStripUtils.setContinuousDismissCount(0);
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.FORBIDDEN);

        ConditionalTabStripUtils.updateFeatureExpiration(-1);

        assertThat(ConditionalTabStripUtils.getContinuousDismissCount(), equalTo(1));
    }

    @Test
    public void testUpdateDismissCounter_noUpdate() {
        ConditionalTabStripUtils.setContinuousDismissCount(2);
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.DEFAULT);

        ConditionalTabStripUtils.updateFeatureExpiration(-1);

        assertThat(ConditionalTabStripUtils.getContinuousDismissCount(), equalTo(2));
    }

    @Test
    public void testUpdateDismissCounter_reset() {
        ConditionalTabStripUtils.setContinuousDismissCount(2);
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.ACTIVATED);

        ConditionalTabStripUtils.updateFeatureExpiration(-1);

        assertThat(ConditionalTabStripUtils.getContinuousDismissCount(), equalTo(0));
    }

    @Test
    public void testUserStatusUMA_NoUser() {
        assertThat(ConditionalTabStripUtils.getLastShownTimeStamp(), equalTo(-1L));

        verifyUMAUserStatusRecord(UserStatus.NON_USER);
    }

    @Test
    public void testUserStatusUMA_TabStripNotShown() {
        ConditionalTabStripUtils.updateLastShownTimeStamp();
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.DEFAULT);

        verifyUMAUserStatusRecord(UserStatus.TAB_STRIP_NOT_SHOWN);
    }

    @Test
    public void testUserStatusUMA_TabStripShown() {
        ConditionalTabStripUtils.updateLastShownTimeStamp();
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.ACTIVATED);

        verifyUMAUserStatusRecord(UserStatus.TAB_STRIP_SHOWN);
    }

    @Test
    public void testUserStatusUMA_TabStripShownAndDismissed() {
        ConditionalTabStripUtils.updateLastShownTimeStamp();
        ConditionalTabStripUtils.setFeatureStatus(FeatureStatus.FORBIDDEN);

        verifyUMAUserStatusRecord(UserStatus.TAB_STRIP_SHOWN_AND_DISMISSED);
    }

    @Test
    public void testUserStatusUMA_TabStripPermanentlyHidden() {
        ConditionalTabStripUtils.updateLastShownTimeStamp();
        ConditionalTabStripUtils.setOptOutIndicator(true);

        verifyUMAUserStatusRecord(UserStatus.TAB_STRIP_PERMANENTLY_HIDDEN);
    }

    @Test
    public void testShouldShowSnackbar_OptOut() {
        ConditionalTabStripUtils.setContinuousDismissCount(
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_DISMISS_COUNTER_ABANDONED);

        assertThat(ConditionalTabStripUtils.shouldShowSnackbarForDismissal(), equalTo(true));
    }

    @Test
    public void testShouldShowSnackbar_BeyondLimit() {
        ConditionalTabStripUtils.setContinuousDismissCount(
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_LIMIT.getValue());

        assertThat(ConditionalTabStripUtils.shouldShowSnackbarForDismissal(), equalTo(true));
    }

    @Test
    public void testShouldNotShowSnackbar() {
        ConditionalTabStripUtils.setContinuousDismissCount(
                ConditionalTabStripUtils.CONDITIONAL_TAB_STRIP_INFOBAR_PERIOD.getValue() - 1);

        assertThat(ConditionalTabStripUtils.shouldShowSnackbarForDismissal(), equalTo(false));
    }

    private void verifyUMAUserStatusRecord(@UserStatus int userStatus) {
        int oldNoUserCount = getHistogramValueCountForUserStatus(UserStatus.NON_USER);
        int oldTabStripNotShownCount =
                getHistogramValueCountForUserStatus(UserStatus.TAB_STRIP_NOT_SHOWN);
        int oldTabStripShownCount = getHistogramValueCountForUserStatus(UserStatus.TAB_STRIP_SHOWN);
        int oldTabStripShownAndDismissedCount =
                getHistogramValueCountForUserStatus(UserStatus.TAB_STRIP_SHOWN_AND_DISMISSED);
        int oldTabStripPermanentlyHiddenCount =
                getHistogramValueCountForUserStatus(UserStatus.TAB_STRIP_PERMANENTLY_HIDDEN);

        // Trigger user action record.
        ConditionalTabStripUtils.updateFeatureExpiration(-1);

        int currentNoUserCount = getHistogramValueCountForUserStatus(UserStatus.NON_USER);
        int currentTabStripNotShownCount =
                getHistogramValueCountForUserStatus(UserStatus.TAB_STRIP_NOT_SHOWN);
        int currentTabStripShownCount =
                getHistogramValueCountForUserStatus(UserStatus.TAB_STRIP_SHOWN);
        int currentTabStripShownAndDismissedCount =
                getHistogramValueCountForUserStatus(UserStatus.TAB_STRIP_SHOWN_AND_DISMISSED);
        int currentTabStripPermanentlyHiddenCount =
                getHistogramValueCountForUserStatus(UserStatus.TAB_STRIP_PERMANENTLY_HIDDEN);

        assertThat(currentNoUserCount - oldNoUserCount,
                equalTo(userStatus == UserStatus.NON_USER ? 1 : 0));
        assertThat(currentTabStripNotShownCount - oldTabStripNotShownCount,
                equalTo(userStatus == UserStatus.TAB_STRIP_NOT_SHOWN ? 1 : 0));
        assertThat(currentTabStripShownCount - oldTabStripShownCount,
                equalTo(userStatus == UserStatus.TAB_STRIP_SHOWN ? 1 : 0));
        assertThat(currentTabStripShownAndDismissedCount - oldTabStripShownAndDismissedCount,
                equalTo(userStatus == UserStatus.TAB_STRIP_SHOWN_AND_DISMISSED ? 1 : 0));
        assertThat(currentTabStripPermanentlyHiddenCount - oldTabStripPermanentlyHiddenCount,
                equalTo(userStatus == UserStatus.TAB_STRIP_PERMANENTLY_HIDDEN ? 1 : 0));
    }

    private int getHistogramValueCountForUserStatus(@UserStatus int userStatus) {
        return RecordHistogram.getHistogramValueCountForTesting(
                ConditionalTabStripUtils.UMA_USER_STATUS_RESULT, userStatus);
    }
}
