// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.os.Build;
import android.support.test.filters.LargeTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Feature;
import org.chromium.base.test.util.MinAndroidSdkLevel;
import org.chromium.base.test.util.Restriction;
import org.chromium.base.test.util.UrlUtils;
import org.chromium.chrome.test.ChromeBrowserTestRule;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.test.util.UiRestriction;

import java.io.File;

/**
 * Tests to verify DownloadMediaParser, which retrieves media metadata and thumbnails.
 *
 * Most of the work is done in utility process and GPU process.
 *
 * All download media parser usage must be called on UI thread in this test to get message loop and
 * threading contexts in native.
 *
 * Because each media parser call may perform multiple process and thread hops, it can be slow.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DownloadMediaParserTest {
    private static final long MAX_MEDIA_PARSER_POLL_TIME_MS = 10000;
    private static final long MEDIA_PARSER_POLL_INTERVAL_MS = 1000;

    @Rule
    public ChromeBrowserTestRule mTestRule = new ChromeBrowserTestRule();

    /**
     * Wraps result from download media parser.
     */
    public static class MediaParserResult {
        public boolean done;
        public DownloadMediaData mediaData;
    }

    @Before
    public void setUp() {
        mTestRule.loadNativeLibraryAndInitBrowserProcess();
    }

    private MediaParserResult parseMediaFile(String filePath, String mimeType) {
        File mediaFile = new File(filePath);
        Assert.assertTrue(mediaFile.exists());
        boolean done = false;
        MediaParserResult result = new MediaParserResult();

        // The native DownloadMediaParser needs to be created on UI thread.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadMediaParserBridge parser = new DownloadMediaParserBridge(
                    mimeType, filePath, (DownloadMediaData mediaData) -> {
                        result.mediaData = mediaData;
                        result.done = true;
                    });
            parser.start();
        });

        CriteriaHelper.pollUiThread(new Criteria() {
            @Override
            public boolean isSatisfied() {
                return result.done;
            }
        }, MAX_MEDIA_PARSER_POLL_TIME_MS, MEDIA_PARSER_POLL_INTERVAL_MS);
        return result;
    }

    @Test
    @LargeTest
    @Feature({"Download"})
    /**
     * Verify that the metadata from audio file can be retrieved correctly.
     * @throws InterruptedException
     */
    public void testParseAudioMetatadata() {
        String filePath = UrlUtils.getIsolatedTestRoot() + "/media/test/data/sfx.mp3";
        MediaParserResult result = parseMediaFile(filePath, "audio/mp3");
        Assert.assertTrue("Failed to parse audio metadata.", result.mediaData != null);
    }

    @Test
    @LargeTest
    @Feature({"Download"})
    @MinAndroidSdkLevel(Build.VERSION_CODES.LOLLIPOP)
    @Restriction(UiRestriction.RESTRICTION_TYPE_PHONE)
    /**
     * Verify metadata and thumbnail can be retrieved correctly from h264 video file.
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
    @Feature({"Download"})
    /**
     * Verify metadata and thumbnail can be retrieved correctly from vp8 video file.
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
    @Feature({"Download"})
    /**
     * Verify metadata and thumbnail can be retrieved correctly from vp8 video file with alpha
     * plane.
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
    @Feature({"Download"})
    /**
     * Verify graceful failure on parsing invalid video file.
     * @throws InterruptedException
     */
    public void testParseInvalidVideoFile() throws Exception {
        File invalidFile = File.createTempFile("test", "webm");
        MediaParserResult result = parseMediaFile(invalidFile.getAbsolutePath(), "video/webm");
        Assert.assertTrue("Should fail to parse invalid video.", result.mediaData == null);
    }
}
