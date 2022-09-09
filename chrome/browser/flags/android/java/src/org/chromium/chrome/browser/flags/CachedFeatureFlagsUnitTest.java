// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.flags;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertTrue;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/**
 * Unit Tests for {@link CachedFeatureFlags}.
 */
@RunWith(BaseRobolectricTestRunner.class)
public class CachedFeatureFlagsUnitTest {
    @Rule
    public final BaseFlagTestRule baseFlagTestRule = new BaseFlagTestRule();

    @Test
    public void testGetLastCachedMinimalBrowserFlagsTimeMillis() {
        // Initial time is 0.
        assertEquals(0, CachedFeatureFlags.getLastCachedMinimalBrowserFlagsTimeMillis());
        final long timeMillis = System.currentTimeMillis();
        CachedFeatureFlags.cacheMinimalBrowserFlagsTimeFromNativeTime();
        assertTrue(CachedFeatureFlags.getLastCachedMinimalBrowserFlagsTimeMillis() >= timeMillis);
    }
}
