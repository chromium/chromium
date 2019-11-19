// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webshare;

import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.blink.mojom.SerializedBlob;
import org.chromium.webshare.mojom.SharedFile;

/**
 * Unit tests for {@link SharedFileCollator}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class SharedFileCollatorTest {
    @Test
    @SmallTest
    public void testDissimilar() {
        Assert.assertEquals("*/*", SharedFileCollator.commonMimeType(new SharedFile[0]));
        Assert.assertEquals(
                "*/*", SharedFileCollator.commonMimeType(createFiles("text/plain", "image/jpeg")));
        Assert.assertEquals("*/*",
                SharedFileCollator.commonMimeType(
                        createFiles("video/mpeg", "video/ogg", "text/html")));
    }

    @Test
    @SmallTest
    public void testMalformed() {
        Assert.assertEquals("*/*", SharedFileCollator.commonMimeType(createFiles("invalid")));
        Assert.assertEquals(
                "*/*", SharedFileCollator.commonMimeType(createFiles("text/xml/svg", "text/xml")));
        Assert.assertEquals("*/*",
                SharedFileCollator.commonMimeType(createFiles("image/webp", "image/webp/jpeg")));
    }

    @Test
    @SmallTest
    public void testApplication() {
        Assert.assertEquals("application/*",
                SharedFileCollator.commonMimeType(
                        createFiles("application/rtf", "application/x-bzip2")));
    }

    @Test
    @SmallTest
    public void testAudio() {
        Assert.assertEquals("audio/*",
                SharedFileCollator.commonMimeType(createFiles("audio/mp3", "audio/wav")));
    }

    @Test
    @SmallTest
    public void testImage() {
        Assert.assertEquals(
                "image/jpeg", SharedFileCollator.commonMimeType(createFiles("image/jpeg")));
        Assert.assertEquals("image/gif",
                SharedFileCollator.commonMimeType(
                        createFiles("image/gif", "image/gif", "image/gif")));
        Assert.assertEquals("image/*",
                SharedFileCollator.commonMimeType(createFiles("image/gif", "image/jpeg")));
        Assert.assertEquals("image/*",
                SharedFileCollator.commonMimeType(
                        createFiles("image/gif", "image/gif", "image/jpeg")));
    }

    @Test
    @SmallTest
    public void testText() {
        Assert.assertEquals("text/css", SharedFileCollator.commonMimeType(createFiles("text/css")));
        Assert.assertEquals(
                "text/csv", SharedFileCollator.commonMimeType(createFiles("text/csv", "text/csv")));
        Assert.assertEquals("text/*",
                SharedFileCollator.commonMimeType(
                        createFiles("text/csv", "text/html", "text/csv")));
    }

    @Test
    @SmallTest
    public void testVideo() {
        Assert.assertEquals(
                "video/webm", SharedFileCollator.commonMimeType(createFiles("video/webm")));
        Assert.assertEquals("video/*",
                SharedFileCollator.commonMimeType(createFiles("video/mpeg", "video/webm")));
        Assert.assertEquals("video/*",
                SharedFileCollator.commonMimeType(
                        createFiles("video/mpeg", "video/webm", "video/webm")));
    }

    private static SharedFile[] createFiles(String... mimeTypeList) {
        SharedFile[] result = new SharedFile[mimeTypeList.length];
        for (int i = 0; i < mimeTypeList.length; ++i) {
            SerializedBlob blob = new SerializedBlob();
            blob.contentType = mimeTypeList[i];
            result[i] = new SharedFile();
            result[i].blob = blob;
        }
        return result;
    }
}
