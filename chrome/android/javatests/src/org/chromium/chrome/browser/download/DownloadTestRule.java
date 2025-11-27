// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import org.chromium.chrome.test.ChromeTabbedActivityTestRule;

import java.io.IOException;
import java.util.List;

/**
 * Custom TestRule for tests that need to download a file.
 *
 * <p>This has to be a base class because some classes (like BrowserEvent) are exposed only to
 * children of ChromeActivityTestCaseBase. It is a very broken approach to sharing but the only
 * other option is to refactor the ChromeActivityTestCaseBase implementation and all of our test
 * cases.
 */
public class DownloadTestRule extends ChromeTabbedActivityTestRule {
    private DownloadTestHelper mHelper;
    private final CustomMainActivityStart mActivityStart;

    public DownloadTestRule(CustomMainActivityStart action) {
        mActivityStart = action;
    }

    public boolean hasDownloaded(String fileName, String expectedContents) {
        return mHelper.hasDownloaded(fileName, expectedContents);
    }

    public boolean hasDownloadedRegex(String fileNameRegex) {
        return mHelper.hasDownloadedRegex(fileNameRegex);
    }

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

    @Override
    protected void before() throws Throwable {
        super.before();

        mActivityStart.customMainActivityStart();
        mHelper = DownloadTestHelper.create(this::getActivity);
    }

    @Override
    protected void after() {
        mHelper.close();

        super.after();
    }

    public void deleteFilesInDownloadDirectory(String... filenames) {
        mHelper.deleteFilesInDownloadDirectory(filenames);
    }

    /**
     * Interface for Download tests to define actions that starts the activity.
     *
     * <p>This method will be called in DownloadTestRule's setUp process, which means it would
     * happen before Test class' own setUp() call
     */
    public interface CustomMainActivityStart {
        void customMainActivityStart() throws InterruptedException;
    }
}
