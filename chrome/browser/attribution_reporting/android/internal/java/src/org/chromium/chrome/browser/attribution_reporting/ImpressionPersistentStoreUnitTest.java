// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.attribution_reporting;

import static org.mockito.Mockito.anyLong;
import static org.mockito.Mockito.doThrow;
import static org.mockito.Mockito.eq;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.when;

import android.util.Pair;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.Mockito;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.mockito.quality.Strictness;
import org.robolectric.annotation.Config;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.JniMocker;
import org.chromium.chrome.browser.attribution_reporting.ImpressionPersistentStoreFileManager.AttributionFileProperties;
import org.chromium.chrome.browser.attribution_reporting.ImpressionPersistentStoreFileManager.CachedEnumMetric;

import java.io.Closeable;
import java.io.DataInput;
import java.io.DataOutput;
import java.io.EOFException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * Unit tests for the ImpressionPersistentStore
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ImpressionPersistentStoreUnitTest {
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

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule().strictness(Strictness.STRICT_STUBS);

    @Rule
    public final JniMocker mJniMocker = new JniMocker();

    interface Writer extends DataOutput, Closeable {}
    interface Reader extends DataInput, Closeable {}

    @Mock
    private Writer mOutputStream;

    @Mock
    private Reader mInputStream1;

    @Mock
    private Reader mInputStream2;

    @Mock
    private ImpressionPersistentStoreFileManager<Writer, Reader> mFileManager;

    @Mock
    private AttributionMetrics.Natives mMetricsJni;

    private ImpressionPersistentStore<Writer, Reader> mImpressionStore;
    private InOrder mInOrder;

    @Before
    public void setUp() {
        ThreadUtils.setThreadAssertsDisabledForTesting(true);
        mJniMocker.mock(AttributionMetricsJni.TEST_HOOKS, mMetricsJni);
        mImpressionStore = new ImpressionPersistentStore(mFileManager);
        mInOrder = inOrder(mOutputStream, mInputStream1, mInputStream2);
    }

    @Test
    @SmallTest
    public void testStoreImpression() throws Exception {
        when(mFileManager.getForPackage(PACKAGE_1, ImpressionPersistentStore.VERSION))
                .thenReturn(Pair.create(mOutputStream, 0L));
        Assert.assertFalse(mImpressionStore.storeImpression(PARAMS_1));
        mInOrder.verify(mOutputStream).writeUTF(EVENT_ID_1);
        mInOrder.verify(mOutputStream).writeUTF(DESTINATION_1);
        mInOrder.verify(mOutputStream).writeUTF(REPORT_TO_1);
        mInOrder.verify(mOutputStream).writeLong(EXPIRY_1);
        mInOrder.verify(mOutputStream).writeLong(anyLong());
        mInOrder.verify(mOutputStream).writeChar(ImpressionPersistentStore.SENTINEL);
        mInOrder.verify(mOutputStream).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mOutputStream);

        Mockito.verify(mFileManager, Mockito.times(1))
                .incrementEnumMetric(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE));
    }

    @Test
    @SmallTest
    public void testStoreImpression_NearlyFull() throws Exception {
        when(mFileManager.getForPackage(PACKAGE_1, ImpressionPersistentStore.VERSION))
                .thenReturn(Pair.create(
                        mOutputStream, (long) ImpressionPersistentStore.STORAGE_FLUSH_THRESHOLD));
        Assert.assertTrue(mImpressionStore.storeImpression(PARAMS_1));
        mInOrder.verify(mOutputStream).writeUTF(EVENT_ID_1);
        mInOrder.verify(mOutputStream).writeUTF(DESTINATION_1);
        mInOrder.verify(mOutputStream).writeUTF(REPORT_TO_1);
        mInOrder.verify(mOutputStream).writeLong(EXPIRY_1);
        mInOrder.verify(mOutputStream).writeLong(anyLong());
        mInOrder.verify(mOutputStream).writeChar(ImpressionPersistentStore.SENTINEL);
        mInOrder.verify(mOutputStream).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mOutputStream);

        Mockito.verify(mFileManager, Mockito.times(1))
                .incrementEnumMetric(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE));
    }

    @Test
    @SmallTest
    public void testStoreImpression_ExceedsStorageCap() throws Exception {
        when(mFileManager.getForPackage(PACKAGE_1, ImpressionPersistentStore.VERSION))
                .thenReturn(Pair.create(
                        mOutputStream, ImpressionPersistentStore.MAX_STORAGE_BYTES_PER_PACKAGE));
        Assert.assertTrue(mImpressionStore.storeImpression(PARAMS_1));
        mInOrder.verify(mOutputStream).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mOutputStream);
        Mockito.verify(mFileManager, Mockito.times(1))
                .incrementEnumMetric(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME),
                        eq(AttributionMetrics.AttributionEvent.DROPPED_STORAGE_FULL));
        Mockito.verify(mFileManager, Mockito.times(0))
                .incrementEnumMetric(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE));
    }

    @Test
    @SmallTest
    public void testStoreImpression_WithException() throws Exception {
        when(mFileManager.getForPackage(PACKAGE_1, ImpressionPersistentStore.VERSION))
                .thenReturn(Pair.create(mOutputStream, 0L));
        doThrow(new IOException()).when(mOutputStream).writeUTF(EVENT_ID_1);
        Assert.assertFalse(mImpressionStore.storeImpression(PARAMS_1));
        mInOrder.verify(mOutputStream).writeUTF(EVENT_ID_1);
        mInOrder.verify(mOutputStream).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mOutputStream);
        Mockito.verify(mFileManager, Mockito.times(1))
                .incrementEnumMetric(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME),
                        eq(AttributionMetrics.AttributionEvent.DROPPED_WRITE_FAILED));
        Mockito.verify(mFileManager, Mockito.times(0))
                .incrementEnumMetric(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE));
    }

    @Test
    @SmallTest
    public void testStoreImpression_GetFileException() throws Exception {
        when(mFileManager.getForPackage(PACKAGE_1, ImpressionPersistentStore.VERSION))
                .thenThrow(new IOException());
        Assert.assertFalse(mImpressionStore.storeImpression(PARAMS_1));
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mOutputStream);
        Mockito.verify(mFileManager, Mockito.times(1))
                .incrementEnumMetric(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME),
                        eq(AttributionMetrics.AttributionEvent.DROPPED_WRITE_FAILED));
        Mockito.verify(mFileManager, Mockito.times(0))
                .incrementEnumMetric(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE));
    }

    @Test
    @SmallTest
    public void testReadImpressions_GetFileException() throws Exception {
        List<CachedEnumMetric> metrics = new ArrayList<CachedEnumMetric>();
        metrics.add(new CachedEnumMetric(AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE, 1234));
        when(mFileManager.getCachedEnumMetrics()).thenReturn(metrics);

        when(mFileManager.getAllAttributionFiles()).thenThrow(new IOException());
        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        Assert.assertTrue(params.isEmpty());

        Mockito.verify(mMetricsJni, Mockito.times(1))
                .recordEnumMetrics(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME), eq(0L),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE),
                        eq(AttributionMetrics.AttributionEvent.NUM_ENTRIES), eq(1234));
        Mockito.verify(mMetricsJni, Mockito.times(1))
                .recordEnumMetrics(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME), eq(0L),
                        eq(AttributionMetrics.AttributionEvent.DROPPED_READ_FAILED),
                        eq(AttributionMetrics.AttributionEvent.NUM_ENTRIES), eq(1234));
    }

    @Test
    @SmallTest
    public void testReadImpressions_VersionTooNew() throws Exception {
        List<CachedEnumMetric> metrics = new ArrayList<CachedEnumMetric>();
        metrics.add(new CachedEnumMetric(AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE, 1234));
        when(mFileManager.getCachedEnumMetrics()).thenReturn(metrics);

        List<AttributionFileProperties<Reader>> files = new ArrayList<>();
        files.add(new AttributionFileProperties<Reader>(
                mInputStream1, PACKAGE_1, ImpressionPersistentStore.VERSION + 1));
        when(mFileManager.getAllAttributionFiles()).thenReturn(files);
        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        mInOrder.verify(mInputStream1).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mInputStream1);
        Assert.assertTrue(params.isEmpty());

        Mockito.verify(mMetricsJni, Mockito.times(1))
                .recordEnumMetrics(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME), eq(0L),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE),
                        eq(AttributionMetrics.AttributionEvent.NUM_ENTRIES), eq(1234));
        Mockito.verify(mMetricsJni, Mockito.times(1))
                .recordEnumMetrics(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME), eq(0L),
                        eq(AttributionMetrics.AttributionEvent.DROPPED_READ_FAILED),
                        eq(AttributionMetrics.AttributionEvent.NUM_ENTRIES), eq(1234));
    }

    @Test
    @SmallTest
    public void testReadImpressions_Multiple_SingleFile() throws Exception {
        List<CachedEnumMetric> metrics = new ArrayList<CachedEnumMetric>();
        metrics.add(new CachedEnumMetric(AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE, 3));
        when(mFileManager.getCachedEnumMetrics()).thenReturn(metrics);

        List<AttributionFileProperties<Reader>> files = new ArrayList<>();
        files.add(new AttributionFileProperties<Reader>(
                mInputStream1, PACKAGE_1, ImpressionPersistentStore.VERSION));
        when(mFileManager.getAllAttributionFiles()).thenReturn(files);
        when(mInputStream1.readUTF())
                .thenReturn(EVENT_ID_1)
                .thenReturn(DESTINATION_1)
                .thenReturn(REPORT_TO_1)
                .thenReturn(EVENT_ID_2)
                .thenReturn(DESTINATION_2)
                .thenReturn("")
                .thenThrow(new EOFException());
        when(mInputStream1.readLong())
                .thenReturn(EXPIRY_1)
                .thenReturn(EVENT_TIME_1)
                .thenReturn(EXPIRY_2)
                .thenReturn(EVENT_TIME_2);
        when(mInputStream1.readChar()).thenReturn(ImpressionPersistentStore.SENTINEL);
        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        // First Attribution.
        mInOrder.verify(mInputStream1, times(3)).readUTF();
        mInOrder.verify(mInputStream1, times(2)).readLong();
        mInOrder.verify(mInputStream1).readChar();
        // Second Attribution.
        mInOrder.verify(mInputStream1, times(3)).readUTF();
        mInOrder.verify(mInputStream1, times(2)).readLong();
        mInOrder.verify(mInputStream1).readChar();
        // Third Attribution throws.
        mInOrder.verify(mInputStream1).readUTF();
        mInOrder.verify(mInputStream1).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mInputStream1);
        Assert.assertTrue(params.size() == 2);
        Assert.assertEquals(AttributionParameters.forCachedEvent(PACKAGE_1, EVENT_ID_1,
                                    DESTINATION_1, REPORT_TO_1, EXPIRY_1, EVENT_TIME_1),
                params.get(0));
        Assert.assertEquals(AttributionParameters.forCachedEvent(PACKAGE_1, EVENT_ID_2,
                                    DESTINATION_2, "", EXPIRY_2, EVENT_TIME_2),
                params.get(1));

        Mockito.verify(mMetricsJni, Mockito.times(1))
                .recordEnumMetrics(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME), eq(0L),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE),
                        eq(AttributionMetrics.AttributionEvent.NUM_ENTRIES), eq(3));
        Mockito.verify(mMetricsJni, Mockito.times(1))
                .recordEnumMetrics(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME), eq(0L),
                        eq(AttributionMetrics.AttributionEvent.DROPPED_READ_FAILED),
                        eq(AttributionMetrics.AttributionEvent.NUM_ENTRIES), eq(1));
    }

    @Test
    @SmallTest
    public void testReadImpressions_Multiple_TwoFiles() throws Exception {
        List<CachedEnumMetric> metrics = new ArrayList<CachedEnumMetric>();
        metrics.add(new CachedEnumMetric(AttributionMetrics.ATTRIBUTION_EVENTS_NAME,
                AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE, 4));
        when(mFileManager.getCachedEnumMetrics()).thenReturn(metrics);

        List<AttributionFileProperties<Reader>> files = new ArrayList<>();
        files.add(new AttributionFileProperties<Reader>(
                mInputStream1, PACKAGE_1, ImpressionPersistentStore.VERSION));
        files.add(new AttributionFileProperties<Reader>(
                mInputStream2, PACKAGE_2, ImpressionPersistentStore.VERSION));
        when(mFileManager.getAllAttributionFiles()).thenReturn(files);
        when(mInputStream1.readUTF())
                .thenReturn(EVENT_ID_1)
                .thenReturn(DESTINATION_1)
                .thenReturn(REPORT_TO_1)
                .thenThrow(new EOFException());
        when(mInputStream1.readLong()).thenReturn(EXPIRY_1).thenReturn(EVENT_TIME_1);
        when(mInputStream1.readChar()).thenReturn(ImpressionPersistentStore.SENTINEL);
        when(mInputStream2.readUTF())
                .thenReturn(EVENT_ID_2)
                .thenReturn(DESTINATION_2)
                .thenReturn("")
                .thenThrow(new EOFException());
        when(mInputStream2.readLong()).thenReturn(EXPIRY_2).thenReturn(EVENT_TIME_2);
        when(mInputStream2.readChar()).thenReturn(ImpressionPersistentStore.SENTINEL);
        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        // First Attribution, first package.
        mInOrder.verify(mInputStream1, times(3)).readUTF();
        mInOrder.verify(mInputStream1, times(2)).readLong();
        mInOrder.verify(mInputStream1).readChar();
        // Second Attribution throws.
        mInOrder.verify(mInputStream1).readUTF();
        mInOrder.verify(mInputStream1).close();

        // First Attribution, second package.
        mInOrder.verify(mInputStream2, times(3)).readUTF();
        mInOrder.verify(mInputStream2, times(2)).readLong();
        mInOrder.verify(mInputStream2).readChar();
        // Second Attribution throws.
        mInOrder.verify(mInputStream2).readUTF();
        mInOrder.verify(mInputStream2).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mInputStream1);
        Mockito.verifyNoMoreInteractions(mInputStream2);
        Assert.assertTrue(params.size() == 2);
        Assert.assertEquals(AttributionParameters.forCachedEvent(PACKAGE_1, EVENT_ID_1,
                                    DESTINATION_1, REPORT_TO_1, EXPIRY_1, EVENT_TIME_1),
                params.get(0));
        Assert.assertEquals(AttributionParameters.forCachedEvent(PACKAGE_2, EVENT_ID_2,
                                    DESTINATION_2, "", EXPIRY_2, EVENT_TIME_2),
                params.get(1));

        Mockito.verify(mMetricsJni, Mockito.times(1))
                .recordEnumMetrics(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME), eq(0L),
                        eq(AttributionMetrics.AttributionEvent.CACHED_PRE_NATIVE),
                        eq(AttributionMetrics.AttributionEvent.NUM_ENTRIES), eq(4));
        Mockito.verify(mMetricsJni, Mockito.times(1))
                .recordEnumMetrics(eq(AttributionMetrics.ATTRIBUTION_EVENTS_NAME), eq(0L),
                        eq(AttributionMetrics.AttributionEvent.DROPPED_READ_FAILED),
                        eq(AttributionMetrics.AttributionEvent.NUM_ENTRIES), eq(2));
    }

    @Test
    @SmallTest
    public void testReadImpressions_Multiple_Corruption() throws Exception {
        List<AttributionFileProperties<Reader>> files = new ArrayList<>();
        files.add(new AttributionFileProperties<Reader>(
                mInputStream1, PACKAGE_1, ImpressionPersistentStore.VERSION));
        when(mFileManager.getAllAttributionFiles()).thenReturn(files);
        when(mInputStream1.readUTF())
                .thenReturn(EVENT_ID_1)
                .thenReturn(DESTINATION_1)
                .thenReturn(REPORT_TO_1)
                .thenReturn(EVENT_ID_2)
                .thenReturn(DESTINATION_2)
                .thenReturn("")
                .thenThrow(new EOFException());
        when(mInputStream1.readLong())
                .thenReturn(EXPIRY_1)
                .thenReturn(EVENT_TIME_1)
                .thenReturn(EXPIRY_2)
                .thenReturn(EVENT_TIME_2);
        when(mInputStream1.readChar())
                .thenReturn(ImpressionPersistentStore.SENTINEL)
                .thenReturn('F');
        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        // First Attribution.
        mInOrder.verify(mInputStream1, times(3)).readUTF();
        mInOrder.verify(mInputStream1, times(2)).readLong();
        mInOrder.verify(mInputStream1).readChar();
        // Second Attribution.
        mInOrder.verify(mInputStream1, times(3)).readUTF();
        mInOrder.verify(mInputStream1, times(2)).readLong();
        mInOrder.verify(mInputStream1).readChar(); // Bad Sentinel
        mInOrder.verify(mInputStream1).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mInputStream1);
        // Second attribution is dropped.
        Assert.assertTrue(params.size() == 1);
        Assert.assertEquals(AttributionParameters.forCachedEvent(PACKAGE_1, EVENT_ID_1,
                                    DESTINATION_1, REPORT_TO_1, EXPIRY_1, EVENT_TIME_1),
                params.get(0));
    }

    @Test
    @SmallTest
    public void testReadImpressions_Multiple_Truncation() throws Exception {
        List<AttributionFileProperties<Reader>> files = new ArrayList<>();
        files.add(new AttributionFileProperties<Reader>(
                mInputStream1, PACKAGE_1, ImpressionPersistentStore.VERSION));
        when(mFileManager.getAllAttributionFiles()).thenReturn(files);
        when(mInputStream1.readUTF())
                .thenReturn(EVENT_ID_1)
                .thenReturn(DESTINATION_1)
                .thenReturn(REPORT_TO_1)
                .thenReturn(EVENT_ID_2)
                .thenReturn(DESTINATION_2)
                .thenThrow(new EOFException());
        when(mInputStream1.readLong()).thenReturn(EXPIRY_1).thenReturn(EVENT_TIME_1);
        when(mInputStream1.readChar()).thenReturn(ImpressionPersistentStore.SENTINEL);
        List<AttributionParameters> params = mImpressionStore.getAndClearStoredImpressions();
        // First Attribution.
        mInOrder.verify(mInputStream1, times(3)).readUTF();
        mInOrder.verify(mInputStream1, times(2)).readLong();
        mInOrder.verify(mInputStream1).readChar();
        // Second Attribution.
        mInOrder.verify(mInputStream1, times(3)).readUTF();
        mInOrder.verify(mInputStream1).close();
        mInOrder.verifyNoMoreInteractions();
        Mockito.verifyNoMoreInteractions(mInputStream1);
        // Second attribution is dropped.
        Assert.assertTrue(params.size() == 1);
        Assert.assertEquals(AttributionParameters.forCachedEvent(PACKAGE_1, EVENT_ID_1,
                                    DESTINATION_1, REPORT_TO_1, EXPIRY_1, EVENT_TIME_1),
                params.get(0));
    }
}
