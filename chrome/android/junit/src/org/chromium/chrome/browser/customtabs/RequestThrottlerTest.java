// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Context;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Batch;

/** Tests for RequestThrottler. */
@RunWith(BaseRobolectricTestRunner.class)
@Batch(Batch.UNIT_TESTS)
@Config(manifest = Config.NONE)
public class RequestThrottlerTest {
    private static final int UID = 1234;
    private static final int UID2 = 12345;
    private static final String URL = "https://www.google.com";
    private static final String URL2 = "https://www.chromium.org";

    private Context mContext;

    @Before
    public void setUp() {
        mContext = ApplicationProvider.getApplicationContext();
        RequestThrottler.purgeAllEntriesForTesting();
    }

    @After
    public void tearDown() {
        RequestThrottler.purgeAllEntriesForTesting();
    }

    /** Tests that a client starts not banned. */
    @Test
    @SmallTest
    public void testIsInitiallyNotBanned() {
        Assert.assertTrue(RequestThrottler.getForUid(UID).isPrerenderingAllowed());
    }

    /** Tests that a misbehaving client gets banned. */
    @Test
    @SmallTest
    public void testBansUid() {
        RequestThrottler throttler = RequestThrottler.getForUid(UID);
        Assert.assertTrue(throttler.isPrerenderingAllowed());
        for (int i = 0; i < 100; i++) throttler.registerPrerenderRequest(URL);
        Assert.assertFalse(throttler.isPrerenderingAllowed());
    }

    /** Tests that the URL needs to match to avoid getting banned. */
    @Test
    @SmallTest
    public void testBanningMatchesUrls() {
        RequestThrottler throttler = RequestThrottler.getForUid(UID);
        Assert.assertTrue(throttler.isPrerenderingAllowed());
        for (int i = 0; i < 100; i++) {
            throttler.registerPrerenderRequest(URL);
            throttler.registerPrerenderRequest(URL);
            throttler.registerSuccess(URL2);
        }
        Assert.assertFalse(throttler.isPrerenderingAllowed());
    }

    /** Tests that a client can send a lot of requests, as long as they are matched by successes. */
    @Test
    @SmallTest
    public void testDontBanAccurateClients() {
        RequestThrottler throttler = RequestThrottler.getForUid(UID);
        Assert.assertTrue(throttler.isPrerenderingAllowed());
        for (int i = 0; i < 100; i++) {
            throttler.registerPrerenderRequest(URL);
            throttler.registerSuccess(URL);
        }
        Assert.assertTrue(throttler.isPrerenderingAllowed());
    }

    /** Tests that partially accurate clients are not banned. */
    @Test
    @SmallTest
    public void testDontBanPartiallyAccurateClients() {
        RequestThrottler throttler = RequestThrottler.getForUid(UID);
        Assert.assertTrue(throttler.isPrerenderingAllowed());
        for (int j = 0; j < 10; j++) {
            throttler.registerPrerenderRequest(URL);
            throttler.registerPrerenderRequest(URL);
            throttler.registerSuccess(URL2);
            throttler.registerSuccess(URL);
            Assert.assertTrue(throttler.isPrerenderingAllowed());
        }
    }

    /** Tests that banning a UID doesn't ban another one. */
    @Test
    @SmallTest
    public void testThrottlingBanIsByUid() {
        RequestThrottler throttler = RequestThrottler.getForUid(UID);
        Assert.assertTrue(throttler.isPrerenderingAllowed());
        for (int i = 0; i < 100; i++) throttler.registerPrerenderRequest(URL);
        Assert.assertFalse(throttler.isPrerenderingAllowed());
        Assert.assertTrue(RequestThrottler.getForUid(UID2).isPrerenderingAllowed());
    }
}
