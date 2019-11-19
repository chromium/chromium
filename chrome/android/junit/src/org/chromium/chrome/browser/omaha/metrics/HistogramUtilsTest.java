// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.omaha.metrics;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.test.ShadowRecordHistogram;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Source;
import org.chromium.chrome.browser.omaha.metrics.UpdateProtos.Tracking.Type;
import org.chromium.chrome.browser.omaha.metrics.UpdateSuccessMetrics.AttributionType;

/** Tests for HistogramUtils. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE, shadows = {ShadowRecordHistogram.class})
public class HistogramUtilsTest {
    /** Tests {@link HistogramUtils#recordStartedUpdateHistogram(boolean)} */
    @Test
    public void testStartHistogram() {
        HistogramUtils.recordStartedUpdateHistogram(false);
        CachedMetrics.commitCachedMetrics();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.StartingUpdateState", 0));
        ShadowRecordHistogram.reset();

        HistogramUtils.recordStartedUpdateHistogram(true);
        CachedMetrics.commitCachedMetrics();
        Assert.assertEquals(1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "GoogleUpdate.StartingUpdateState", 1));
        ShadowRecordHistogram.reset();
    }

    /** Tests {@link HistogramUtils#recordResultHistogram(int, Tracking, boolean)}. */
    @Test
    public void testResultHistogram() {
        validateHistogram(-3, Type.UNKNOWN_TYPE, Source.UNKNOWN_SOURCE,
                "GoogleUpdate.Result.Unknown", "Unknown.Unknown");
        validateHistogram(-3, Type.UNKNOWN_TYPE, Source.FROM_MENU, "GoogleUpdate.Result.Unknown",
                "Unknown.Menu");
        validateHistogram(-3, Type.UNKNOWN_TYPE, Source.FROM_INFOBAR, "GoogleUpdate.Result.Unknown",
                "Unknown.Infobar");
        validateHistogram(-3, Type.UNKNOWN_TYPE, Source.FROM_NOTIFICATION,
                "GoogleUpdate.Result.Unknown", "Unknown.Notification");
        validateHistogram(-3, Type.INTENT, Source.UNKNOWN_SOURCE, "GoogleUpdate.Result.Unknown",
                "Intent.Unknown");
        validateHistogram(
                -3, Type.INTENT, Source.FROM_MENU, "GoogleUpdate.Result.Unknown", "Intent.Menu");
        validateHistogram(-3, Type.INTENT, Source.FROM_INFOBAR, "GoogleUpdate.Result.Unknown",
                "Intent.Infobar");
        validateHistogram(-3, Type.INTENT, Source.FROM_NOTIFICATION, "GoogleUpdate.Result.Unknown",
                "Intent.Notification");
        validateHistogram(-3, Type.INLINE, Source.UNKNOWN_SOURCE, "GoogleUpdate.Result.Unknown",
                "Inline.Unknown");
        validateHistogram(
                -3, Type.INLINE, Source.FROM_MENU, "GoogleUpdate.Result.Unknown", "Inline.Menu");
        validateHistogram(-3, Type.INLINE, Source.FROM_INFOBAR, "GoogleUpdate.Result.Unknown",
                "Inline.Infobar");
        validateHistogram(-3, Type.INLINE, Source.FROM_NOTIFICATION, "GoogleUpdate.Result.Unknown",
                "Inline.Notification");

        validateHistogram(AttributionType.SESSION, Type.UNKNOWN_TYPE, Source.UNKNOWN_SOURCE,
                "GoogleUpdate.Result.Session", "Unknown.Unknown");
        validateHistogram(AttributionType.SESSION, Type.UNKNOWN_TYPE, Source.FROM_MENU,
                "GoogleUpdate.Result.Session", "Unknown.Menu");
        validateHistogram(AttributionType.SESSION, Type.UNKNOWN_TYPE, Source.FROM_INFOBAR,
                "GoogleUpdate.Result.Session", "Unknown.Infobar");
        validateHistogram(AttributionType.SESSION, Type.UNKNOWN_TYPE, Source.FROM_NOTIFICATION,
                "GoogleUpdate.Result.Session", "Unknown.Notification");
        validateHistogram(AttributionType.SESSION, Type.INTENT, Source.UNKNOWN_SOURCE,
                "GoogleUpdate.Result.Session", "Intent.Unknown");
        validateHistogram(AttributionType.SESSION, Type.INTENT, Source.FROM_MENU,
                "GoogleUpdate.Result.Session", "Intent.Menu");
        validateHistogram(AttributionType.SESSION, Type.INTENT, Source.FROM_INFOBAR,
                "GoogleUpdate.Result.Session", "Intent.Infobar");
        validateHistogram(AttributionType.SESSION, Type.INTENT, Source.FROM_NOTIFICATION,
                "GoogleUpdate.Result.Session", "Intent.Notification");
        validateHistogram(AttributionType.SESSION, Type.INLINE, Source.UNKNOWN_SOURCE,
                "GoogleUpdate.Result.Session", "Inline.Unknown");
        validateHistogram(AttributionType.SESSION, Type.INLINE, Source.FROM_MENU,
                "GoogleUpdate.Result.Session", "Inline.Menu");
        validateHistogram(AttributionType.SESSION, Type.INLINE, Source.FROM_INFOBAR,
                "GoogleUpdate.Result.Session", "Inline.Infobar");
        validateHistogram(AttributionType.SESSION, Type.INLINE, Source.FROM_NOTIFICATION,
                "GoogleUpdate.Result.Session", "Inline.Notification");

        validateHistogram(AttributionType.TIME_WINDOW, Type.UNKNOWN_TYPE, Source.UNKNOWN_SOURCE,
                "GoogleUpdate.Result.TimeWindow", "Unknown.Unknown");
        validateHistogram(AttributionType.TIME_WINDOW, Type.UNKNOWN_TYPE, Source.FROM_MENU,
                "GoogleUpdate.Result.TimeWindow", "Unknown.Menu");
        validateHistogram(AttributionType.TIME_WINDOW, Type.UNKNOWN_TYPE, Source.FROM_INFOBAR,
                "GoogleUpdate.Result.TimeWindow", "Unknown.Infobar");
        validateHistogram(AttributionType.TIME_WINDOW, Type.UNKNOWN_TYPE, Source.FROM_NOTIFICATION,
                "GoogleUpdate.Result.TimeWindow", "Unknown.Notification");
        validateHistogram(AttributionType.TIME_WINDOW, Type.INTENT, Source.UNKNOWN_SOURCE,
                "GoogleUpdate.Result.TimeWindow", "Intent.Unknown");
        validateHistogram(AttributionType.TIME_WINDOW, Type.INTENT, Source.FROM_MENU,
                "GoogleUpdate.Result.TimeWindow", "Intent.Menu");
        validateHistogram(AttributionType.TIME_WINDOW, Type.INTENT, Source.FROM_INFOBAR,
                "GoogleUpdate.Result.TimeWindow", "Intent.Infobar");
        validateHistogram(AttributionType.TIME_WINDOW, Type.INTENT, Source.FROM_NOTIFICATION,
                "GoogleUpdate.Result.TimeWindow", "Intent.Notification");
        validateHistogram(AttributionType.TIME_WINDOW, Type.INLINE, Source.UNKNOWN_SOURCE,
                "GoogleUpdate.Result.TimeWindow", "Inline.Unknown");
        validateHistogram(AttributionType.TIME_WINDOW, Type.INLINE, Source.FROM_MENU,
                "GoogleUpdate.Result.TimeWindow", "Inline.Menu");
        validateHistogram(AttributionType.TIME_WINDOW, Type.INLINE, Source.FROM_INFOBAR,
                "GoogleUpdate.Result.TimeWindow", "Inline.Infobar");
        validateHistogram(AttributionType.TIME_WINDOW, Type.INLINE, Source.FROM_NOTIFICATION,
                "GoogleUpdate.Result.TimeWindow", "Inline.Notification");
    }

    private static void validateHistogram(@AttributionType int attributionType, Type type,
            Source source, String base, String suffix) {
        Tracking info = Tracking.newBuilder().setType(type).setSource(source).build();

        HistogramUtils.recordResultHistogram(attributionType, info, false);
        validateHistogram(base, suffix, false);
        HistogramUtils.recordResultHistogram(attributionType, info, true);
        validateHistogram(base, suffix, true);
    }

    private static void validateHistogram(String base, String suffix, boolean success) {
        CachedMetrics.commitCachedMetrics();
        int value = success ? 1 : 0;
        Assert.assertEquals(1, RecordHistogram.getHistogramValueCountForTesting(base, value));
        Assert.assertEquals(
                1, RecordHistogram.getHistogramValueCountForTesting(base + "." + suffix, value));
        ShadowRecordHistogram.reset();
    }
}
