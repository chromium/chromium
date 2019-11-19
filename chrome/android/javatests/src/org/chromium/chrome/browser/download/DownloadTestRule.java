// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.Context;
import android.database.Cursor;
import android.os.Environment;
import android.text.TextUtils;

import org.junit.Assert;
import org.junit.runner.Description;
import org.junit.runners.model.Statement;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Custom TestRule for tests that need to download a file.
 *
 * This has to be a base class because some classes (like BrowserEvent) are exposed only
 * to children of ChromeActivityTestCaseBase. It is a very broken approach to sharing
 * but the only other option is to refactor the ChromeActivityTestCaseBase implementation
 * and all of our test cases.
 *
 */
public class DownloadTestRule extends ChromeActivityTestRule<ChromeActivity> {
    private static final String TAG = "DownloadTestBase";
    private static final File DOWNLOAD_DIRECTORY =
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    public static final long UPDATE_DELAY_MILLIS = 1000;

    private final CustomMainActivityStart mActivityStart;

    public DownloadTestRule(CustomMainActivityStart action) {
        super(ChromeActivity.class);
        mActivityStart = action;
    }

    /**
     * Check the download exists in DownloadManager by matching the local file
     * path.
     *
     * @param fileName Expected file name. Path is built by appending filename to
     * the system downloads path.
     *
     * @param expectedContents Expected contents of the file, or null if the contents should not be
     * checked.
     */
    public boolean hasDownload(String fileName, String expectedContents) throws IOException {
        File downloadedFile = new File(DOWNLOAD_DIRECTORY, fileName);
        if (!downloadedFile.exists()) {
            Log.d(TAG, "The file " + fileName + " does not exist");
            return false;
        }

        String fullPath = downloadedFile.getAbsolutePath();

        DownloadManager manager =
                (DownloadManager) getActivity().getSystemService(Context.DOWNLOAD_SERVICE);
        Cursor cursor = manager.query(new DownloadManager.Query());

        cursor.moveToFirst();
        boolean result = false;
        while (!cursor.isAfterLast()) {
            if (fileName.equals(getTitleFromCursor(cursor))) {
                if (expectedContents != null) {
                    FileInputStream stream = new FileInputStream(new File(fullPath));
                    byte[] data =
                            new byte[ApiCompatibilityUtils.getBytesUtf8(expectedContents).length];
                    try {
                        Assert.assertEquals(stream.read(data), data.length);
                        String contents = new String(data);
                        Assert.assertEquals(expectedContents, contents);
                    } finally {
                        stream.close();
                    }
                }
                result = true;
                break;
            }
            cursor.moveToNext();
        }
        cursor.close();
        return result;
    }

    /**
     * Check the last download matches the given name and exists in DownloadManager.
     */
    public void checkLastDownload(String fileName) throws IOException {
        String lastDownload = getLastDownloadFile();
        Assert.assertTrue(isSameDownloadFile(fileName, lastDownload));
        Assert.assertTrue(hasDownload(lastDownload, null));
    }

    /**
     * Delete all download entries in DownloadManager and delete the corresponding files.
     */
    private void cleanUpAllDownloads() {
        DownloadManager manager =
                (DownloadManager) getActivity().getSystemService(Context.DOWNLOAD_SERVICE);
        Cursor cursor = manager.query(new DownloadManager.Query());
        int idColumnIndex = cursor.getColumnIndexOrThrow(DownloadManager.COLUMN_ID);
        cursor.moveToFirst();
        while (!cursor.isAfterLast()) {
            long id = cursor.getLong(idColumnIndex);
            String fileName = getTitleFromCursor(cursor);
            manager.remove(id);

            // manager.remove does not remove downloaded file.
            if (!TextUtils.isEmpty(fileName)) {
                File localFile = new File(DOWNLOAD_DIRECTORY, fileName);
                if (localFile.exists()) {
                    localFile.delete();
                }
            }

            cursor.moveToNext();
        }
        cursor.close();
    }

    /**
     * Retrieve the title of the download from a DownloadManager cursor, the title should correspond
     * to the filename of the downloaded file, unless the title has been set explicitly.
     */
    private String getTitleFromCursor(Cursor cursor) {
        return cursor.getString(cursor.getColumnIndex(DownloadManager.COLUMN_TITLE));
    }

    private String mLastDownloadFilePath;
    private final CallbackHelper mHttpDownloadFinished = new CallbackHelper();
    private TestDownloadManagerServiceObserver mDownloadManagerServiceObserver;

    public String getLastDownloadFile() {
        return new File(mLastDownloadFilePath).getName();
    }

    // The Android DownloadManager sometimes appends a number to a file name when it downloads it
    // ex: google.png becomes google-23.png
    // This happens even when there is no other prior download with that name, it could be a bug.
    // TODO(jcivelli): investigate if we can isolate that behavior and file a bug to Android.
    public boolean isSameDownloadFile(String originalName, String downloadName) {
        String fileName = originalName;
        String extension = "";
        int dotIndex = originalName.lastIndexOf('.');
        if (dotIndex != -1 && dotIndex < originalName.length()) {
            fileName = originalName.substring(0, dotIndex);
            extension = originalName.substring(dotIndex); // We include the '.'
        }
        return downloadName.startsWith(fileName) && downloadName.endsWith(extension);
    }

    public int getChromeDownloadCallCount() {
        return mHttpDownloadFinished.getCallCount();
    }

    public boolean waitForChromeDownloadToFinish(int currentCallCount) {
        boolean eventReceived = true;
        try {
            mHttpDownloadFinished.waitForCallback(currentCallCount, 1, 5, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            eventReceived = false;
        }
        return eventReceived;
    }

    private class TestDownloadManagerServiceObserver
            implements DownloadManagerService.DownloadObserver {
        @Override
        public void onAllDownloadsRetrieved(final List<DownloadItem> list, boolean isOffTheRecord) {
        }

        @Override
        public void onDownloadItemCreated(DownloadItem item) {}

        @Override
        public void onDownloadItemRemoved(String guid, boolean isOffTheRecord) {}

        @Override
        public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {}

        @Override
        public void onDownloadItemUpdated(DownloadItem item) {}

        @Override
        public void broadcastDownloadSuccessfulForTesting(DownloadInfo downloadInfo) {
            mLastDownloadFilePath = downloadInfo.getFilePath();
            mHttpDownloadFinished.notifyCalled();
        }
    }

    private class TestDownloadBackendObserver implements OfflineContentProvider.Observer {
        @Override
        public void onItemsAdded(ArrayList<OfflineItem> items) {}

        @Override
        public void onItemRemoved(ContentId id) {}

        @Override
        public void onItemUpdated(OfflineItem item, UpdateDelta updateDelta) {
            if (item.state == OfflineItemState.COMPLETE) {
                mLastDownloadFilePath = item.filePath;
                mHttpDownloadFinished.notifyCalled();
            }
        }
    }

    @Override
    public Statement apply(final Statement base, Description description) {
        return super.apply(new Statement() {
            @Override
            public void evaluate() throws Throwable {
                setUp();
                base.evaluate();
                tearDown();
            }
        }, description);
    }

    private void setUp() throws Exception {
        mActivityStart.customMainActivityStart();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadUtils.setPromptForDownloadAndroid(DownloadPromptStatus.DONT_SHOW);
        });

        cleanUpAllDownloads();

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mDownloadManagerServiceObserver = new TestDownloadManagerServiceObserver();
            DownloadManagerService.getDownloadManagerService().addDownloadObserver(
                    mDownloadManagerServiceObserver);
            OfflineContentAggregatorFactory.get().addObserver(new TestDownloadBackendObserver());
        });
    }

    private void tearDown() {
        cleanUpAllDownloads();
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            DownloadManagerService.getDownloadManagerService().removeDownloadObserver(
                    mDownloadManagerServiceObserver);
        });
    }

    public void deleteFilesInDownloadDirectory(String... filenames) {
        for (String filename : filenames) {
            final File fileToDelete = new File(DOWNLOAD_DIRECTORY, filename);
            if (fileToDelete.exists()) {
                Assert.assertTrue(
                        "Could not delete file that would block this test", fileToDelete.delete());
            }
        }
    }

    /**
     * Interface for Download tests to define actions that starts the activity.
     *
     * This method will be called in DownloadTestRule's setUp process, which means
     * it would happen before Test class' own setUp() call
     **/
    public interface CustomMainActivityStart {
        void customMainActivityStart() throws InterruptedException;
    }
}
