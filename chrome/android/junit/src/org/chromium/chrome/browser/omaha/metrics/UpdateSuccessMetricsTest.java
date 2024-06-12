// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import static org.mockito.ArgumentMatchers.any;
import static org.mockito.ArgumentMatchers.argThat;
import static org.mockito.Mockito.inOrder;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.os.Looper;

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
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Source;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Type;

/** Tests the API surface of UpdateSuccessMetrics. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UpdateSuccessMetricsTest {
    @Mock private TrackingProvider mProvider;

    private UpdateSuccessMetrics mMetrics;

    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Before
    public void setUp() {
        mMetrics = new UpdateSuccessMetrics(mProvider);
    }

    /** Tests that StartTracking properly persists the right tracking information. */
    @Test
    public void testStartTracking() {
        when(mProvider.get()).thenReturn(Promise.fulfilled(null));

        InOrder order = inOrder(mProvider);

        mMetrics.startUpdate();

        Shadows.shadowOf(Looper.myLooper()).runToEndOfTasks();
        order.verify(mProvider).put(argThat(new TrackingMatcher(Type.INTENT, Source.FROM_MENU)));
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

    private static Tracking buildProto() {
        return Tracking.newBuilder()
                .setTimestampMs(System.currentTimeMillis())
                .setVersion(VersionConstants.PRODUCT_VERSION)
                .setType(Type.INTENT)
                .setSource(Source.FROM_MENU)
                .setRecordedSession(false)
                .build();
    }

    private static class TrackingMatcher implements ArgumentMatcher<Tracking> {
        private final Type mType;
        private final Source mSource;

        public TrackingMatcher(Type type, Source source) {
            mType = type;
            mSource = source;
        }

        @Override
        public boolean matches(Tracking argument) {
            return mType.equals(argument.getType()) && mSource.equals(argument.getSource());
        }
    }
}
