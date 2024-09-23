// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.mockito.Mockito.when;

import androidx.test.ext.junit.rules.ActivityScenarioRule;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mock;
import org.mockito.junit.MockitoJUnit;
import org.mockito.junit.MockitoRule;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.ui.base.TestActivity;

import java.time.Clock;

/** Unit tests for {@link TabGroupTimeAgoResolver}. */
@RunWith(BaseRobolectricTestRunner.class)
public class TabGroupTimeAgoResolverUnitTest {
    @Rule public MockitoRule mMockitoRule = MockitoJUnit.rule();

    @Rule
    public ActivityScenarioRule<TestActivity> mActivityScenarioRule =
            new ActivityScenarioRule<>(TestActivity.class);

    @Mock private Clock mClock;

    private TabGroupTimeAgoResolver mResolver;

    @Before
    public void setup() {
        when(mClock.millis()).thenReturn(0L);
        mActivityScenarioRule.getScenario().onActivity(this::onActivity);
    }

    private void onActivity(TestActivity activity) {
        mResolver = new TabGroupTimeAgoResolver(activity.getResources(), mClock);
    }

    private String fromSecondsAgo(long secondsDelta) {
        return mResolver.resolveTimeAgoText(0 - 1000 * secondsDelta);
    }

    @Test
    public void testResolveTimeAgoText() {
        Assert.assertEquals("Created just now", fromSecondsAgo(-123456789));
        Assert.assertEquals("Created just now", fromSecondsAgo(0));
        Assert.assertEquals("Created just now", fromSecondsAgo(59));
        Assert.assertEquals("Created 1 minute ago", fromSecondsAgo(60));
        Assert.assertEquals("Created 1 minute ago", fromSecondsAgo(119));
        Assert.assertEquals("Created 2 minutes ago", fromSecondsAgo(120));
        Assert.assertEquals("Created 59 minutes ago", fromSecondsAgo(59 * 60));
        Assert.assertEquals("Created 1 hour ago", fromSecondsAgo(60 * 60));
        Assert.assertEquals("Created 2 hours ago", fromSecondsAgo(60 * 60 * 2));
        Assert.assertEquals("Created 1 day ago", fromSecondsAgo(60 * 60 * 24));
        Assert.assertEquals("Created 2 days ago", fromSecondsAgo(60 * 60 * 24 * 2));
        Assert.assertEquals("Created 1 week ago", fromSecondsAgo(60 * 60 * 24 * 7));
        Assert.assertEquals("Created 2 weeks ago", fromSecondsAgo(60 * 60 * 24 * 7 * 2));
        Assert.assertEquals("Created 1 month ago", fromSecondsAgo(60 * 60 * 24 * 31));
        Assert.assertEquals("Created 2 months ago", fromSecondsAgo(60 * 60 * 24 * 31 * 2));
        Assert.assertEquals("Created 1 year ago", fromSecondsAgo(60 * 60 * 24 * 366));
        Assert.assertEquals("Created 2 years ago", fromSecondsAgo(60 * 60 * 24 * 366 * 2));
        Assert.assertEquals("Created 10 years ago", fromSecondsAgo(60 * 60 * 24 * 366 * 10));
    }
}
