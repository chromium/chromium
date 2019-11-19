// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.support.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for {@link WebappAuthenticator}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class WebappAuthenticatorTest {
    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testAuthentication() {
        String url = "http://www.example.org/hello.html";
        byte[] mac = WebappAuthenticator.getMacForUrl(url);
        Assert.assertNotNull(mac);
        Assert.assertTrue(WebappAuthenticator.isUrlValid(url, mac));
        Assert.assertFalse(WebappAuthenticator.isUrlValid(url + "?goats=true", mac));
        mac[4] += (byte) 1;
        Assert.assertFalse(WebappAuthenticator.isUrlValid(url, mac));
    }
}
