// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import android.text.TextUtils;

import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;

import java.io.DataInputStream;
import java.io.DataOutputStream;
import java.util.List;

/**
 * Test real file handling for the ImpressionPersistentStore
 */
@RunWith(BaseJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class ImpressionPersistentStoreFileTest {
    private static final String PACKAGE_1 = "org.package1";
    private static final String PACKAGE_2 = "org.package2";
    private static final String EVENT_ID_1 = "12345";
    private static final String EVENT_ID_2 = "23456";
    private static final String DESTINATION_1 = "https://example.com";
    private static final String DESTINATION_2 = "https://other.com";
    private static final String REPORT_TO_1 = "https://report.com";
    private static final String REPORT_TO_2 = null;
    private static final long EXPIRY_1 = 34567;
    private static final long EXPIRY_2 = 0;
    private static final long EVENT_TIME_1 = 5678;
    private static final long EVENT_TIME_2 = 6789;

    private static final AttributionParameters PARAMS_1 =
            new AttributionParameters(PACKAGE_1, EVENT_ID_1, DESTINATION_1, REPORT_TO_1, EXPIRY_1);
    private static final AttributionParameters PARAMS_2 =
            new AttributionParameters(PACKAGE_2, EVENT_ID_2, DESTINATION_2, REPORT_TO_2, EXPIRY_2);

    private ImpressionPersistentStoreFileManagerImpl mFileManager;

    private ImpressionPersistentStore<DataOutputStream, DataInputStream> mImpressionStore;

    private int mPreTestTotalHistogramCount;
    private int mPreTestCachedHistogramCount;

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryNoBrowserProcess();

        mFileManager = new ImpressionPersistentStoreFileManagerImpl();
        mImpressionStore = new ImpressionPersistentStore<>(mFileManager);

        mPreTestTotalHistogramCount = RecordHistogram.getHistogramTotalCountForTesting(
                AttributionMetrics.ATTRIBUTION_EVENTS_NAME);
        mPreTestCachedHistogramCount = RecordHistogram.getHistogramValueCountForTesting(
                AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE);
    }

    @After
    public void tearDown() {
        mFileManager.clearAllData();
    }

    private void assertAllButEventTimeEqual(
            AttributionParameters expected, AttributionParameters actual) {
        Assert.assertEquals(expected.getSourcePackageName(), actual.getSourcePackageName());
        Assert.assertEquals(expected.getSourceEventId(), actual.getSourceEventId());
        Assert.assertEquals(expected.getDestination(), actual.getDestination());
        if (TextUtils.isEmpty(expected.getReportTo())) {
            Assert.assertTrue(TextUtils.isEmpty(actual.getReportTo()));
        } else {
            Assert.assertEquals(expected.getReportTo(), actual.getReportTo());
        }
        Assert.assertEquals(expected.getExpiry(), actual.getExpiry());
    }

    private void assertEventTimeRecorded(AttributionParameters params, long before) {
        Assert.assertTrue(params.getEventTime() >= before);
        Assert.assertTrue(params.getEventTime() <= System.currentTimeMillis());
    }

    @Test
    @SmallTest
    public void testStoreImpressions_SameFile() throws Exception {
        long before = System.currentTimeMillis();
        AttributionParameters params2 = new AttributionParameters(
                PACKAGE_1, EVENT_ID_2, DESTINATION_2, REPORT_TO_2, EXPIRY_2);
        mImpressionStore.storeImpression(PARAMS_1);
        mImpressionStore.storeImpression(params2);
        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        assertAllButEventTimeEqual(PARAMS_1, params.get(0));
        assertAllButEventTimeEqual(params2, params.get(1));
        assertEventTimeRecorded(params.get(0), before);
        assertEventTimeRecorded(params.get(1), before);
        // Check that attributions were cleared.
        params = mImpressionStore.getAndClearStoredImpressions();
        Assert.assertTrue(params.isEmpty());
        Assert.assertEquals(2 + mPreTestCachedHistogramCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                        AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE));
        Assert.assertEquals(2 + mPreTestTotalHistogramCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME));
    }

    @Test
    @SmallTest
    public void testStoreImpressions_TwoFiles() throws Exception {
        long before = System.currentTimeMillis();
        mImpressionStore.storeImpression(PARAMS_1);
        mImpressionStore.storeImpression(PARAMS_2);
        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        AttributionParameters params1;
        AttributionParameters params2;
        if (params.get(0).getSourcePackageName().equals(PACKAGE_1)) {
            params1 = params.get(0);
            params2 = params.get(1);
        } else {
            params2 = params.get(0);
            params1 = params.get(1);
        }

        assertAllButEventTimeEqual(PARAMS_1, params1);
        assertAllButEventTimeEqual(PARAMS_2, params2);
        assertEventTimeRecorded(params1, before);
        assertEventTimeRecorded(params2, before);
        // Check that attributions were cleared.
        params = mImpressionStore.getAndClearStoredImpressions();
        Assert.assertTrue(params.isEmpty());
        Assert.assertEquals(2 + mPreTestCachedHistogramCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                        AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE));
        Assert.assertEquals(2 + mPreTestTotalHistogramCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME));
    }

    @Test
    @SmallTest
    public void testStoreImpressions_NoFilesWithMetrics() throws Exception {
        int writeFailedCount = RecordHistogram.getHistogramValueCountForTesting(
                AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                AttributionMetrics.AttributionEvent.DROPPED_WRITE_FAILED);
        int storageFullCount = RecordHistogram.getHistogramValueCountForTesting(
                AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                AttributionMetrics.AttributionEvent.DROPPED_STORAGE_FULL);
        long before = System.currentTimeMillis();

        mImpressionStore.cacheAttributionEvent(
                AttributionMetrics.AttributionEvent.DROPPED_WRITE_FAILED);
        mImpressionStore.cacheAttributionEvent(
                AttributionMetrics.AttributionEvent.DROPPED_STORAGE_FULL);
        mImpressionStore.cacheAttributionEvent(
                AttributionMetrics.AttributionEvent.DROPPED_STORAGE_FULL);

        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        Assert.assertTrue(params.isEmpty());

        Assert.assertEquals(3 + mPreTestTotalHistogramCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME));

        Assert.assertEquals(1 + writeFailedCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                        AttributionMetrics.AttributionEvent.DROPPED_WRITE_FAILED));

        Assert.assertEquals(2 + storageFullCount,
                RecordHistogram.getHistogramValueCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                        AttributionMetrics.AttributionEvent.DROPPED_STORAGE_FULL));
        // Check that metrics were cleared.
        params = mImpressionStore.getAndClearStoredImpressions();
        Assert.assertEquals(3 + mPreTestTotalHistogramCount,
                RecordHistogram.getHistogramTotalCountForTesting(
                        AttributionMetrics.ATTRIBUTION_EVENTS_NAME));
    }
}
