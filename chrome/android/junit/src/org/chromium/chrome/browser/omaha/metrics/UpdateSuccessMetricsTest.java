// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import static org.mockito.Mockito.any;
import static org.mockito.Mockito.argThat;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Looper;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Source;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Type;
import org.chromium.chrome.test.util.browser.Features;
import org.chromium.components.version_info.VersionConstants;

/** Tests the API surface of UpdateSuccessMetrics. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UpdateSuccessMetricsTest {
    private static final int FAILED = 0;
    private static final int SUCCESS = 1;

    private static final int NOT_UPDATING = 0;
    private static final int UPDATING = 1;
    private static final String NOT_CURRENT_VERSION = "---";

    @Mock
    private TrackingProvider mProvider;

    private UpdateSuccessMetrics mMetrics;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public TestRule mFeaturesProcessorRule = new Features.JUnitProcessor();

    @Before
    public void setUp() {
        mMetrics = new UpdateSuccessMetrics(mProvider);
    }

    @After
    public void tearDown() {
        UmaRecorderHolder.resetForTesting();
    }

    /** Tests that StartTracking properly persists the right tracking information. */
    @Test
    public void testStartTracking() {
        when(mProvider.get()).thenReturn(Promise.fulfilled(null));

        InOrder order = inOrder(mProvider);

        mMetrics.startUpdate();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INTENT, Source.FROM_MENU)));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.StartingUpdateState", NOT_UPDATING));
    }

    /**
     * Tests that StartTracking properly persists the right tracking information even when already
     * tracking an update.
     */
    @Test
    public void testStartTrackingWhenAlreadyTracking() {
        when(mProvider.get()).thenReturn(Promise.fulfilled(buildProto()));

        InOrder order = inOrder(mProvider);

        mMetrics.startUpdate();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INTENT, Source.FROM_MENU)));

        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.StartingUpdateState", UPDATING));
    }

    /** Tests having no persisted state. */
    @Test
    public void testAnalyzeNoState() {
        when(mProvider.get()).thenReturn(Promise.fulfilled(null));

        mMetrics.analyzeFirstStatus();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, never()).clear();
        verify(mProvider, never()).put(any());
    }

    /** Tests recording a session success. */
    @Test
    public void testRecordSessionSuccess() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(
                        buildProto(System.currentTimeMillis(), NOT_CURRENT_VERSION, false)));
        mMetrics.analyzeFirstStatus();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(1)).clear();
        verify(mProvider, never()).put(any());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Intent.Menu", SUCCESS));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Intent.Menu", SUCCESS));
    }

    /** Tests recording a session failure. */
    @Test
    public void testRecordSessionFailure() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(buildProto(
                        System.currentTimeMillis(), VersionConstants.PRODUCT_VERSION, false)));
        mMetrics.analyzeFirstStatus();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, never()).clear();
        verify(mProvider, times(1))
                .put(argThat(new TrackingMatcher(
                        VersionConstants.PRODUCT_VERSION, Type.INTENT, Source.FROM_MENU, true)));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Intent.Menu", FAILED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Intent.Menu", FAILED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Intent.Menu", SUCCESS));
    }

    /** Tests recording a time window success. */
    @Test
    public void testRecordTimeWindowSuccess() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(buildProto(1, NOT_CURRENT_VERSION, false)));
        mMetrics.analyzeFirstStatus();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(1)).clear();
        verify(mProvider, never()).put(any());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Intent.Menu", SUCCESS));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Intent.Menu", SUCCESS));
    }

    /** Tests recording a time window failure. */
    @Test
    public void testRecordTimeWindowFailure() {
        when(mProvider.get())
                .thenReturn(
                        Promise.fulfilled(buildProto(1, VersionConstants.PRODUCT_VERSION, false)));
        mMetrics.analyzeFirstStatus();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(1)).clear();
        verify(mProvider, never()).put(any());
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Intent.Menu", FAILED));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Intent.Menu", FAILED));
    }

    /** Tests recording a time window failure. */
    @Test
    public void testRecordTimeWindowSuccessSessionAlreadyRecorded() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(buildProto(1, NOT_CURRENT_VERSION, true)));
        mMetrics.analyzeFirstStatus();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(1)).clear();
        verify(mProvider, never()).put(any());
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Intent.Menu", FAILED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Intent.Menu", SUCCESS));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Intent.Menu", SUCCESS));
    }

    /** Tests recording session failure only happens once. */
    @Test
    public void testNoDuplicateSessionFailures() {
        when(mProvider.get())
                .thenReturn(
                        Promise.fulfilled(buildProto(1, VersionConstants.PRODUCT_VERSION, true)));
        mMetrics.analyzeFirstStatus();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(1)).clear();
        verify(mProvider, never()).put(any());
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Intent.Menu", FAILED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Intent.Menu", SUCCESS));
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Intent.Menu", FAILED));
    }

    private static Tracking buildProto() {
        return Tracking.newBuilder()
                .setTimestampMs(System.currentTimeMillis())
                .setVersion(VersionConstants.PRODUCT_VERSION)
                .setType(Type.INTENT)
                .setSource(Source.FROM_MENU)
                .setRecordedSession(false)
                .build();
    }

    private static Tracking buildProto(long timestamp, String version, boolean recordedSession) {
        return Tracking.newBuilder()
                .setTimestampMs(timestamp)
                .setVersion(version)
                .setType(Type.INTENT)
                .setSource(Source.FROM_MENU)
                .setRecordedSession(recordedSession)
                .build();
    }

    private static class TrackingMatcher implements ArgumentMatcher<Tracking> {
        private final String mVersion;
        private final boolean mRecordedSession;
        private final Type mType;
        private final Source mSource;

        public TrackingMatcher(Type type, Source source) {
            this(VersionConstants.PRODUCT_VERSION, type, source, false);
        }

        public TrackingMatcher(String version, Type type, Source source, boolean recordedSession) {
            mVersion = version;
            mType = type;
            mSource = source;
            mRecordedSession = recordedSession;
        }

        @Override
        public boolean matches(Tracking argument) {
            return mVersion.equals(argument.getVersion()) && mType.equals(argument.getType())
                    && mSource.equals(argument.getSource())
                    && mRecordedSession == argument.getRecordedSession();
        }
    }
}
