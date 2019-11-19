// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.AndroidProtocolHandler;
import org.chromium.base.FileUtils;
import org.chromium.base.test.util.Feature;

import java.io.IOException;
import java.io.InputStream;

/**
 * Test AndroidProtocolHandler.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class AndroidProtocolHandlerTest {
    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOpenNullUrl() {
        Assert.assertNull(AndroidProtocolHandler.open(null));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOpenEmptyUrl() {
        Assert.assertNull(AndroidProtocolHandler.open(""));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOpenMalformedUrl() {
        Assert.assertNull(AndroidProtocolHandler.open("abcdefg"));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testOpenPathlessUrl() {
        // These URLs are interesting because android.net.Uri parses them unintuitively:
        // Uri.getPath() returns "/" but Uri.getLastPathSegment() returns null.
        Assert.assertNull(AndroidProtocolHandler.open("file:///"));
        Assert.assertNull(AndroidProtocolHandler.open("content:///"));
    }

    // star.svg and star.svgz contain the same data. AndroidProtocolHandler should decompress the
    // svgz automatically. Load both from assets and assert that they're equal.
    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSvgzAsset() throws IOException {
        InputStream svgStream = null;
        InputStream svgzStream = null;
        try {
            svgStream = assertOpen("file:///android_asset/star.svg");
            byte[] expectedData = FileUtils.readStream(svgStream);

            svgzStream = assertOpen("file:///android_asset/star.svgz");
            byte[] actualData = FileUtils.readStream(svgzStream);

            Assert.assertArrayEquals(
                    "Decompressed star.svgz doesn't match star.svg", expectedData, actualData);
        } finally {
            if (svgStream != null) svgStream.close();
            if (svgzStream != null) svgzStream.close();
        }
    }

    private InputStream assertOpen(String url) {
        InputStream stream = AndroidProtocolHandler.open(url);
        Assert.assertNotNull("Failed top open \"" + url + "\"", stream);
        return stream;
    }
}
