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

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testAuthenticationWithIcon() {
        String url = "http://www.example.org/hello.html";
        String icon = "base64_encoded_icon_data";

        // 1. MAC validation passes with trust (level 2) when both URL and Icon match the MAC.
        byte[] macWithIcon = WebappAuthenticator.getMacForUrlAndIcon(url, icon);
        Assert.assertNotNull(macWithIcon);
        Assert.assertEquals(
                WebappAuthenticator.MAC_TRUSTED,
                WebappAuthenticator.verifyMac(url, icon, macWithIcon));

        // 2. MAC validation passes without trust (level 1) when only URL matches the MAC (legacy
        // shortcut).
        byte[] macUrlOnly = WebappAuthenticator.getMacForUrl(url);
        Assert.assertNotNull(macUrlOnly);
        Assert.assertEquals(
                WebappAuthenticator.MAC_LEGACY,
                WebappAuthenticator.verifyMac(url, icon, macUrlOnly));

        // 3. MAC validation fails (level 0) if the Icon is modified from what was signed.
        Assert.assertEquals(
                WebappAuthenticator.MAC_INVALID,
                WebappAuthenticator.verifyMac(url, icon + "_modified", macWithIcon));

        // MAC validation fails (level 0) if the MAC is invalid.
        byte[] invalidMac = macWithIcon.clone();
        invalidMac[4] += (byte) 1;
        Assert.assertEquals(
                WebappAuthenticator.MAC_INVALID,
                WebappAuthenticator.verifyMac(url, icon, invalidMac));

        // If icon is null, and MAC matches URL, verifyMac returns MAC_LEGACY.
        Assert.assertEquals(
                WebappAuthenticator.MAC_LEGACY,
                WebappAuthenticator.verifyMac(url, null, macUrlOnly));

        // If icon is null, but we passed macWithIcon, it should be invalid.
        Assert.assertEquals(
                WebappAuthenticator.MAC_INVALID,
                WebappAuthenticator.verifyMac(url, null, macWithIcon));
    }

    @Test
    @SmallTest
    @Feature({"Webapps"})
    public void testAuthenticationFieldBoundaries() {
        String url = "https://www.example.org/app/";
        String icon = "PREFIX_SAMPLE_ICON";
        byte[] mac = WebappAuthenticator.getMacForUrlAndIcon(url, icon);
        Assert.assertNotNull(mac);

        // Shift boundary between url and icon (move prefix from icon to URL)
        String shiftedUrl = "https://www.example.org/app/PREFIX_";
        String shiftedIcon = "SAMPLE_ICON";
        Assert.assertEquals(
                WebappAuthenticator.MAC_INVALID,
                WebappAuthenticator.verifyMac(shiftedUrl, shiftedIcon, mac));

        // Shift boundary in the opposite direction (move suffix from URL to icon)
        String shiftedUrl2 = "https://www.example.org/";
        String shiftedIcon2 = "app/PREFIX_SAMPLE_ICON";
        Assert.assertEquals(
                WebappAuthenticator.MAC_INVALID,
                WebappAuthenticator.verifyMac(shiftedUrl2, shiftedIcon2, mac));

        // Empty icon string handling
        byte[] emptyIconMac = WebappAuthenticator.getMacForUrlAndIcon(url, "");
        Assert.assertNotNull(emptyIconMac);
        Assert.assertEquals(
                WebappAuthenticator.MAC_TRUSTED,
                WebappAuthenticator.verifyMac(url, "", emptyIconMac));
        Assert.assertEquals(
                WebappAuthenticator.MAC_INVALID,
                WebappAuthenticator.verifyMac(url, null, emptyIconMac));

        byte[] urlOnlyMac = WebappAuthenticator.getMacForUrl(url);
        Assert.assertNotNull(urlOnlyMac);
        Assert.assertEquals(
                WebappAuthenticator.MAC_LEGACY, WebappAuthenticator.verifyMac(url, "", urlOnlyMac));
    }
}
