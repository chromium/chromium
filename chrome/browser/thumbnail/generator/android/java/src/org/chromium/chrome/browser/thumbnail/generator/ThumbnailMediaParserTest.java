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

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.NativeLibraryTestUtils;
import org.chromium.ui.base.DeviceFormFactor;

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
        MediaParserResult result = new MediaParserResult();

        // The native MediaParser needs to be created on UI thread.
        ThreadUtils.runOnUiThreadBlocking(
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

    /** Verify that the metadata from audio file can be retrieved correctly. */
    @Test
    @LargeTest
    @Feature({"MediaParser"})
    public void testParseAudioMetatadata() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/sfx.mp3";
        MediaParserResult result = parseMediaFile(filePath, "audio/mp3");
        Assert.assertTrue("Failed to parse audio metadata.", result.mediaData != null);
    }

    /** Verify metadata and thumbnail can be retrieved correctly from h264 video file. */
    @Test
    @LargeTest
    @Feature({"MediaParser"})
    @Restriction(DeviceFormFactor.PHONE)
    public void testParseVideoH264() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear.mp4";
        MediaParserResult result = parseMediaFile(filePath, "video/mp4");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    /** Verify metadata and thumbnail can be retrieved correctly from vp8 video file. */
    @Test
    @LargeTest
    @Feature({"MediaParser"})
    public void testParseVideoThumbnailVp8() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear-vp8-webvtt.webm";
        MediaParserResult result = parseMediaFile(filePath, "video/webm");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    /**
     * Verify metadata and thumbnail can be retrieved correctly from vp8 video file with alpha
     * plane.
     */
    @Test
    @LargeTest
    @Feature({"MediaParser"})
    public void testParseVideoThumbnailVp8WithAlphaPlane() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear-vp8a.webm";
        MediaParserResult result = parseMediaFile(filePath, "video/webm");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    /** Verify metadata and thumbnail can be retrieved correctly from vp9 video file. */
    @Test
    @LargeTest
    @Feature({"MediaParser"})
    public void testParseVideoThumbnailVp9() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear-vp9.webm";
        MediaParserResult result = parseMediaFile(filePath, "video/webm");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    /** Verify metadata and thumbnail can be retrieved correctly from av1 video file. */
    @Test
    @LargeTest
    @Feature({"MediaParser"})
    public void testParseVideoThumbnailAv1() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear-av1.mp4";
        MediaParserResult result = parseMediaFile(filePath, "video/mp4");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    /** Verify metadata and thumbnail can be retrieved correctly from h265 video file. */
    @Test
    @LargeTest
    @Feature({"MediaParser"})
    public void testParseVideoThumbnailH265() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/bear-hevc-frag.mp4";
        MediaParserResult result = parseMediaFile(filePath, "video/mp4");
        Assert.assertTrue("Failed to parse video file.", result.mediaData != null);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getWidth() > 0);
        Assert.assertTrue(
                "Failed to retrieve thumbnail.", result.mediaData.thumbnail.getHeight() > 0);
    }

    /** Verify graceful failure on parsing invalid video file. */
    @Test
    @LargeTest
    @Feature({"MediaParser"})
    public void testParseInvalidVideoFile() throws Exception {
        File invalidFile = File.createTempFile("test", "webm");
        MediaParserResult result = parseMediaFile(invalidFile.getAbsolutePath(), "video/webm");
        Assert.assertTrue("Should fail to parse invalid video.", result.mediaData == null);
    }
}
