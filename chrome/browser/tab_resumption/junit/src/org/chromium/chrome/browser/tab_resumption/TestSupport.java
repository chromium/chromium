// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.graphics.Bitmap;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTab;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSessionTabWithLastActiveTime;
import org.chromium.url.JUnitTestGURLs;

public class TestSupport {
    static final long BASE_TIME_MS = 1705000000000L; // 2024-01-11, 14:06:40 EST.

    // Injected "current time" for each test, set at 1 day after `BASE_TIME_MS`.
    static final long CURRENT_TIME_MS = makeTimestamp(24, 0, 0);

    static final ForeignSessionTab TAB1 =
            new ForeignSessionTabWithLastActiveTime(
                    JUnitTestGURLs.BLUE_1,
                    "Blue 1",
                    makeTimestamp(3, 0, 0),
                    101,
                    makeTimestamp(3, 0, 0));
    // This one is stale.
    static final ForeignSessionTab TAB2 =
            new ForeignSessionTabWithLastActiveTime(
                    JUnitTestGURLs.GOOGLE_URL_DOG,
                    "Google Dog",
                    makeTimestamp(0, 0, -1),
                    102,
                    makeTimestamp(0, 0, -1));
    static final ForeignSessionTab TAB3 =
            new ForeignSessionTabWithLastActiveTime(
                    JUnitTestGURLs.CHROME_ABOUT,
                    "About",
                    makeTimestamp(0, 0, -1), // timestamp != lastUpdatedTime.
                    103,
                    makeTimestamp(7, 0, 0));
    static final ForeignSessionTab TAB4 =
            new ForeignSessionTabWithLastActiveTime(
                    JUnitTestGURLs.URL_1,
                    "One",
                    makeTimestamp(0, 30, 0),
                    104,
                    makeTimestamp(0, 30, 0));
    static final ForeignSessionTab TAB5 =
            new ForeignSessionTabWithLastActiveTime(
                    JUnitTestGURLs.MAPS_URL,
                    "Maps",
                    makeTimestamp(4, 0, 0),
                    105,
                    makeTimestamp(4, 0, 0));
    // This one is the most recent.
    static final ForeignSessionTab TAB6 =
            new ForeignSessionTabWithLastActiveTime(
                    JUnitTestGURLs.INITIAL_URL,
                    "Initial",
                    makeTimestamp(2, 0, 0), // timestamp != lastUpdatedTime.
                    106,
                    makeTimestamp(8, 0, 0));
    static final ForeignSessionTab TAB7 =
            new ForeignSessionTabWithLastActiveTime(
                    JUnitTestGURLs.HTTP_URL,
                    "Old HTTP",
                    makeTimestamp(3, 0, 0),
                    107,
                    makeTimestamp(3, 0, 0));

    /** Makes a test bitmap with specified dimensions. */
    static Bitmap makeBitmap(int width, int height) {
        return Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
    }

    /**
     * Makes a test timestamp in ms since the epoch relative to `BASE_TIME_MS`.
     *
     * @param hours Relative hours, may be negative.
     * @param minutes Relative minutes, may be negative.
     * @param seconds Relative seconds, may be negative.
     */
    static long makeTimestamp(int hours, int minutes, int seconds) {
        return BASE_TIME_MS + ((hours * 60L + minutes) * 60L + seconds) * 1000L;
    }
}
