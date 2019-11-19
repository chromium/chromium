// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;
import android.text.format.DateUtils;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.ChromeFeatureList;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;

import java.util.HashMap;

/**
 * Tests of {@link DownloadUtils}.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
public class DownloadUtilsTest {
    private static final String OFFLINE_ITEM_TITLE = "Some Web Page Title.mhtml";
    private static final String OFFLINE_ITEM_DESCRIPTION = "Our web page";
    private static final String FILE_PATH = "/fake_sd_card/Download/Some Web Page Title.mhtml";
    private static final String TEMP_FILE_PATH = "/offline-cache/1234.mhtml";
    private static final String CONTENT_URI =
            "content://org.chromium.chrome.FileProvider/offline-cache/1234.mhtml";
    private static final String MULTIPART_RELATED = "multipart/related";
    private static final String ITEM_ID = "42";

    @Before
    public void setUp() {
        RecordHistogram.setDisabledForTests(true);

        HashMap<String, Boolean> features = new HashMap<String, Boolean>();
        features.put(ChromeFeatureList.DOWNLOAD_FILE_PROVIDER, false);
        ChromeFeatureList.setTestFeatures(features);
    }

    @After
    public void tearDown() {
        RecordHistogram.setDisabledForTests(false);
    }

    /**
     * Test {@link DownloadUtils#getAbbrieviatedFileName()} method.
     */
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetAbbrieviatedFileName() {
        Assert.assertEquals("123.pdf", DownloadUtils.getAbbreviatedFileName("123.pdf", 10));
        Assert.assertEquals(
                "1" + DownloadUtils.ELLIPSIS, DownloadUtils.getAbbreviatedFileName("123.pdf", 1));
        Assert.assertEquals(
                "12" + DownloadUtils.ELLIPSIS, DownloadUtils.getAbbreviatedFileName("1234567", 2));
        Assert.assertEquals("123" + DownloadUtils.ELLIPSIS + ".pdf",
                DownloadUtils.getAbbreviatedFileName("1234567.pdf", 7));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testFormatRemainingTime() {
        final Context context = InstrumentationRegistry.getTargetContext();
        Assert.assertEquals("0 secs left", DownloadUtils.formatRemainingTime(context, 0));
        Assert.assertEquals("1 sec left",
                DownloadUtils.formatRemainingTime(context, DateUtils.SECOND_IN_MILLIS));
        Assert.assertEquals("1 min left",
                DownloadUtils.formatRemainingTime(context, 60 * DateUtils.SECOND_IN_MILLIS));
        Assert.assertEquals("2 mins left",
                DownloadUtils.formatRemainingTime(context, 149 * DateUtils.SECOND_IN_MILLIS));
        Assert.assertEquals("3 mins left",
                DownloadUtils.formatRemainingTime(context, 150 * DateUtils.SECOND_IN_MILLIS));
        Assert.assertEquals("1 hour left",
                DownloadUtils.formatRemainingTime(context, 60 * DateUtils.MINUTE_IN_MILLIS));
        Assert.assertEquals("2 hours left",
                DownloadUtils.formatRemainingTime(context, 149 * DateUtils.MINUTE_IN_MILLIS));
        Assert.assertEquals("3 hours left",
                DownloadUtils.formatRemainingTime(context, 150 * DateUtils.MINUTE_IN_MILLIS));
        Assert.assertEquals("1 day left",
                DownloadUtils.formatRemainingTime(context, 24 * DateUtils.HOUR_IN_MILLIS));
        Assert.assertEquals("2 days left",
                DownloadUtils.formatRemainingTime(context, 59 * DateUtils.HOUR_IN_MILLIS));
        Assert.assertEquals("3 days left",
                DownloadUtils.formatRemainingTime(context, 60 * DateUtils.HOUR_IN_MILLIS));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testFormatBytesReceived() {
        final Context context = InstrumentationRegistry.getTargetContext();
        Assert.assertEquals("Downloaded 0.0 KB",
                DownloadUtils.getStringForBytes(
                        context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 0));
        Assert.assertEquals("Downloaded 0.5 KB",
                DownloadUtils.getStringForBytes(
                        context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 512));
        Assert.assertEquals("Downloaded 1.0 KB",
                DownloadUtils.getStringForBytes(
                        context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 1024));
        Assert.assertEquals("Downloaded 1.0 MB",
                DownloadUtils.getStringForBytes(
                        context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 1024 * 1024));
        Assert.assertEquals("Downloaded 1.0 GB",
                DownloadUtils.getStringForBytes(
                        context, DownloadUtils.BYTES_DOWNLOADED_STRINGS, 1024 * 1024 * 1024));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testFormatRemainingFiles() {
        final Context context = InstrumentationRegistry.getTargetContext();
        Progress progress = new Progress(3, Long.valueOf(5), OfflineItemProgressUnit.FILES);
        Assert.assertEquals(60, progress.getPercentage());
        Assert.assertEquals("2 files left", DownloadUtils.formatRemainingFiles(context, progress));
        progress = new Progress(4, Long.valueOf(5), OfflineItemProgressUnit.FILES);
        Assert.assertEquals(80, progress.getPercentage());
        Assert.assertEquals("1 file left", DownloadUtils.formatRemainingFiles(context, progress));
        progress = new Progress(5, Long.valueOf(5), OfflineItemProgressUnit.FILES);
        Assert.assertEquals(100, progress.getPercentage());
        Assert.assertEquals("0 files left", DownloadUtils.formatRemainingFiles(context, progress));
    }
}
