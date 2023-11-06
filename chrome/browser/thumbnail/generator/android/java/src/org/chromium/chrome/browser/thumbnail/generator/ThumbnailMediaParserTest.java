// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.thumbnail.generator;

import androidx.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;

/**
 * Tests to verify ThumbnailMediaParser, which retrieves media metadata and thumbnails.
 *
 * <p>Most of the work is done in utility process and GPU process.
 *
 * <p>All media parser usage must be called on UI thread in this test to get message loop and
 * threading contexts in native.
 *
 * <p>Because each media parser call may perform multiple process and thread hops, it can be slow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ThumbnailMediaParserTest {
    @Rule public ChromeBrowserTestRule mTestRule = new ChromeBrowserTestRule();

    /** Wraps result from media parser. */
    public static class MediaParserResult {
        public boolean done;
        public ThumbnailMediaData mediaData;
    }

    @Before
    public void setUp() {
        NativeLibraryTestUtils.loadNativeLibraryAndInitBrowserProcess();
    }

    private MediaParserResult parseMediaFile(String filePath, String mimeType) {
        File mediaFile = new File(filePath);
        Assert.assertTrue(mediaFile.exists());
        boolean done = false;
        MediaParserResult result = new MediaParserResult();

        // The native MediaParser needs to be created on UI thread.
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ThumbnailMediaParserBridge.parse(
                            mimeType,
                            filePath,
                            (ThumbnailMediaData mediaData) -> {
                                result.mediaData = mediaData;
                                result.done = true;
                            });
                });

        CriteriaHelper.pollUiThread(
                () -> result.done, 10000, CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        return result;
    }

    @Test
    @LargeTest
    @Feature({"MediaParser"})
    /**
     * Verify that the metadata from audio file can be retrieved correctly.
     *
     * @throws InterruptedException
     */
    public void testParseAudioMetatadata() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/sfx.mp3";
        MediaParserResult result = parseMediaFile(filePath, "audio/mp3");
        Assert.assertTrue("Failed to parse audio metadata.", result.mediaData != null);
    }

    @Test
    @LargeTest
    @Feature({"MediaParser"})
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    /**
     * Verify metadata and thumbnail can be retrieved correctly from h264 video file.
     *
     * @throws InterruptedException
     */
    public void testParseVideoH264() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear.mp4";
        MediaParserResult result = parseMediaFile(filePath, "video/mp4");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    @Test
    @LargeTest
    @Feature({"MediaParser"})
    /**
     * Verify metadata and thumbnail can be retrieved correctly from vp8 video file.
     *
     * @throws InterruptedException
     */
    public void testParseVideoThumbnailVp8() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear-vp8-webvtt.webm";
        MediaParserResult result = parseMediaFile(filePath, "video/webm");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    @Test
    @LargeTest
    @Feature({"MediaParser"})
    /**
     * Verify metadata and thumbnail can be retrieved correctly from vp8 video file with alpha
     * plane.
     *
     * @throws InterruptedException
     */
    public void testParseVideoThumbnailVp8WithAlphaPlane() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear-vp8a.webm";
        MediaParserResult result = parseMediaFile(filePath, "video/webm");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    @Test
    @LargeTest
    @Feature({"MediaParser"})
    /**
     * Verify graceful failure on parsing invalid video file.
     *
     * @throws InterruptedException
     */
    public void testParseInvalidVideoFile() throws Exception {
        File invalidFile = File.createTempFile("test", "webm");
        MediaParserResult result = parseMediaFile(invalidFile.getAbsolutePath(), "video/webm");
        Assert.assertTrue("Should fail to parse invalid video.", result.mediaData == null);
    }
}
