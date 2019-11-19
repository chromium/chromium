// Copyright 2019 The Chromium Authors. All rights reserved.
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
import org.junit.runner.RunWith;
import org.mockito.ArgumentMatcher;
import org.mockito.InOrder;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;
import org.robolectric.Shadows;
import org.robolectric.annotation.Config;

import org.chromium.base.Promise;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateInteractionSource;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateState;
import org.chromium.chrome.browser.omaha.UpdateStatusProvider.UpdateStatus;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Source;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Type;
import org.chromium.chrome.browser.omaha.metrics.UpdateSuccessMetrics.UpdateType;
import org.chromium.components.version_info.VersionConstants;

import java.util.HashMap;
import java.util.Map;

/** Tests the API surface of UpdateSuccessMetrics. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class UpdateSuccessMetricsTest {
    private static final int FAILED = 0;
    private static final int SUCCESS = 1;

    private static final int NOT_UPDATING = 0;
    private static final int UPDATING = 1;

    @Mock
    private TrackingProvider mProvider;

    private UpdateSuccessMetrics mMetrics;

    @Rule
    public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        Map<String, Boolean> featureList = new HashMap<>();
        // The value we use does not matter.  ChromeFeatureList just needs to be initialized.
        featureList.put(ChromeFeatureList.INLINE_UPDATE_FLOW, false);
        ChromeFeatureList.setTestFeatures(featureList);

        mMetrics = new UpdateSuccessMetrics(mProvider);
    }

    @After
    public void tearDown() {
        ShadowRecordHistogram.reset();
    }

    /** Tests that StartTracking properly persists the right tracking information. */
    @Test
    public void testStartTracking() {
        when(mProvider.get()).thenReturn(Promise.fulfilled(null));

        InOrder order = inOrder(mProvider);

        mMetrics.startUpdate(UpdateType.INLINE, UpdateInteractionSource.FROM_MENU);
        mMetrics.startUpdate(UpdateType.INLINE, UpdateInteractionSource.FROM_INFOBAR);
        mMetrics.startUpdate(UpdateType.INLINE, UpdateInteractionSource.FROM_NOTIFICATION);
        mMetrics.startUpdate(UpdateType.INTENT, UpdateInteractionSource.FROM_MENU);
        mMetrics.startUpdate(UpdateType.INTENT, UpdateInteractionSource.FROM_INFOBAR);
        mMetrics.startUpdate(UpdateType.INTENT, UpdateInteractionSource.FROM_NOTIFICATION);

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INLINE, Source.FROM_MENU)));
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INLINE, Source.FROM_INFOBAR)));
        order.verify(mProvider).put(
                argThat(new TrackingMatcher(Type.INLINE, Source.FROM_NOTIFICATION)));
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INTENT, Source.FROM_MENU)));
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INTENT, Source.FROM_INFOBAR)));
        order.verify(mProvider).put(
                argThat(new TrackingMatcher(Type.INTENT, Source.FROM_NOTIFICATION)));

        Assert.assertEquals(6,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.StartingUpdateState", NOT_UPDATING));
    }

    /**
     * Tests that StartTracking properly persists the right tracking information even when already
     * tracking an update.
     */
    @Test
    public void testStartTrackingWhenAlreadyTracking() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(buildProto(Type.INLINE, Source.FROM_MENU)));

        InOrder order = inOrder(mProvider);

        mMetrics.startUpdate(UpdateType.INLINE, UpdateInteractionSource.FROM_MENU);
        mMetrics.startUpdate(UpdateType.INLINE, UpdateInteractionSource.FROM_INFOBAR);
        mMetrics.startUpdate(UpdateType.INLINE, UpdateInteractionSource.FROM_NOTIFICATION);
        mMetrics.startUpdate(UpdateType.INTENT, UpdateInteractionSource.FROM_MENU);
        mMetrics.startUpdate(UpdateType.INTENT, UpdateInteractionSource.FROM_INFOBAR);
        mMetrics.startUpdate(UpdateType.INTENT, UpdateInteractionSource.FROM_NOTIFICATION);

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INLINE, Source.FROM_MENU)));
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INLINE, Source.FROM_INFOBAR)));
        order.verify(mProvider).put(
                argThat(new TrackingMatcher(Type.INLINE, Source.FROM_NOTIFICATION)));
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INTENT, Source.FROM_MENU)));
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INTENT, Source.FROM_INFOBAR)));
        order.verify(mProvider).put(
                argThat(new TrackingMatcher(Type.INTENT, Source.FROM_NOTIFICATION)));

        Assert.assertEquals(6,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.StartingUpdateState", UPDATING));
    }

    /** Tests having no persisted state. */
    @Test
    public void testAnalyzeNoState() {
        when(mProvider.get()).thenReturn(Promise.fulfilled(null));

        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.NONE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UNSUPPORTED_OS_VERSION));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_DOWNLOADING));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_READY));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_FAILED));

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, never()).clear();
        verify(mProvider, never()).put(any());
    }

    /** Tests still updating when called. */
    @Test
    public void testAnalyzeStillUpdating() {
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_DOWNLOADING));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_READY));

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, never()).get();
        verify(mProvider, never()).clear();
        verify(mProvider, never()).put(any());
    }

    /** Tests recording a session success. */
    @Test
    public void testRecordSessionSuccess() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(buildProto(
                        System.currentTimeMillis(), "---", Type.INLINE, Source.FROM_MENU, false)));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.NONE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UNSUPPORTED_OS_VERSION));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_FAILED));

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(5)).clear();
        verify(mProvider, never()).put(any());
        Assert.assertEquals(5,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Inline.Menu", SUCCESS));
        Assert.assertEquals(5,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Inline.Menu", SUCCESS));
    }

    /** Tests recording a session failure. */
    @Test
    public void testRecordSessionFailure() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(buildProto(System.currentTimeMillis(),
                        VersionConstants.PRODUCT_VERSION, Type.INLINE, Source.FROM_MENU, false)));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.NONE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UNSUPPORTED_OS_VERSION));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_FAILED));

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, never()).clear();
        verify(mProvider, times(5))
                .put(argThat(new TrackingMatcher(
                        VersionConstants.PRODUCT_VERSION, Type.INLINE, Source.FROM_MENU, true)));
        Assert.assertEquals(5,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Inline.Menu", FAILED));
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Inline.Menu", FAILED));
    }

    /** Tests recording a time window success. */
    @Test
    public void testRecordTimeWindowSuccess() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(
                        buildProto(1, "---", Type.INLINE, Source.FROM_MENU, false)));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.NONE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UNSUPPORTED_OS_VERSION));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_FAILED));

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(5)).clear();
        verify(mProvider, never()).put(any());
        Assert.assertEquals(5,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Inline.Menu", SUCCESS));
        Assert.assertEquals(5,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Inline.Menu", SUCCESS));
    }

    /** Tests recording a time window failure. */
    @Test
    public void testRecordTimeWindowFailure() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(buildProto(1, VersionConstants.PRODUCT_VERSION,
                        Type.INLINE, Source.FROM_MENU, false)));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.NONE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UNSUPPORTED_OS_VERSION));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_FAILED));

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(5)).clear();
        verify(mProvider, never()).put(any());
        Assert.assertEquals(5,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Inline.Menu", FAILED));
        Assert.assertEquals(5,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Inline.Menu", FAILED));
    }

    /** Tests recording session failure only happens once. */
    @Test
    public void testNoDuplicateSessionFailures() {
        when(mProvider.get())
                .thenReturn(Promise.fulfilled(buildProto(
                        1, VersionConstants.PRODUCT_VERSION, Type.INLINE, Source.FROM_MENU, true)));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.NONE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.UNSUPPORTED_OS_VERSION));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_AVAILABLE));
        mMetrics.analyzeFirstStatus(buildStatus(UpdateState.INLINE_UPDATE_FAILED));

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        verify(mProvider, times(5)).clear();
        verify(mProvider, never()).put(any());
        CachedMetrics.commitCachedMetrics();
        Assert.assertEquals(0,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.Session.Inline.Menu", FAILED));
        Assert.assertEquals(5,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.Result.TimeWindow.Inline.Menu", FAILED));
    }

    private static Tracking buildProto(Type type, Source source) {
        return Tracking.newBuilder()
                .setTimestampMs(System.currentTimeMillis())
                .setVersion(VersionConstants.PRODUCT_VERSION)
                .setType(type)
                .setSource(source)
                .setRecordedSession(false)
                .build();
    }

    private static Tracking buildProto(
            long timestamp, String version, Type type, Source source, boolean recordedSession) {
        return Tracking.newBuilder()
                .setTimestampMs(timestamp)
                .setVersion(version)
                .setType(type)
                .setSource(source)
                .setRecordedSession(recordedSession)
                .build();
    }

    private static UpdateStatus buildStatus(@UpdateState int state) {
        UpdateStatus status = new UpdateStatus();
        status.updateState = state;
        return status;
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
