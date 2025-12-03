// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.junit.rules.ExternalResource;

import org.chromium.chrome.browser.ChromeTabbedActivity;

import java.io.IOException;
import java.util.List;

/** Custom TestRule for tests that need to download a file. */
public class DownloadTestRule extends ExternalResource {
    private DownloadTestHelper mHelper;

    public DownloadTestRule() {}

    /** Call this after the activity has been created. */
    public void attach(ChromeTabbedActivity activity) {
        assert mHelper == null : "attachToActivity() does not yet support multiple activities";

        mHelper = new DownloadTestHelper(activity);
        mHelper.attach();
    }

    @Override
    protected void after() {
        if (mHelper != null) {
            mHelper.detach();
        }
    }

    /**
     * Checks if a file has downloaded. Is agnostic to the mechanism by which the file has
     * downloaded.
     *
     * @param fileName Expected file name. Path is built by appending filename to the system
     *     downloads path.
     * @param expectedContents Expected contents of the file, or null if the contents should not be
     *     checked.
     */
    public boolean hasDownloaded(String fileName, String expectedContents) {
        return mHelper.hasDownloaded(fileName, expectedContents);
    }

    /**
     * Checks if a file matching the regex has downloaded. Is agnostic to the mechanism by which the
     * file has downloaded.
     *
     * @param fileNameRegex Expected regex the file name should match. Files are non-recursively
     *     searched in the system downloads path.
     */
    public boolean hasDownloadedRegex(String fileNameRegex) {
        return mHelper.hasDownloadedRegex(fileNameRegex);
    }

    /**
     * Check the download exists in DownloadManager by matching the local file path.
     *
     * @param fileName Expected file name. Path is built by appending filename to the system
     *     downloads path.
     * @param expectedContents Expected contents of the file, or null if the contents should not be
     *     checked.
     */
    public boolean hasDownload(String fileName, String expectedContents) throws IOException {
        return mHelper.hasDownload(fileName, expectedContents);
    }

    public int getChromeDownloadCallCount() {
        return mHelper.getChromeDownloadCallCount();
    }

    protected void resetCallbackHelper() {
        mHelper.resetCallbackHelper();
    }

    public boolean waitForChromeDownloadToFinish(int currentCallCount) {
        return mHelper.waitForChromeDownloadToFinish(currentCallCount);
    }

    public List<DownloadItem> getAllDownloads() {
        return mHelper.getAllDownloads();
    }

    public void deleteFilesInDownloadDirectory(String... filenames) {
        mHelper.deleteFilesInDownloadDirectory(filenames);
    }
}
