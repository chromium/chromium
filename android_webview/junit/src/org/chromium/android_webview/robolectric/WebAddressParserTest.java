// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.robolectric;

import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.android_webview.WebAddressParser;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.Feature;

import java.net.URISyntaxException;

/** Unit tests for WebAddressParser. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class WebAddressParserTest {
    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testGoodInput() throws Throwable {
        Assert.assertEquals("https://www.example.com/", fixupUrl("https://www.example.com"));
        Assert.assertEquals("https://www.example.com/", fixupUrl("https://www.example.com/"));
        Assert.assertEquals("http://www.example.com/", fixupUrl("http://www.example.com/"));
        Assert.assertEquals("http://www.example.com/", fixupUrl("www.example.com"));
        Assert.assertEquals("http://www.example.com/", fixupUrl("www.example.com:80"));
        Assert.assertEquals("https://www.example.com/", fixupUrl("www.example.com:443"));
        Assert.assertEquals("http://192.168.0.1/", fixupUrl("192.168.0.1"));
        Assert.assertEquals("http://[::1]/", fixupUrl("[::1]"));
        Assert.assertEquals("file://www.example.com/", fixupUrl("file://www.example.com/"));
        Assert.assertEquals("http://www.example.com:8000/", fixupUrl("www.example.com:8000"));
        Assert.assertEquals(
                "https://www.example.com:8000/", fixupUrl("https://www.example.com:8000"));
        Assert.assertEquals(
                "http://www.example.com:8000/", fixupUrl("http://www.example.com:8000"));
        Assert.assertEquals(
                "http://www.example.com/somepath/otherpath/thirdpath",
                fixupUrl("http://www.example.com/somepath/otherpath/thirdpath"));
        Assert.assertEquals(
                "http://www.example.com/somepath/otherpath/thirdpath",
                fixupUrl("http://www.example.com/somepath/otherpath/thirdpath#fragment"));
        Assert.assertEquals(
                "http://www.example.com:8000/somepath/otherpath/thirdpath",
                fixupUrl("http://www.example.com:8000/somepath/otherpath/thirdpath#fragment"));
        Assert.assertEquals("http://www.example.com/", fixupUrl("http://www.example.com#fragment"));

        Assert.assertEquals(
                "http://username:password@www.example.com/",
                fixupUrl("http://username:password@www.example.com"));
        Assert.assertEquals(
                "http://username:password@@@@www.example.com/",
                fixupUrl("http://username:password@@@@www.example.com"));
        Assert.assertEquals(
                "http://username@@www.example.com:80000/",
                fixupUrl("username@@www.example.com:80000"));
        Assert.assertEquals(
                "http://username@@www.example.com/somepath/otherpath/thirdpath",
                fixupUrl("http://username@@www.example.com/somepath/otherpath/thirdpath"));
        Assert.assertEquals(
                "http://username:password@@www.example.com/somepath/otherpath/thirdpath",
                fixupUrl("http://username:password@@www.example.com/somepath/otherpath/thirdpath"));

        // crbug.com/1247395
        Assert.assertEquals(
                "http://www.example.com@@example.com/", fixupUrl("www.example.com@@example.com"));
        Assert.assertEquals(
                "http://www.google.com@@localhost:8000/",
                fixupUrl("www.google.com@@localhost:8000"));
        Assert.assertEquals(
                "http://www.google.com^@localhost:8000/",
                fixupUrl("http://www.google.com^@localhost:8000"));

        Assert.assertEquals("http://foo/", fixupUrl("foo"));

        // From Android frameworks/base/core/tests/coretests/src/android/net/WebAddressTest.java
        Assert.assertEquals("http://google.com./b/c/g", fixupUrl("http://google.com./b/c/g"));
        Assert.assertEquals(
                "http://www.myspace.com/?si=1", fixupUrl("http://www.myspace.com?si=1"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testKnownBadInputButWithNoException() throws Throwable {
        // The below two cases are for crbug.com/779887
        Assert.assertEquals("http:///.some.domain", fixupUrl(".some.domain"));
        Assert.assertEquals("http:///.some.domain", fixupUrl("http://.some.domain"));

        Assert.assertEquals("http://www.example.com/-1", fixupUrl("www.example.com:-1"));
        Assert.assertEquals(
                "http:///www.example.com@@example.com:80",
                fixupUrl(":www.example.com@@example.com:80"));
        Assert.assertEquals(
                "http://rtsp//www.example.com/media.mp4",
                fixupUrl("rtsp://www.example.com/media.mp4"));
        Assert.assertEquals("http:///^", fixupUrl("^"));
        Assert.assertEquals("http:///.", fixupUrl("."));
        Assert.assertEquals("http:///", fixupUrl(""));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView", "Privacy"})
    public void testInputWithURISyntaxException() {
        try {
            fixupUrl("www.example.com:1234567890123");
            Assert.fail("Bad port should throw an exception");
        } catch (URISyntaxException e) {
        }

        // Because the ANCHOR regex matches everything, WebAddressParser won't throw exception
        // because of no matching.
    }

    private String fixupUrl(String url) throws URISyntaxException {
        return new WebAddressParser(url).toString();
    }
}
