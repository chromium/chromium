// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.DownloadManager;
import android.content.Context;
import android.database.Cursor;
import android.net.Uri;
import android.os.Build;
import android.os.Environment;
import android.text.TextUtils;

import org.junit.Assert;

import org.chromium.base.ApiCompatibilityUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.download.items.OfflineContentAggregatorFactory;
import org.chromium.chrome.browser.profiles.ProfileKey;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.components.download.DownloadCollectionBridge;
import org.chromium.components.offline_items_collection.ContentId;
import org.chromium.components.offline_items_collection.OfflineContentProvider;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemState;
import org.chromium.components.offline_items_collection.UpdateDelta;

import java.io.File;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.List;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.TimeoutException;

/**
 * Custom TestRule for tests that need to download a file.
 *
 * <p>This has to be a base class because some classes (like BrowserEvent) are exposed only to
 * children of ChromeActivityTestCaseBase. It is a very broken approach to sharing but the only
 * other option is to refactor the ChromeActivityTestCaseBase implementation and all of our test
 * cases.
 */
public class DownloadTestRule extends ChromeTabbedActivityTestRule {
    private static final String TAG = "DownloadTestBase";
    private static final File DOWNLOAD_DIRECTORY =
            Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS);
    public static final long UPDATE_DELAY_MILLIS = 1000;

    private final CustomMainActivityStart mActivityStart;
    private List<DownloadItem> mAllDownloads;

    public DownloadTestRule(CustomMainActivityStart action) {
        mActivityStart = action;
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
        try {
            File downloadedFile = getDownloadedPath(fileName);
            if (!downloadedFile.exists()) {
                return false;
            }
            if (expectedContents != null) {
                checkFileContents(downloadedFile.getAbsolutePath(), expectedContents);
            }
            return true;
        } catch (IOException e) {
            Assert.fail("IOException when opening file " + fileName);
            return false;
        }
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
        File downloadedFile = getDownloadedPath(fileName);
        if (!downloadedFile.exists()) {
            return false;
        }

        DownloadManager manager =
                (DownloadManager) getActivity().getSystemService(Context.DOWNLOAD_SERVICE);
        Cursor cursor = manager.query(new DownloadManager.Query());

        cursor.moveToFirst();
        boolean result = false;
        while (!cursor.isAfterLast()) {
            if (fileName.equals(getTitleFromCursor(cursor))) {
                if (expectedContents != null) {
                    checkFileContents(downloadedFile.getAbsolutePath(), expectedContents);
                }
                result = true;
                break;
            }
            cursor.moveToNext();
        }
        cursor.close();
        return result;
    }

    private static File getDownloadedPath(String fileName) {
        File downloadedFile = new File(DOWNLOAD_DIRECTORY, fileName);
        if (!downloadedFile.exists()) {
            Log.d(TAG, "The file " + fileName + " does not exist");
        }
        return downloadedFile;
    }

    private static void checkFileContents(String fullPath, String expectedContents)
            throws IOException {
        FileInputStream stream = new FileInputStream(new File(fullPath));
        byte[] data = new byte[ApiCompatibilityUtils.getBytesUtf8(expectedContents).length];
        try {
            Assert.assertEquals(stream.read(data), data.length);
            String contents = new String(data);
            Assert.assertEquals(expectedContents, contents);
        } finally {
            stream.close();
        }
    }

    /** Delete all download entries in DownloadManager and delete the corresponding files. */
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
    private CallbackHelper mHttpDownloadFinished = new CallbackHelper();
    private TestDownloadManagerServiceObserver mDownloadManagerServiceObserver;

    public int getChromeDownloadCallCount() {
        return mHttpDownloadFinished.getCallCount();
    }

    protected void resetCallbackHelper() {
        mHttpDownloadFinished = new CallbackHelper();
    }

    public boolean waitForChromeDownloadToFinish(int currentCallCount) {
        boolean eventReceived = true;
        try {
            mHttpDownloadFinished.waitForCallback(currentCallCount, 1, 10, TimeUnit.SECONDS);
        } catch (TimeoutException e) {
            eventReceived = false;
        }
        return eventReceived;
    }

    public List<DownloadItem> getAllDownloads() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DownloadManagerService.getDownloadManagerService().getAllDownloads(null);
                });
        return mAllDownloads;
    }

    private class TestDownloadManagerServiceObserver
            implements DownloadManagerService.DownloadObserver {
        @Override
        public void onAllDownloadsRetrieved(final List<DownloadItem> list, ProfileKey profileKey) {
            mAllDownloads = list;
        }

        @Override
        public void onDownloadItemCreated(DownloadItem item) {}

        @Override
        public void onDownloadItemRemoved(String guid) {}

        @Override
        public void onAddOrReplaceDownloadSharedPreferenceEntry(ContentId id) {}

        @Override
        public void onDownloadItemUpdated(DownloadItem item) {}

        @Override
        public void broadcastDownloadSuccessful(DownloadInfo downloadInfo) {
            mLastDownloadFilePath = downloadInfo.getFilePath();
            mHttpDownloadFinished.notifyCalled();
        }
    }

    private class TestDownloadBackendObserver implements OfflineContentProvider.Observer {
        @Override
        public void onItemsAdded(List<OfflineItem> items) {}

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
    protected void before() throws Throwable {
        super.before();
        mActivityStart.customMainActivityStart();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DownloadDialogBridge.setPromptForDownloadAndroid(
                            getActivity().getProfileProviderSupplier().get().getOriginalProfile(),
                            DownloadPromptStatus.DONT_SHOW);
                });

        cleanUpAllDownloads();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mDownloadManagerServiceObserver = new TestDownloadManagerServiceObserver();
                    DownloadManagerService.getDownloadManagerService()
                            .addDownloadObserver(mDownloadManagerServiceObserver);
                    OfflineContentAggregatorFactory.get()
                            .addObserver(new TestDownloadBackendObserver());
                });
    }

    @Override
    protected void after() {
        cleanUpAllDownloads();
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    DownloadManagerService.getDownloadManagerService()
                            .removeDownloadObserver(mDownloadManagerServiceObserver);
                });
        super.after();
    }

    public void deleteFilesInDownloadDirectory(String... filenames) {
        for (String filename : filenames) deleteFile(filename);
    }

    private void deleteFile(String fileName) {
        // Delete file path on pre Q.
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.Q) {
            final File fileToDelete = new File(DOWNLOAD_DIRECTORY, fileName);
            if (fileToDelete.exists()) {
                Assert.assertTrue(
                        "Could not delete file that would block this test", fileToDelete.delete());
            }
            return;
        }

        // Delete content URI starting from Q.
        Uri uri = DownloadCollectionBridge.getDownloadUriForFileName(fileName);
        if (uri == null) {
            Log.e(TAG, "Can't find URI of file for deletion: %s on Android P+.", fileName);
            return;
        }
        DownloadCollectionBridge.deleteIntermediateUri(uri.toString());
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
