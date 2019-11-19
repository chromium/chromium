// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.components.minidump_uploader.CrashFileManager;

import java.io.BufferedInputStream;
import java.io.BufferedOutputStream;
import java.io.BufferedReader;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.io.FileReader;
import java.io.FileWriter;
import java.io.IOException;
import java.util.List;

/**
 * Prepends a logcat file to a minidump file for upload.
 */
public class MinidumpLogcatPrepender {
    private static final String TAG = "LogcatPrepender";

    @VisibleForTesting
    static final String LOGCAT_CONTENT_DISPOSITION =
            "Content-Disposition: form-data; name=\"logcat\"; filename=\"logcat\"";

    @VisibleForTesting
    static final String LOGCAT_CONTENT_TYPE = "Content-Type: text/plain";

    private final CrashFileManager mFileManager;
    private final File mMinidumpFile;
    private final List<String> mLogcat;

    public MinidumpLogcatPrepender(
            CrashFileManager fileManager, File minidumpFile, List<String> logcat) {
        mFileManager = fileManager;
        mMinidumpFile = minidumpFile;
        mLogcat = logcat;
    }

    /**
     * Read the boundary from the first line of the file.
     */
    private static String getBoundary(File minidumpFile) throws IOException {
        BufferedReader reader = null;
        try {
            reader = new BufferedReader(new FileReader(minidumpFile));
            return reader.readLine();
        } finally {
            if (reader != null) {
                reader.close();
            }
        }
    }

    /**
     * Write the logcat data to the specified target {@link File}.
     *
     * Target file is overwritten, not appended to the end.
     *
     * @param targetFile File to which logcat data should be written.
     * @param logcat The lines of the logcat output.
     * @param boundary String MIME boundary to prepend.
     * @throws IOException if something goes wrong.
     */
    private static void writeLogcat(File targetFile, List<String> logcat, String boundary)
            throws IOException {
        BufferedWriter writer = null;
        try {
            writer = new BufferedWriter(new FileWriter(targetFile, false));
            writer.write(boundary);
            writer.newLine();
            // Next we write the logcat data in a MIME block.
            writer.write(LOGCAT_CONTENT_DISPOSITION);
            writer.newLine();
            writer.write(LOGCAT_CONTENT_TYPE);
            writer.newLine();
            writer.newLine();
            // Emits the contents of the buffer into the output file.
            for (String ln : logcat) {
                writer.write(ln);
                writer.newLine();
            }
        } finally {
            if (writer != null) {
                writer.close();
            }
        }
    }

    /**
     * Append the minidump file data to the specified target {@link File}.
     *
     * @param minidumpFile File containing data to append.
     * @param targetFile File to which data should be appended.
     * @throws IOException when standard IO errors occur.
     */
    private static void appendMinidump(File minidumpFile, File targetFile) throws IOException {
        BufferedInputStream in = null;
        BufferedOutputStream out = null;
        try {
            byte[] buf = new byte[256];
            in = new BufferedInputStream(new FileInputStream(minidumpFile));
            out = new BufferedOutputStream(new FileOutputStream(targetFile, true));
            int count;
            while ((count = in.read(buf)) != -1) {
                out.write(buf, 0, count);
            }
        } finally {
            if (in != null) in.close();
            if (out != null) out.close();
        }
    }

    /**
     * Prepends the logcat output to the minidump file.
     * @return On success, returns the file containing the combined logcat and minidump output.
     *     On failure, returns the original file containing just the minidump.
     */
    public File run() {
        if (mLogcat.isEmpty()) return mMinidumpFile;

        String targetFileName = mMinidumpFile.getName() + ".try0";
        File targetFile = null;
        boolean success = false;
        try {
            String boundary = getBoundary(mMinidumpFile);
            if (boundary == null) {
                return mMinidumpFile;
            }

            targetFile = mFileManager.createNewTempFile(targetFileName);
            writeLogcat(targetFile, mLogcat, boundary);

            // Finally reopen and append the original minidump MIME sections, including the leading
            // boundary.
            appendMinidump(mMinidumpFile, targetFile);
            success = true;
        } catch (IOException e) {
            Log.w(TAG, "Error while trying to annotate minidump file %s with logcat data",
                    mMinidumpFile.getAbsoluteFile(), e);
            if (targetFile != null) {
                CrashFileManager.deleteFile(targetFile);
            }
        }

        if (!success) return mMinidumpFile;

        // Try to clean up the previous file. Note that this step is best-effort, and failing to
        // perform the cleanup does not count as an overall failure to prepend the logcat output.
        if (!mMinidumpFile.delete()) {
            Log.w(TAG, "Failed to delete minidump file: " + mMinidumpFile.getName());
        }

        assert targetFile != null;
        assert targetFile.exists();
        return targetFile;
    }
}
