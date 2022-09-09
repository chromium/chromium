// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks;

import static org.junit.Assert.assertEquals;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Unit tests for JourneyManager. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public final class EngagementTimeUtilTest {
    private static final long TEST_ELAPSED_MS = 5000L;

    private MockEngagementTimeUtil mEngagementTimeUtil;

    @Before
    public void setUp() {
        mEngagementTimeUtil = new MockEngagementTimeUtil();
    }

    @Test
    public void timeSinceLastEngagement_shouldReportElapsedTimeSinceLastEngagement() {
        long currentTimeMillis = System.currentTimeMillis();
        mEngagementTimeUtil.setCurrentTime(currentTimeMillis);
        assertEquals(TEST_ELAPSED_MS,
                mEngagementTimeUtil.timeSinceLastEngagement(currentTimeMillis - TEST_ELAPSED_MS));
    }

    @Test
    public void timeSinceLastEngagement_shouldReportInvalidIfNegative() {
        long currentTimeMillis = System.currentTimeMillis();
        mEngagementTimeUtil.setCurrentTime(currentTimeMillis);
        assertEquals(-1,
                mEngagementTimeUtil.timeSinceLastEngagement(
                        System.currentTimeMillis() + TEST_ELAPSED_MS));
    }

    @Test
    public void timeSinceLastEngagement_shouldReportElapsedTimeBetweenTimestamps() {
        long currentTimeMillis = System.currentTimeMillis();
        mEngagementTimeUtil.setCurrentTime(currentTimeMillis);
        assertEquals(TEST_ELAPSED_MS,
                mEngagementTimeUtil.timeSinceLastEngagement(
                        currentTimeMillis - (2L * TEST_ELAPSED_MS),
                        currentTimeMillis - TEST_ELAPSED_MS));
    }

    private static class MockEngagementTimeUtil extends EngagementTimeUtil {
        private long mCurrentTime = System.currentTimeMillis();

        public void setCurrentTime(long currentTime) {
            mCurrentTime = currentTime;
        }

        // EngagementTimeUtil implementation.
        @Override
        public long currentTime() {
            return mCurrentTime;
        }
    }
}
