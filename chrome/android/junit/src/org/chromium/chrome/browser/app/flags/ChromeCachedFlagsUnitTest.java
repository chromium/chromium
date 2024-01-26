// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.flags;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.BaseFlagTestRule;

/** Unit Tests for {@link ChromeCachedFlags}. */
@RunWith(BaseRobolectricTestRunner.class)
public class ChromeCachedFlagsUnitTest {
    @Rule public final BaseFlagTestRule baseFlagTestRule = new BaseFlagTestRule();

    @Test
    public void testGetLastCachedMinimalBrowserFlagsTimeMillis() {
        // Initial time is 0.
        assertEquals(0, ChromeCachedFlags.getLastCachedMinimalBrowserFlagsTimeMillis());
        final long timeMillis = System.currentTimeMillis();
        ChromeCachedFlags.cacheMinimalBrowserFlagsTimeFromNativeTime();
        assertTrue(ChromeCachedFlags.getLastCachedMinimalBrowserFlagsTimeMillis() >= timeMillis);
    }
}
