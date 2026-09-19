// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseRobolectricTestRunner;

/** Tests for MimeUtils class. */
@RunWith(BaseRobolectricTestRunner.class)
public class MimeUtilsUnitTest {
    @Test
    public void testGetFileExtension() {
        Assert.assertEquals("ext", MimeUtils.getFileExtension("", "file.ext"));
        Assert.assertEquals("ext", MimeUtils.getFileExtension("http://file.ext", ""));
        Assert.assertEquals("txt", MimeUtils.getFileExtension("http://file.ext", "file.txt"));
        Assert.assertEquals("txt", MimeUtils.getFileExtension("http://file.ext", "file name.txt"));
    }

    /**
     * Test to make sure {@link DownloadUtils#shouldAutoOpenDownload} returns the right result for
     * varying MIME types and Content-Dispositions.
     */
    @Test
    public void testCanAutoOpenMimeType() {
        // Should not open any download type MIME types.
        Assert.assertFalse(MimeUtils.canAutoOpenMimeType("application/download"));
        Assert.assertFalse(MimeUtils.canAutoOpenMimeType("application/x-download"));
        Assert.assertFalse(MimeUtils.canAutoOpenMimeType("application/octet-stream"));
        Assert.assertTrue(MimeUtils.canAutoOpenMimeType("application/pdf"));
        Assert.assertTrue(MimeUtils.canAutoOpenMimeType("application/x-x509-server-cert"));
        Assert.assertTrue(MimeUtils.canAutoOpenMimeType("application/x-wifi-config"));
        Assert.assertTrue(MimeUtils.canAutoOpenMimeType("application/pkix-cert"));
    }

    /**
     * Test to make sure {@link MimeUtils#remapGenericMimeType} preserves non-generic MIME types
     * (such as application/x-wifi-config) regardless of file extension.
     */
    @Test
    public void testRemapGenericMimeType() {
        Assert.assertEquals(
                "image/jpeg",
                MimeUtils.remapGenericMimeType("application/octet-stream", "http://file.jpg", ""));
        Assert.assertEquals(
                "image/jpeg", MimeUtils.remapGenericMimeType("binary/data", "http://file.jpg", ""));
        Assert.assertEquals(
                "application/x-wifi-config",
                MimeUtils.remapGenericMimeType(
                        "application/x-wifi-config", "http://file.xml", "file.xml"));
    }
}
