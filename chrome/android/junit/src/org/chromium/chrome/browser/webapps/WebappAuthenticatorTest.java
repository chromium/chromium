// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

/** Tests for {@link WebappAuthenticator}. */
@RunWith(BaseRobolectricTestRunner.class)
public class WebappAuthenticatorTest {
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
