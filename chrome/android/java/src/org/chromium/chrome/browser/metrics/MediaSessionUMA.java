// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import androidx.annotation.IntDef;

import org.chromium.base.metrics.RecordHistogram;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Centralizes UMA data collection for Android-specific MediaSession features.
 */
public class MediaSessionUMA {
    // MediaSessionAction defined in tools/metrics/histograms/histograms.xml.
    @IntDef({MediaSessionActionSource.MEDIA_NOTIFICATION, MediaSessionActionSource.MEDIA_SESSION,
            MediaSessionActionSource.HEADSET_UNPLUG})
    @Retention(RetentionPolicy.SOURCE)
    public @interface MediaSessionActionSource {
        int MEDIA_NOTIFICATION = 0;
        int MEDIA_SESSION = 1;
        int HEADSET_UNPLUG = 2;

        int NUM_ENTRIES = 3;
    }

    public static void recordPlay(@MediaSessionActionSource int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Media.Session.Play", action, MediaSessionActionSource.NUM_ENTRIES);
    }

    public static void recordPause(@MediaSessionActionSource int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Media.Session.Pause", action, MediaSessionActionSource.NUM_ENTRIES);
    }

    public static void recordStop(@MediaSessionActionSource int action) {
        RecordHistogram.recordEnumeratedHistogram(
                "Media.Session.Stop", action, MediaSessionActionSource.NUM_ENTRIES);
    }
}
