// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.preferences.website;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;

/**
 * Tests for WebsiteAddress.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class WebsiteAddressTest {
    @Rule
    public final ChromeBrowserTestRule mBrowserTestRule = new ChromeBrowserTestRule();

    @Test
    @SmallTest
    @Feature({"Preferences", "Main"})
    public void testCreate() {
        Assert.assertEquals(null, WebsiteAddress.create(null));
        Assert.assertEquals(null, WebsiteAddress.create(""));

        WebsiteAddress httpAddress = WebsiteAddress.create("http://a.google.com");
        Assert.assertEquals("http://a.google.com", httpAddress.getOrigin());
        Assert.assertEquals("a.google.com", httpAddress.getHost());
        Assert.assertEquals("a.google.com", httpAddress.getTitle());

        WebsiteAddress http8080Address = WebsiteAddress.create("http://a.google.com:8080/");
        Assert.assertEquals("http://a.google.com:8080", http8080Address.getOrigin());
        Assert.assertEquals("a.google.com", http8080Address.getHost());
        Assert.assertEquals("http://a.google.com:8080", http8080Address.getTitle());

        WebsiteAddress httpsAddress = WebsiteAddress.create("https://a.google.com/");
        Assert.assertEquals("https://a.google.com", httpsAddress.getOrigin());
        Assert.assertEquals("a.google.com", httpsAddress.getHost());
        Assert.assertEquals("https://a.google.com", httpsAddress.getTitle());

        WebsiteAddress hostAddress = WebsiteAddress.create("a.google.com");
        Assert.assertEquals("http://a.google.com", hostAddress.getOrigin());
        Assert.assertEquals("a.google.com", hostAddress.getHost());
        Assert.assertEquals("a.google.com", hostAddress.getTitle());

        WebsiteAddress anySubdomainAddress = WebsiteAddress.create("[*.]google.com");
        Assert.assertEquals("http://google.com", anySubdomainAddress.getOrigin());
        Assert.assertEquals("google.com", anySubdomainAddress.getHost());
        Assert.assertEquals("google.com", anySubdomainAddress.getTitle());
    }

    @Test
    @SmallTest
    @Feature({"Preferences"})
    public void testEqualsHashCodeCompareTo() {
        Object[][] testData = {
            { 0, "http://google.com", "http://google.com" },
            { -1, "[*.]google.com", "http://google.com" },
            { -1, "[*.]google.com", "http://a.google.com" },
            { -1, "[*.]a.com", "[*.]b.com" },
            { 0, "[*.]google.com", "google.com" },
            { -1, "[*.]google.com", "a.google.com" },
            { -1, "http://google.com", "http://a.google.com" },
            { -1, "http://a.google.com", "http://a.a.google.com" },
            { -1, "http://a.a.google.com", "http://a.b.google.com" },
            { 1, "http://a.b.google.com", "http://google.com" },
            { -1, "http://google.com", "https://google.com" },
            { -1, "http://google.com", "https://a.google.com" },
            { 1, "https://b.google.com", "https://a.google.com" },
            { -1, "http://a.com", "http://b.com" },
            { -1, "http://a.com", "http://a.b.com" }
        };

        for (int i = 0; i < testData.length; ++i) {
            Object[] testRow = testData[i];

            int compareToResult = (Integer) testRow[0];

            String string1 = (String) testRow[1];
            String string2 = (String) testRow[2];

            WebsiteAddress addr1 = WebsiteAddress.create(string1);
            WebsiteAddress addr2 = WebsiteAddress.create(string2);

            Assert.assertEquals("\"" + string1 + "\" vs \"" + string2 + "\"", compareToResult,
                    Integer.signum(addr1.compareTo(addr2)));

            // Test that swapping arguments gives an opposite result.
            Assert.assertEquals("\"" + string2 + "\" vs \"" + string1 + "\"", -compareToResult,
                    Integer.signum(addr2.compareTo(addr1)));

            if (compareToResult == 0) {
                Assert.assertTrue(addr1.equals(addr2));
                Assert.assertTrue(addr2.equals(addr1));
                Assert.assertEquals(addr1.hashCode(), addr2.hashCode());
            } else {
                Assert.assertFalse(addr1.equals(addr2));
                Assert.assertFalse(addr2.equals(addr1));
                // Note: hash codes could still be the same.
            }
        }
    }
}
