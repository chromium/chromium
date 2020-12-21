// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.content.Context;

import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.util.LinkedList;
import java.util.Locale;

import android.os.StrictMode;

/**
 * {@link PersistedTabDataStorage} which uses a file for the storage
 */
public class FilePersistedTabDataStorage implements PersistedTabDataStorage {
    private static final String TAG = "FilePTDS";
    protected static final Callback<Integer> NO_OP_CALLBACK = new Callback<Integer>() {
        @Override
        public void onResult(Integer result) {}
    };
    private static final int DECREMENT_SEMAPHORE_VAL = 1;

    private static final String sBaseDirName = "persisted_tab_data_storage";
    private static class BaseStorageDirectoryHolder {
        private static File sDirectory;

        static {
            StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskWrites();
            try {
                sDirectory =
                ContextUtils.getApplicationContext().getDir(sBaseDirName, Context.MODE_PRIVATE);
            } finally {
                StrictMode.setThreadPolicy(oldPolicy);
            }
        }
    }
    private SequencedTaskRunner mSequencedTaskRunner;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected LinkedList<StorageRequest> mQueue = new LinkedList<>();

    protected FilePersistedTabDataStorage() {
        mSequencedTaskRunner =
                PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);
    }

    @MainThread
    @Override
    public void save(int tabId, String dataId, byte[] data) {
        save(tabId, dataId, data, NO_OP_CALLBACK);
    }

    // Callback used for test synchronization between save, restore and delete operations
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void save(int tabId, String dataId, byte[] data, Callback<Integer> callback) {
        ThreadUtils.assertOnUiThread();
        // TODO(crbug.com/1059637) we should introduce a retry mechanisms
        addSaveRequest(new FileSaveRequest(tabId, dataId, data, callback));
        processNextItemOnQueue();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void addSaveRequest(FileSaveRequest fileSaveRequest) {
        // FileSaveRequest for the same tabid/data id will get overwritten
        // by new FileSaveRequest so remove if it exists in the queue.
        mQueue.remove(fileSaveRequest);
        mQueue.add(fileSaveRequest);
    }

    @MainThread
    @Override
    public void restore(int tabId, String dataId, Callback<byte[]> callback) {
        ThreadUtils.assertOnUiThread();
        mQueue.add(new FileRestoreRequest(tabId, dataId, callback));
        processNextItemOnQueue();
    }

    @MainThread
    @Override
    public byte[] restore(int tabId, String dataId) {
        return new FileRestoreRequest(tabId, dataId, null).executeSyncTask();
    }

    @MainThread
    @Override
    public void delete(int tabId, String dataId) {
        delete(tabId, dataId, NO_OP_CALLBACK);
    }

    // Callback used for test synchronization between save, restore and delete operations
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void delete(int tabId, String dataId, Callback<Integer> callback) {
        ThreadUtils.assertOnUiThread();
        mQueue.add(new FileDeleteRequest(tabId, dataId, callback));
        processNextItemOnQueue();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected File getFile(int tabId, String dataId) {
        return new File(getOrCreateBaseStorageDirectory(),
                String.format(Locale.ENGLISH, "%d%s", tabId, dataId));
    }

    public static File getOrCreateBaseStorageDirectory() {
        return BaseStorageDirectoryHolder.sDirectory;
    }

    /**
     * Request for saving, restoring and deleting {@link PersistedTabData}
     */
    private abstract class StorageRequest<T> {
        protected final int mTabId;
        protected final String mDataId;
        protected final File mFile;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         */
        StorageRequest(int tabId, String dataId) {
            mTabId = tabId;
            mDataId = dataId;
            mFile = getFile(tabId, dataId);
        }

        /**
         * @return unique identifier for the StorageRequest
         */
        String getRequestId() {
            return String.format(Locale.ENGLISH, "%d_%s", mTabId, mDataId);
        }

        /**
         * AsyncTask to execute the StorageRequest
         */
        abstract AsyncTask getAsyncTask();

        /**
         * Execute the task synchronously
         */
        abstract T executeSyncTask();

        @Override
        public boolean equals(Object other) {
            if (this == other) return true;
            if (other == null) return false;
            if (!(other instanceof StorageRequest)) return false;
            StorageRequest otherStorageRequest = (StorageRequest) other;
            return mTabId == otherStorageRequest.mTabId
                    && mDataId.equals(otherStorageRequest.mDataId)
                    && mFile.equals(otherStorageRequest.mFile);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + mTabId;
            result = 31 * result + mDataId.hashCode();
            result = 31 * result + mFile.hashCode();
            return result;
        }
    }

    /**
     * Request to save {@link PersistedTabData}
     */
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected class FileSaveRequest extends StorageRequest<Void> {
        private byte[] mData;
        private Callback<Integer> mCallback;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param data - data to be saved
         */
        FileSaveRequest(int tabId, String dataId, byte[] data, Callback<Integer> callback) {
            super(tabId, dataId);
            mData = data;
            mCallback = callback;
        }

        @Override
        public Void executeSyncTask() {
            FileOutputStream outputStream = null;
            boolean success = false;
            try {
                outputStream = new FileOutputStream(mFile);
                outputStream.write(mData);
                success = true;
            } catch (FileNotFoundException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "FileNotFoundException while attempting to save file %s "
                                        + "Details: %s",
                                mFile, e.getMessage()));
            } catch (IOException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "IOException while attempting to save for file %s. "
                                        + " Details: %s",
                                mFile, e.getMessage()));
            } finally {
                StreamUtil.closeQuietly(outputStream);
            }
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Save." + getUmaTag(), success);
            return null;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    return executeSyncTask();
                }

                @Override
                protected void onPostExecute(Void result) {
                    PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                            () -> { mCallback.onResult(DECREMENT_SEMAPHORE_VAL); });
                    processNextItemOnQueue();
                }
            };
        }

        @Override
        public boolean equals(Object other) {
            if (!(other instanceof FileSaveRequest)) return false;
            return super.equals(other);
        }
    }

    /**
     * Request to delete a saved {@link PersistedTabData}
     */
    private class FileDeleteRequest extends StorageRequest<Void> {
        private byte[] mData;
        private Callback<Integer> mCallback;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         */
        FileDeleteRequest(int tabId, String dataId, Callback<Integer> callback) {
            super(tabId, dataId);
            mCallback = callback;
        }

        @Override
        public Void executeSyncTask() {
            boolean exists = mFile.exists();
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Exists." + getUmaTag(), exists);
            if (!exists) {
                return null;
            }
            boolean success = mFile.delete();
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Delete." + getUmaTag(), success);
            if (!success) {
                Log.e(TAG, String.format(Locale.ENGLISH, "Error deleting file %s", mFile));
            }
            return null;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<Void>() {
                @Override
                protected Void doInBackground() {
                    return executeSyncTask();
                }

                @Override
                protected void onPostExecute(Void result) {
                    PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT,
                            () -> { mCallback.onResult(DECREMENT_SEMAPHORE_VAL); });
                    processNextItemOnQueue();
                }
            };
        }
        @Override
        public boolean equals(Object other) {
            if (!(other instanceof FileDeleteRequest)) return false;
            return super.equals(other);
        }
    }

    /**
     * Request to restore saved serialized {@link PersistedTabData}
     */
    private class FileRestoreRequest extends StorageRequest<byte[]> {
        private Callback<byte[]> mCallback;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param callback - callback to return the retrieved serialized
         * {@link PersistedTabData} in
         */
        FileRestoreRequest(int tabId, String dataId, Callback<byte[]> callback) {
            super(tabId, dataId);
            mCallback = callback;
        }

        @Override
        public byte[] executeSyncTask() {
            boolean success = false;
            byte[] res = null;
            try {
                AtomicFile atomicFile = new AtomicFile(mFile);
                res = atomicFile.readFully();
                success = true;
            } catch (FileNotFoundException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "FileNotFoundException while attempting to restore "
                                        + " %s. Details: %s",
                                mFile, e.getMessage()));
            } catch (IOException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "IOException while attempting to restore "
                                        + "%s. Details: %s",
                                mFile, e.getMessage()));
            }
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Restore." + getUmaTag(), success);
            return res;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<byte[]>() {
                @Override
                protected byte[] doInBackground() {
                    return executeSyncTask();
                }

                @Override
                protected void onPostExecute(byte[] res) {
                    PostTask.runOrPostTask(
                            UiThreadTaskTraits.DEFAULT, () -> { mCallback.onResult(res); });
                    processNextItemOnQueue();
                }
            };
        }
        @Override
        public boolean equals(Object other) {
            if (!(other instanceof FileRestoreRequest)) return false;
            return super.equals(other);
        }
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void processNextItemOnQueue() {
        if (mQueue.isEmpty()) return;
        mQueue.poll().getAsyncTask().executeOnTaskRunner(mSequencedTaskRunner);
    }

    @Override
    public String getUmaTag() {
        return "File";
    }
}
