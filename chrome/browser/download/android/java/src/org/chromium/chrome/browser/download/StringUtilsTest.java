// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.text.format.DateUtils;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.OfflineItem.Progress;
import org.chromium.components.offline_items_collection.OfflineItemProgressUnit;

/** Tests of {@link StringUtils}. */
@RunWith(ChromeJUnit4ClassRunner.class)
@Batch(Batch.UNIT_TESTS)
public class StringUtilsTest {
    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetProgressTextForUi() {
        Assert.assertEquals(
                "Downloading…", StringUtils.getProgressTextForUi(ProgressBuilder.indeterminate()));
        Assert.assertEquals(
                "Downloading…",
                StringUtils.getProgressTextForUi(ProgressBuilder.percentage(10, null)));
        Assert.assertEquals(
                "50%", StringUtils.getProgressTextForUi(ProgressBuilder.percentage(50, 100L)));
        Assert.assertEquals(
                "Downloading…", StringUtils.getProgressTextForUi(ProgressBuilder.bytes(0, null)));
        Assert.assertEquals(
                "1.00 KB / ?", StringUtils.getProgressTextForUi(ProgressBuilder.bytes(1000, null)));
        Assert.assertEquals(
                "0.51 KB / ?", StringUtils.getProgressTextForUi(ProgressBuilder.bytes(512, null)));
        Assert.assertEquals(
                "1.00 MB / ?",
                StringUtils.getProgressTextForUi(ProgressBuilder.bytes(1_000_000, null)));
        Assert.assertEquals(
                "1.00 GB / ?",
                StringUtils.getProgressTextForUi(ProgressBuilder.bytes(1_000_000_000L, null)));
        Assert.assertEquals(
                "1.00 KB / 2.00 KB",
                StringUtils.getProgressTextForUi(ProgressBuilder.bytes(1000, 2000L)));
        Assert.assertEquals(
                "Downloading…", StringUtils.getProgressTextForUi(ProgressBuilder.files(0, null)));
        Assert.assertEquals(
                "1 file downloaded",
                StringUtils.getProgressTextForUi(ProgressBuilder.files(1, null)));
        Assert.assertEquals(
                "2 files downloaded",
                StringUtils.getProgressTextForUi(ProgressBuilder.files(2, null)));
        Assert.assertEquals(
                "0 files left", StringUtils.getProgressTextForUi(ProgressBuilder.files(3, 3L)));
        Assert.assertEquals(
                "1 file left", StringUtils.getProgressTextForUi(ProgressBuilder.files(2, 3L)));
        Assert.assertEquals(
                "2 files left", StringUtils.getProgressTextForUi(ProgressBuilder.files(1, 3L)));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testFormatRemainingTime() {
        final Context context = ApplicationProvider.getApplicationContext();
        Assert.assertEquals("0 secs left", StringUtils.timeLeftForUi(context, 0));
        Assert.assertEquals(
                "1 sec left", StringUtils.timeLeftForUi(context, DateUtils.SECOND_IN_MILLIS));
        Assert.assertEquals(
                "1 min left", StringUtils.timeLeftForUi(context, 60 * DateUtils.SECOND_IN_MILLIS));
        Assert.assertEquals(
                "2 mins left",
                StringUtils.timeLeftForUi(context, 149 * DateUtils.SECOND_IN_MILLIS));
        Assert.assertEquals(
                "3 mins left",
                StringUtils.timeLeftForUi(context, 150 * DateUtils.SECOND_IN_MILLIS));
        Assert.assertEquals(
                "1 hour left", StringUtils.timeLeftForUi(context, 60 * DateUtils.MINUTE_IN_MILLIS));
        Assert.assertEquals(
                "2 hours left",
                StringUtils.timeLeftForUi(context, 149 * DateUtils.MINUTE_IN_MILLIS));
        Assert.assertEquals(
                "3 hours left",
                StringUtils.timeLeftForUi(context, 150 * DateUtils.MINUTE_IN_MILLIS));
        Assert.assertEquals(
                "1 day left", StringUtils.timeLeftForUi(context, 24 * DateUtils.HOUR_IN_MILLIS));
        Assert.assertEquals(
                "2 days left", StringUtils.timeLeftForUi(context, 59 * DateUtils.HOUR_IN_MILLIS));
        Assert.assertEquals(
                "3 days left", StringUtils.timeLeftForUi(context, 60 * DateUtils.HOUR_IN_MILLIS));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetAvailableBytesForUi() {
        final Context context = ApplicationProvider.getApplicationContext();
        Assert.assertEquals("0.00 KB available", StringUtils.getAvailableBytesForUi(context, 0));
        Assert.assertEquals("0.51 KB available", StringUtils.getAvailableBytesForUi(context, 512));
        Assert.assertEquals("1.00 KB available", StringUtils.getAvailableBytesForUi(context, 1000));
        Assert.assertEquals(
                "1.00 MB available", StringUtils.getAvailableBytesForUi(context, 1_000_000));
        Assert.assertEquals(
                "1.00 GB available", StringUtils.getAvailableBytesForUi(context, 1_000_000_000L));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testDownloadUtilsGetStringForBytes() {
        final Context context = ApplicationProvider.getApplicationContext();
        Assert.assertEquals("0.00 KB", DownloadUtils.getStringForBytes(context, 0));
        Assert.assertEquals("0.51 KB", DownloadUtils.getStringForBytes(context, 512));
        Assert.assertEquals("1.00 KB", DownloadUtils.getStringForBytes(context, 1000));
        Assert.assertEquals("1.00 MB", DownloadUtils.getStringForBytes(context, 1_000_000));
        Assert.assertEquals("10.00 MB", DownloadUtils.getStringForBytes(context, 10_000_000));
        Assert.assertEquals("1.00 GB", DownloadUtils.getStringForBytes(context, 1_000_000_000L));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetAbbrieviatedFileName() {
        Assert.assertEquals("123.pdf", StringUtils.getAbbreviatedFileName("123.pdf", 10));
        Assert.assertEquals(
                "1" + StringUtils.ELLIPSIS, StringUtils.getAbbreviatedFileName("123.pdf", 1));
        Assert.assertEquals(
                "12" + StringUtils.ELLIPSIS, StringUtils.getAbbreviatedFileName("1234567", 2));
        Assert.assertEquals(
                "123" + StringUtils.ELLIPSIS + ".pdf",
                StringUtils.getAbbreviatedFileName("1234567.pdf", 7));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetAbbreviatedFileNameEarlyReturn() {
        // Pure ASCII with length <= limit should early return
        Assert.assertEquals("123.pdf", StringUtils.getAbbreviatedFileName("123.pdf", 7));
        // Unicode characters with length <= limit might exceed the budget and be abbreviated
        Assert.assertNotEquals("中中中中.apk", StringUtils.getAbbreviatedFileName("中中中中.apk", 8));
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetAbbreviatedFileNameOptimization() {
        // Verify that extremely long ASCII filenames with length > limit are abbreviated correctly
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 2000; i++) {
            sb.append("a");
        }
        sb.append(".txt");
        String veryLongName = sb.toString();
        String abbreviated = StringUtils.getAbbreviatedFileName(veryLongName, 10);
        Assert.assertEquals("aaaaaa" + StringUtils.ELLIPSIS + ".txt", abbreviated);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetAbbreviatedFileNameUnicodeOptimization() {
        // Verify that extremely long Unicode filenames do not cause timeout and loop limit works
        StringBuilder sb = new StringBuilder();
        for (int i = 0; i < 2000; i++) {
            sb.append("中");
        }
        sb.append(".txt");
        String veryLongUnicodeName = sb.toString();
        String abbreviated = StringUtils.getAbbreviatedFileName(veryLongUnicodeName, 10);
        Assert.assertTrue(abbreviated.endsWith(".txt"));
        Assert.assertTrue(abbreviated.contains(StringUtils.ELLIPSIS));
        // The base name part should be truncated and its character length should not exceed 6
        // (limit - 4)
        int ellipsisIndex = abbreviated.indexOf(StringUtils.ELLIPSIS);
        String basePart = abbreviated.substring(0, ellipsisIndex);
        Assert.assertTrue(basePart.length() <= 6);
        Assert.assertTrue(basePart.length() > 0);
    }

    @Test
    @SmallTest
    @Feature({"Download"})
    public void testGetAbbreviatedFileNameUnicode() {
        // Verify that it handles very wide Unicode and decorative characters, and places ellipsis
        // correctly
        String longUnicode = "𠀋𠀋𠀋𠀋𠀋𠀋𠀋𠀋𠀋𠀋𠀋𠀋𠀋𠀋.apk";
        String abbreviated = StringUtils.getAbbreviatedFileName(longUnicode, 15);
        Assert.assertTrue(abbreviated.endsWith(".apk"));
        Assert.assertTrue(abbreviated.contains(StringUtils.ELLIPSIS));

        // Verify surrogate-pair integrity (no broken/halved characters)
        for (int i = 0; i < abbreviated.length(); i++) {
            char c = abbreviated.charAt(i);
            if (Character.isHighSurrogate(c)) {
                Assert.assertTrue(i + 1 < abbreviated.length());
                Assert.assertTrue(Character.isLowSurrogate(abbreviated.charAt(i + 1)));
            } else if (Character.isLowSurrogate(c)) {
                Assert.assertTrue(i - 1 >= 0);
                Assert.assertTrue(Character.isHighSurrogate(abbreviated.charAt(i - 1)));
            }
        }
    }

    private static class ProgressBuilder {
        public static Progress indeterminate() {
            return Progress.createIndeterminateProgress();
        }

        public static Progress bytes(long bytes, Long max) {
            return new Progress(bytes, max, OfflineItemProgressUnit.BYTES);
        }

        public static Progress files(long files, Long max) {
            return new Progress(files, max, OfflineItemProgressUnit.FILES);
        }

        public static Progress percentage(long percentage, Long max) {
            return new Progress(percentage, max, OfflineItemProgressUnit.PERCENTAGE);
        }
    }
}
