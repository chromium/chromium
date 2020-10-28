// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.util.Pair;

import org.chromium.base.SysUtils;
import org.chromium.base.metrics.JSONVerbosityLevel;
import org.chromium.base.metrics.StatisticsRecorderAndroid;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.browser_ui.util.ConversionUtils;

/** Grabs feedback about the UMA histograms. */
// TODO(dtrainor): Make this class protected and HISTOGRAMS_KEY private once grabbing specific log
// sources is no longer required.
public class HistogramFeedbackSource implements FeedbackSource {
    public static final String HISTOGRAMS_KEY = "histograms";

    // Minimum physical memory (in KB) required on the device to get a dump of the buckets.
    // Currently, it is required that the physical memory be more than 2 GB.
    private static final int MIN_PHYSICAL_MEMORY_KB =
            2 * ConversionUtils.KILOBYTES_PER_GIGABYTE + 1;

    private final boolean mIsOffTheRecord;

    HistogramFeedbackSource(Profile profile) {
        mIsOffTheRecord = profile.isOffTheRecord();
    }

    @Override
    public Pair<String, String> getLogs() {
        if (mIsOffTheRecord) return null;
        int jsonVerbosityLevel = JSONVerbosityLevel.JSON_VERBOSITY_LEVEL_FULL;

        if (SysUtils.isLowEndDevice()
                || SysUtils.amountOfPhysicalMemoryKB() < MIN_PHYSICAL_MEMORY_KB
                || SysUtils.isCurrentlyLowMemory()) {
            jsonVerbosityLevel = JSONVerbosityLevel.JSON_VERBOSITY_LEVEL_OMIT_BUCKETS;
        }
        return Pair.create(HISTOGRAMS_KEY, StatisticsRecorderAndroid.toJson(jsonVerbosityLevel));
    }
}