// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feedback;

import android.content.Context;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;

import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.RobolectricUtil;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.Map;

/** Unit tests for {@link UploadedCrashIdsFeedbackSource}. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class UploadedCrashIdsFeedbackSourceTest {
    private Context mContext;
    private File mLogFile;

    @Before
    public void setUp() throws IOException {
        mContext = ContextUtils.getApplicationContext();
        // Robolectric provides a fresh cache directory for every test, so no need to clean this up
        // afterwards.
        File crashDir = new File(mContext.getCacheDir(), "Crash Reports");
        if (!crashDir.exists()) {
            Assert.assertTrue(crashDir.mkdirs());
        }
        mLogFile = new File(crashDir, "uploads.log");
        Assert.assertFalse(mLogFile.exists());
    }

    @Test
    public void testDoInBackground_NoLogFile() {
        UploadedCrashIdsFeedbackSource source = new UploadedCrashIdsFeedbackSource();
        Assert.assertNull(source.doInBackground(mContext));
    }

    @Test
    public void testDoInBackground_EmptyLogFile() throws IOException {
        Assert.assertTrue(mLogFile.createNewFile());
        UploadedCrashIdsFeedbackSource source = new UploadedCrashIdsFeedbackSource();
        Assert.assertNull(source.doInBackground(mContext));
    }

    @Test
    public void testDoInBackground_WithEntries() throws IOException {
        try (FileWriter writer = new FileWriter(mLogFile)) {
            writer.write("123456789,upload_id_1,local_id_1\n");
            writer.write("987654321,upload_id_2,local_id_2\n");
        }

        UploadedCrashIdsFeedbackSource source = new UploadedCrashIdsFeedbackSource();
        String result = source.doInBackground(mContext);
        Assert.assertEquals("upload_id_1,upload_id_2", result);
    }

    @Test
    public void testDoInBackground_WithInvalidEntries() throws IOException {
        try (FileWriter writer = new FileWriter(mLogFile)) {
            writer.write("123456789,upload_id_1,local_id_1\n");
            writer.write("invalid_line\n");
            writer.write("987654321,,local_id_2\n");
            writer.write("111222333,upload_id_3\n");
        }

        UploadedCrashIdsFeedbackSource source = new UploadedCrashIdsFeedbackSource();
        String result = source.doInBackground(mContext);
        Assert.assertEquals("upload_id_1,upload_id_3", result);
    }

    @Test
    public void testGetFeedback_Async() throws IOException {
        try (FileWriter writer = new FileWriter(mLogFile)) {
            writer.write("123456789,upload_id_1,local_id_1\n");
        }

        UploadedCrashIdsFeedbackSource source = new UploadedCrashIdsFeedbackSource();
        source.start(() -> {});
        RobolectricUtil.runAllBackgroundAndUiIncludingDelayed();

        Assert.assertTrue("Source should be ready.", source.isReady());
        Map<String, String> feedback = source.getFeedback();
        Assert.assertNotNull("Feedback should not be null.", feedback);
        Assert.assertEquals("upload_id_1", feedback.get("uploaded_crash_hashes"));
    }
}
