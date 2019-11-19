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

/**
 * Unit tests for {@link ShareServiceImpl}.
 */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class ShareServiceImplTest {
    @Test
    @SmallTest
    public void testExtensionFormatting() {
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("foo/bar.txt"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("foo\\bar\u03C0.txt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("foo\\bar.tx\u03C0t"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("https://example.com/a/b.html"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("foo/bar.txt/"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("foobar.tx\\t"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("hello"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("hellotxt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename(".txt"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("https://example.com/a/.txt"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("/.txt"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename(".."));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename(".hello.txt"));
    }

    @Test
    @SmallTest
    public void testExecutable() {
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("application.apk"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("application.dex"));
        Assert.assertTrue(ShareServiceImpl.isDangerousFilename("application.sh"));
    }

    @Test
    @SmallTest
    public void testContent() {
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("diagram.svg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("greeting.txt"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("movie.mpeg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("photo.jpeg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("recording.wav"));
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("statistics.csv"));
    }

    @Test
    @SmallTest
    public void testCompound() {
        Assert.assertFalse(ShareServiceImpl.isDangerousFilename("powerless.sh.txt"));
    }

    @Test
    @SmallTest
    public void testUnsupportedMime() {
        Assert.assertTrue(ShareServiceImpl.isDangerousMimeType("application/x-shockwave-flash"));
        Assert.assertTrue(ShareServiceImpl.isDangerousMimeType("image/wmf"));
        Assert.assertTrue(ShareServiceImpl.isDangerousMimeType("text/calendar"));
        Assert.assertTrue(ShareServiceImpl.isDangerousMimeType("video/H264"));
    }

    @Test
    @SmallTest
    public void testSupportedMime() {
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("audio/wav"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("image/jpeg"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("image/svg+xml"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("text/csv"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("text/plain"));
        Assert.assertFalse(ShareServiceImpl.isDangerousMimeType("video/mpeg"));
    }
}
