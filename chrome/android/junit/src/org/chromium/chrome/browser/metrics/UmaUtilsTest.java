// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

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

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.HistogramWatcher;

/** Unit tests for {@link UmaUtils}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UmaUtilsTest {
    private static final String HISTOGRAM_IS_BACKGROUND_RESTRICTED =
            "Android.BackgroundRestrictions.IsBackgroundRestricted";
    private static final String HISTOGRAM_STANDBY_BUCKET =
            "Android.BackgroundRestrictions.StandbyBucket";
    private static final String HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION =
            "Android.BackgroundRestrictions.StandbyBucket.WithUserRestriction";
    private static final String HISTOGRAM_MINIDUMP_UPLOADING_TIME =
            "Stability.Android.MinidumpUploadingTime";

    private ShadowActivityManager mShadowActivityManager;
    private ShadowUsageStatsManager mShadowUsageStatsManager;

    @Before
    public void setUp() {
        mShadowActivityManager =
                Shadows.shadowOf(
                        (ActivityManager)
                                RuntimeEnvironment.getApplication()
                                        .getSystemService(Context.ACTIVITY_SERVICE));
        mShadowUsageStatsManager =
                Shadows.shadowOf(
                        (UsageStatsManager)
                                RuntimeEnvironment.getApplication()
                                        .getSystemService(Context.USAGE_STATS_SERVICE));
    }

    @Test
    @Config(sdk = Build.VERSION_CODES.O_MR1)
    public void testRecordBackgroundRestrictions_noRestrictionsUnderPie() {
        // Arrange
        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectNoRecords(HISTOGRAM_IS_BACKGROUND_RESTRICTED)
                        .expectNoRecords(HISTOGRAM_STANDBY_BUCKET)
                        .expectNoRecords(HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION)
                        .build();

        // Act
        UmaUtils.recordBackgroundRestrictions();

        // Assert
        histogramWatcher.assertExpected();
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

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HISTOGRAM_IS_BACKGROUND_RESTRICTED, false)
                        .expectIntRecord(HISTOGRAM_STANDBY_BUCKET, expectedUmaStandbyBucketStatus)
                        .expectNoRecords(HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION)
                        .build();

        // Act
        UmaUtils.recordBackgroundRestrictions();

        // Assert
        histogramWatcher.assertExpected();
    }

    private void doTestRecordBackgroundRestrictions_restricted(
            int androidStandbyBucketStatus, int expectedUmaStandbyBucketStatus) {
        // Arrange
        mShadowActivityManager.setBackgroundRestricted(true);
        mShadowUsageStatsManager.setCurrentAppStandbyBucket(androidStandbyBucketStatus);

        var histogramWatcher =
                HistogramWatcher.newBuilder()
                        .expectBooleanRecord(HISTOGRAM_IS_BACKGROUND_RESTRICTED, true)
                        .expectIntRecord(HISTOGRAM_STANDBY_BUCKET, expectedUmaStandbyBucketStatus)
                        .expectIntRecord(
                                HISTOGRAM_STANDBY_BUCKET_WITH_USER_RESTRICTION,
                                expectedUmaStandbyBucketStatus)
                        .build();

        // Act
        UmaUtils.recordBackgroundRestrictions();

        // Assert
        histogramWatcher.assertExpected();
    }
}
