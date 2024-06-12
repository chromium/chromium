// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.privacy_sandbox;

import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;

@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class TrackingProtectionSnackbarLimiterTest {
    @Test
    public void testAllowFirstRequest() {
        TrackingProtectionSnackbarLimiter limiter = new TrackingProtectionSnackbarLimiter();
        assertTrue(limiter.shouldAllowRequest("test.com"));
    }

    @Test
    public void testDisallowSubsequentRequestsWithinLimit() {
        TrackingProtectionSnackbarLimiter limiter = new TrackingProtectionSnackbarLimiter();
        limiter.shouldAllowRequest("test.com");
        assertFalse(limiter.shouldAllowRequest("test.com"));
    }

    @Test
    public void testAllowRequestAfterLimitExpires() {
        TrackingProtectionSnackbarLimiter limiter = new TrackingProtectionSnackbarLimiter();
        long initialTime = System.currentTimeMillis();
        assertTrue(limiter.shouldAllowRequest("test.com", initialTime));
        long timeAfterLimit = initialTime + (11 * 60 * 1000); // 11 minutes later
        assertTrue(limiter.shouldAllowRequest("test.com", timeAfterLimit));
    }

    @Test
    public void testDifferentHostsTreatedIndependently() {
        TrackingProtectionSnackbarLimiter limiter = new TrackingProtectionSnackbarLimiter();
        assertTrue(limiter.shouldAllowRequest("test1.com"));
        assertTrue(limiter.shouldAllowRequest("test2.com"));
        assertFalse(limiter.shouldAllowRequest("test1.com"));
        assertFalse(limiter.shouldAllowRequest("test2.com"));
    }
}
