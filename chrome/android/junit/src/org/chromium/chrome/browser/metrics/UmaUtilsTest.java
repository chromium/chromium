// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import static org.junit.Assert.assertEquals;

import android.app.ActivityManager;
import android.app.usage.UsageStatsManager;
import android.content.Context;
import android.os.Build;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowActivityManager;
import org.robolectric.shadows.ShadowUsageStatsManager;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.MetricsUtils.HistogramDelta;

/**
 * Unit tests for {@link UmaUtils}.
 *
 * TODO(crbug.com/1411456): Use HistogramDelta for total counts too.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UmaUtilsTest {
    private static final String HISTOGRAM_IS_BACKGROUND_RESTRICTED =
            "Android.BackgroundRestrictions.IsBackgroundRestricted";
    private static final String HISTOGRAM_STANDBY_BUCKET =
            "Android.BackgroundRestrictions.StandbyBucket";
    private static final String HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION =
            "Android.BackgroundRestrictions.StandbyBucket.WithUserRestriction";
    private static final String HISTOGRAM_STANDBY_BUCKET_WITHOUT_USER_RESTRICTION =
            "Android.BackgroundRestrictions.StandbyBucket.WithoutUserRestriction";
    private static final int FALSE_BUCKET = 0;
    private static final int TRUE_BUCKET = 1;
    private static final String HISTOGRAM_MINIDUMP_UPLOADING_TIME =
            "Stability.Android.MinidumpUploadingTime";
    private static final String HISTOGRAM_MINIDUMP_UPLOADING_TIME_ACTIVE =
            "Stability.Android.MinidumpUploadingTime.Active";

    private ShadowActivityManager mShadowActivityManager;
    private ShadowUsageStatsManager mShadowUsageStatsManager;

    private HistogramDelta mDeltaIsBackgroundRestricted;
    private HistogramDelta mDeltaStandbyBucket;
    private HistogramDelta mDeltaStandbyBucketWithUserRestriction;
    private HistogramDelta mDeltaStandbyBucketWithoutUserRestriction;
    private HistogramDelta mDeltaMinidumpUploadingTime;
    private HistogramDelta mDeltaMinidumpUploadingTimeSpecific;

    @Before
    public void setUp() {
        mShadowActivityManager = Shadows.shadowOf(
                (ActivityManager) RuntimeEnvironment.getApplication().getSystemService(
                        Context.ACTIVITY_SERVICE));
        mShadowUsageStatsManager = Shadows.shadowOf(
                (UsageStatsManager) RuntimeEnvironment.getApplication().getSystemService(
                        Context.USAGE_STATS_SERVICE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.O_MR1)
    public void testRecordBackgroundRestrictions_noRestrictionsUnderPie() {
        // Act
        UmaUtils.recordBackgroundRestrictions();

        // Assert
        assertHistogramNotRecorded(HISTOGRAM_IS_BACKGROUND_RESTRICTED);
        assertHistogramNotRecorded(HISTOGRAM_STANDBY_BUCKET);
        assertHistogramNotRecorded(HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION);
        assertHistogramNotRecorded(HISTOGRAM_STANDBY_BUCKET_WITHOUT_USER_RESTRICTION);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_notRestricted_standbyBucketActive() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_ACTIVE;
        int expectedUmaStandbyBucketStatus = 0; // StandbyBucketStatus.ACTIVE is 0 in enums.xml

        doTestRecordBackgroundRestrictions_notRestricted(
                androidStandbyBucketStatus, expectedUmaStandbyBucketStatus);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_notRestricted_standbyBucketWorkingSet() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_WORKING_SET;
        int expectedUmaStandbyBucketStatus = 1; // StandbyBucketStatus.WORKING_SET is 1 in enums.xml

        doTestRecordBackgroundRestrictions_notRestricted(
                androidStandbyBucketStatus, expectedUmaStandbyBucketStatus);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_notRestricted_standbyBucketFrequent() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_FREQUENT;
        int expectedUmaStandbyBucketStatus = 2; // StandbyBucketStatus.FREQUENT is 2 in enums.xml

        doTestRecordBackgroundRestrictions_notRestricted(
                androidStandbyBucketStatus, expectedUmaStandbyBucketStatus);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_restricted_standbyBucketRare() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_RARE;
        int expectedUmaStandbyBucketStatus = 3; // StandbyBucketStatus.RARE is 3 in enums.xml

        doTestRecordBackgroundRestrictions_restricted(
                androidStandbyBucketStatus, expectedUmaStandbyBucketStatus);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_notRestricted_standbyBucketRare() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_RARE;
        int expectedUmaStandbyBucketStatus = 3; // StandbyBucketStatus.RARE is 3 in enums.xml

        doTestRecordBackgroundRestrictions_notRestricted(
                androidStandbyBucketStatus, expectedUmaStandbyBucketStatus);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_restricted_standbyBucketRestricted() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_RESTRICTED;
        int expectedUmaStandbyBucketStatus = 4; // StandbyBucketStatus.RESTRICTED is 4 in enums.xml

        doTestRecordBackgroundRestrictions_restricted(
                androidStandbyBucketStatus, expectedUmaStandbyBucketStatus);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_notRestricted_standbyBucketExempted() {
        int androidStandbyBucketStatus = 5; // UsageStatsManager.STANDBY_BUCKET_EXEMPTED
        int expectedStandbyBucketStatus = 6; // StandbyBucketStatus.EXEMPTED is 6 in enums.xml

        doTestRecordBackgroundRestrictions_notRestricted(
                androidStandbyBucketStatus, expectedStandbyBucketStatus);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_restricted_standbyBucketNever() {
        int androidStandbyBucketStatus = 50; // UsageStatsManager.STANDBY_BUCKET_NEVER
        int expectedUmaStandbyBucketStatus = 7; // StandbyBucketStatus.NEVER is 7 in enums.xml

        doTestRecordBackgroundRestrictions_restricted(
                androidStandbyBucketStatus, expectedUmaStandbyBucketStatus);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordBackgroundRestrictions_restricted_standbyBucketOther() {
        int androidStandbyBucketStatus = 42; // UsageStatsManager.STANDBY_BUCKET_OTHER
        int expectedUmaStandbyBucketStatus = 8; // StandbyBucketStatus.OTHER is 8 in enums.xml

        doTestRecordBackgroundRestrictions_restricted(
                androidStandbyBucketStatus, expectedUmaStandbyBucketStatus);
    }

    private void doTestRecordBackgroundRestrictions_notRestricted(
            int androidStandbyBucketStatus, int expectedUmaStandbyBucketStatus) {
        // Arrange
        mShadowActivityManager.setBackgroundRestricted(false);
        mShadowUsageStatsManager.setCurrentAppStandbyBucket(androidStandbyBucketStatus);

        mDeltaIsBackgroundRestricted =
                new HistogramDelta(HISTOGRAM_IS_BACKGROUND_RESTRICTED, FALSE_BUCKET);
        mDeltaStandbyBucket =
                new HistogramDelta(HISTOGRAM_STANDBY_BUCKET, expectedUmaStandbyBucketStatus);
        mDeltaStandbyBucketWithUserRestriction = null;
        mDeltaStandbyBucketWithoutUserRestriction = new HistogramDelta(
                HISTOGRAM_STANDBY_BUCKET_WITHOUT_USER_RESTRICTION, expectedUmaStandbyBucketStatus);

        // Act
        UmaUtils.recordBackgroundRestrictions();

        // Assert
        assertBooleanHistogramRecordedOnce(HISTOGRAM_IS_BACKGROUND_RESTRICTED,
                /* recordedValue */ true, mDeltaIsBackgroundRestricted);
        assertIntHistogramRecordedOnce(HISTOGRAM_STANDBY_BUCKET,
                /* recordedValue */ expectedUmaStandbyBucketStatus, mDeltaStandbyBucket);
        assertHistogramNotRecorded(HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION);
        assertIntHistogramRecordedOnce(HISTOGRAM_STANDBY_BUCKET_WITHOUT_USER_RESTRICTION,
                expectedUmaStandbyBucketStatus, mDeltaStandbyBucketWithoutUserRestriction);
    }

    private void doTestRecordBackgroundRestrictions_restricted(
            int androidStandbyBucketStatus, int expectedUmaStandbyBucketStatus) {
        // Arrange
        mShadowActivityManager.setBackgroundRestricted(true);
        mShadowUsageStatsManager.setCurrentAppStandbyBucket(androidStandbyBucketStatus);

        mDeltaIsBackgroundRestricted =
                new HistogramDelta(HISTOGRAM_IS_BACKGROUND_RESTRICTED, TRUE_BUCKET);
        mDeltaStandbyBucket =
                new HistogramDelta(HISTOGRAM_STANDBY_BUCKET, expectedUmaStandbyBucketStatus);
        mDeltaStandbyBucketWithUserRestriction = new HistogramDelta(
                HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION, expectedUmaStandbyBucketStatus);
        mDeltaStandbyBucketWithoutUserRestriction = null;

        // Act
        UmaUtils.recordBackgroundRestrictions();

        // Assert
        assertBooleanHistogramRecordedOnce(HISTOGRAM_IS_BACKGROUND_RESTRICTED,
                /* recordedValue */ true, mDeltaIsBackgroundRestricted);
        assertIntHistogramRecordedOnce(HISTOGRAM_STANDBY_BUCKET,
                /* recordedValue */ expectedUmaStandbyBucketStatus, mDeltaStandbyBucket);
        assertIntHistogramRecordedOnce(HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION,
                expectedUmaStandbyBucketStatus, mDeltaStandbyBucketWithUserRestriction);
        assertHistogramNotRecorded(HISTOGRAM_STANDBY_BUCKET_WITHOUT_USER_RESTRICTION);
    }

    private static final int UPLOAD_TIME_MS = 5678;

    @Test
    @Config(sdk = Build.VERSION_CODES.O_MR1)
    public void testRecordMinidumpUploadingTime_unsupported() {
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.Unsupported";

        // Arrange
        mDeltaMinidumpUploadingTime =
                new HistogramDelta(HISTOGRAM_MINIDUMP_UPLOADING_TIME, UPLOAD_TIME_MS);
        mDeltaMinidumpUploadingTimeSpecific = new HistogramDelta(specificHistogram, UPLOAD_TIME_MS);

        // Act
        UmaUtils.recordMinidumpUploadingTime(UPLOAD_TIME_MS);

        // Assert
        assertIntHistogramRecordedOnce(
                HISTOGRAM_MINIDUMP_UPLOADING_TIME, UPLOAD_TIME_MS, mDeltaMinidumpUploadingTime);
        assertHistogramNotRecorded(specificHistogram);
        assertHistogramNotRecorded(HISTOGRAM_MINIDUMP_UPLOADING_TIME_ACTIVE);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordMinidumpUploadingTime_active() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_ACTIVE;
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.Active";
        doTestRecordMinidumpUploadingTime(androidStandbyBucketStatus, specificHistogram);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordMinidumpUploadingTime_workingSet() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_WORKING_SET;
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.WorkingSet";
        doTestRecordMinidumpUploadingTime(androidStandbyBucketStatus, specificHistogram);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordMinidumpUploadingTime_frequent() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_FREQUENT;
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.Frequent";
        doTestRecordMinidumpUploadingTime(androidStandbyBucketStatus, specificHistogram);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordMinidumpUploadingTime_rare() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_RARE;
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.Rare";
        doTestRecordMinidumpUploadingTime(androidStandbyBucketStatus, specificHistogram);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordMinidumpUploadingTime_restricted() {
        int androidStandbyBucketStatus = UsageStatsManager.STANDBY_BUCKET_RESTRICTED;
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.Restricted";
        doTestRecordMinidumpUploadingTime(androidStandbyBucketStatus, specificHistogram);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordMinidumpUploadingTime_exempted() {
        int androidStandbyBucketStatus = 5; // UsageStatsManager.STANDBY_BUCKET_EXEMPTED
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.Exempted";
        doTestRecordMinidumpUploadingTime(androidStandbyBucketStatus, specificHistogram);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordMinidumpUploadingTime_never() {
        int androidStandbyBucketStatus = 50; // UsageStatsManager.STANDBY_BUCKET_NEVER
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.Never";
        doTestRecordMinidumpUploadingTime(androidStandbyBucketStatus, specificHistogram);
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.P)
    public void testRecordMinidumpUploadingTime_other() {
        int androidStandbyBucketStatus = 42;
        String specificHistogram = "Stability.Android.MinidumpUploadingTime.Other";
        doTestRecordMinidumpUploadingTime(androidStandbyBucketStatus, specificHistogram);
    }

    private void doTestRecordMinidumpUploadingTime(
            int androidStandbyBucketStatus, String specificHistogram) {
        // Arrange
        mShadowUsageStatsManager.setCurrentAppStandbyBucket(androidStandbyBucketStatus);
        mDeltaMinidumpUploadingTime =
                new HistogramDelta(HISTOGRAM_MINIDUMP_UPLOADING_TIME, UPLOAD_TIME_MS);
        mDeltaMinidumpUploadingTimeSpecific = new HistogramDelta(specificHistogram, UPLOAD_TIME_MS);

        // Act
        UmaUtils.recordMinidumpUploadingTime(UPLOAD_TIME_MS);

        // Assert
        assertIntHistogramRecordedOnce(
                HISTOGRAM_MINIDUMP_UPLOADING_TIME, UPLOAD_TIME_MS, mDeltaMinidumpUploadingTime);
        assertIntHistogramRecordedOnce(
                specificHistogram, UPLOAD_TIME_MS, mDeltaMinidumpUploadingTimeSpecific);
        // Check .Active as evidence that other histograms weren't recorded, unless we are testing
        // the Active status itself.
        if (androidStandbyBucketStatus != UsageStatsManager.STANDBY_BUCKET_ACTIVE) {
            assertHistogramNotRecorded(HISTOGRAM_MINIDUMP_UPLOADING_TIME_ACTIVE);
        }
    }

    // TODO(crbug.com/1411456): Use HistogramDelta for total counts too. Move to MetricsUtils.

    /**
     * Asserts that the given histogram was not recorded during this test.
     * @param histogram the histogram name
     */
    private static void assertHistogramNotRecorded(String histogram) {
        int actualCount = RecordHistogram.getHistogramTotalCountForTesting(histogram);
        assertEquals("Expected no records of histogram " + histogram + " but it was recorded "
                        + actualCount + " time(s)",
                0, actualCount);
    }

    private static void assertHistogramRecordedOnce(String histogram) {
        int actualCount = RecordHistogram.getHistogramTotalCountForTesting(histogram);
        assertEquals("Expected 1 record of histogram " + histogram + " but it was recorded "
                        + actualCount + " time(s)",
                1, actualCount);
    }

    /**
     * Asserts that the given histogram was recorded only once and with a given value during this
     * test.
     * @param histogram the histogram name
     * @param recordedValue the expected boolean value
     */
    private static void assertBooleanHistogramRecordedOnce(
            String histogram, boolean recordedValue, HistogramDelta delta) {
        int expectedBucket = recordedValue ? 1 : 0;
        assertHistogramRecordedOnceToBucket(histogram, expectedBucket, delta);
    }

    /**
     * Asserts that the given histogram was recorded only once and with a given value during this
     * test.
     * @param histogram the histogram name
     * @param recordedValue the expected int value
     */
    private static void assertIntHistogramRecordedOnce(
            String histogram, int recordedValue, HistogramDelta delta) {
        assertHistogramRecordedOnceToBucket(histogram, recordedValue, delta);
    }

    private static void assertHistogramRecordedOnceToBucket(
            String histogram, int expectedBucket, HistogramDelta delta) {
        assertHistogramRecordedOnce(histogram);
        int actualCountInBucket = delta.getDelta();
        assertEquals("Expected 1 record of histogram " + histogram + " with value " + expectedBucket
                        + " but bucket had " + actualCountInBucket + " record(s)",
                1, actualCountInBucket);
    }
}
