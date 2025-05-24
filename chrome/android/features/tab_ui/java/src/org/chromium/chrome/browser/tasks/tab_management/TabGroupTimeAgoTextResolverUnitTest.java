// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.junit.Assert.assertEquals;
import static org.mockito.Mockito.when;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.chrome.browser.tasks.tab_management.TabGroupTimeAgo.TimestampEvent;
import org.chromium.ui.base.TestActivity;

import java.time.Clock;

/** Unit tests for {@link TabGroupTimeAgoTextResolver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupTimeAgoTextResolverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Clock mClock;
    TabGroupTimeAgoTextResolver mResolver;

    @Before
    public void setup() {
        when(mClock.millis()).thenReturn(0L);
        mActivityScenarioRule
                .getScenario()
                .onActivity(
                        (activity) ->
                                mResolver =
                                        new TabGroupTimeAgoTextResolver(
                                                activity.getResources(), mClock));
    }

    private String fromSecondsAgo(long secondsDelta, @TimestampEvent int eventType) {
        return mResolver.resolveTimeAgoText(0 - 1000 * secondsDelta, eventType);
    }

    @Test
    public void testCreateTimeAgoText() {
        assertEquals("Created just now", fromSecondsAgo(-123456789, TimestampEvent.CREATED));
        assertEquals("Created just now", fromSecondsAgo(0, TimestampEvent.CREATED));
        assertEquals("Created just now", fromSecondsAgo(59, TimestampEvent.CREATED));
        assertEquals("Created 1 minute ago", fromSecondsAgo(60, TimestampEvent.CREATED));
        assertEquals("Created 1 minute ago", fromSecondsAgo(119, TimestampEvent.CREATED));
        assertEquals("Created 2 minutes ago", fromSecondsAgo(120, TimestampEvent.CREATED));
        assertEquals("Created 59 minutes ago", fromSecondsAgo(59 * 60, TimestampEvent.CREATED));
        assertEquals("Created 1 hour ago", fromSecondsAgo(60 * 60, TimestampEvent.CREATED));
        assertEquals("Created 2 hours ago", fromSecondsAgo(60 * 60 * 2, TimestampEvent.CREATED));
        assertEquals("Created 1 day ago", fromSecondsAgo(60 * 60 * 24, TimestampEvent.CREATED));
        assertEquals(
                "Created 2 days ago", fromSecondsAgo(60 * 60 * 24 * 2, TimestampEvent.CREATED));
        assertEquals(
                "Created 1 week ago", fromSecondsAgo(60 * 60 * 24 * 7, TimestampEvent.CREATED));
        assertEquals(
                "Created 2 weeks ago",
                fromSecondsAgo(60 * 60 * 24 * 7 * 2, TimestampEvent.CREATED));
        assertEquals(
                "Created 1 month ago", fromSecondsAgo(60 * 60 * 24 * 31, TimestampEvent.CREATED));
        assertEquals(
                "Created 2 months ago",
                fromSecondsAgo(60 * 60 * 24 * 31 * 2, TimestampEvent.CREATED));
        assertEquals(
                "Created 1 year ago", fromSecondsAgo(60 * 60 * 24 * 366, TimestampEvent.CREATED));
        assertEquals(
                "Created 2 years ago",
                fromSecondsAgo(60 * 60 * 24 * 366 * 2, TimestampEvent.CREATED));
        assertEquals(
                "Created 10 years ago",
                fromSecondsAgo(60 * 60 * 24 * 366 * 10, TimestampEvent.CREATED));
    }

    @Test
    public void testUpdateTimeAgoText() {
        assertEquals("Updated just now", fromSecondsAgo(-123456789, TimestampEvent.UPDATED));
        assertEquals("Updated just now", fromSecondsAgo(0, TimestampEvent.UPDATED));
        assertEquals("Updated just now", fromSecondsAgo(59, TimestampEvent.UPDATED));
        assertEquals("Updated 1 minute ago", fromSecondsAgo(60, TimestampEvent.UPDATED));
        assertEquals("Updated 1 minute ago", fromSecondsAgo(119, TimestampEvent.UPDATED));
        assertEquals("Updated 2 minutes ago", fromSecondsAgo(120, TimestampEvent.UPDATED));
        assertEquals("Updated 59 minutes ago", fromSecondsAgo(59 * 60, TimestampEvent.UPDATED));
        assertEquals("Updated 1 hour ago", fromSecondsAgo(60 * 60, TimestampEvent.UPDATED));
        assertEquals("Updated 2 hours ago", fromSecondsAgo(60 * 60 * 2, TimestampEvent.UPDATED));
        assertEquals("Updated 1 day ago", fromSecondsAgo(60 * 60 * 24, TimestampEvent.UPDATED));
        assertEquals(
                "Updated 2 days ago", fromSecondsAgo(60 * 60 * 24 * 2, TimestampEvent.UPDATED));
        assertEquals(
                "Updated 1 week ago", fromSecondsAgo(60 * 60 * 24 * 7, TimestampEvent.UPDATED));
        assertEquals(
                "Updated 2 weeks ago",
                fromSecondsAgo(60 * 60 * 24 * 7 * 2, TimestampEvent.UPDATED));
        assertEquals(
                "Updated 1 month ago", fromSecondsAgo(60 * 60 * 24 * 31, TimestampEvent.UPDATED));
        assertEquals(
                "Updated 2 months ago",
                fromSecondsAgo(60 * 60 * 24 * 31 * 2, TimestampEvent.UPDATED));
        assertEquals(
                "Updated 1 year ago", fromSecondsAgo(60 * 60 * 24 * 366, TimestampEvent.UPDATED));
        assertEquals(
                "Updated 2 years ago",
                fromSecondsAgo(60 * 60 * 24 * 366 * 2, TimestampEvent.UPDATED));
        assertEquals(
                "Updated 10 years ago",
                fromSecondsAgo(60 * 60 * 24 * 366 * 10, TimestampEvent.UPDATED));
    }
}
