// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.LaunchSourceType;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for WebappActivity class.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class WebappActivityTest {
    @Test
    @SmallTest
    public void testActivityTypeMatchesLaunchSourceType() throws Exception {
        Assert.assertEquals(LaunchSourceType.OTHER, WebappActivity.ActivityType.OTHER);
        Assert.assertEquals(LaunchSourceType.WEBAPP, WebappActivity.ActivityType.WEBAPP);
        Assert.assertEquals(LaunchSourceType.WEBAPK, WebappActivity.ActivityType.WEBAPK);
    }
}
