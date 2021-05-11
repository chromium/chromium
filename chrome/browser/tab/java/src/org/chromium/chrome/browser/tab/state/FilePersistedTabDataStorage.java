// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab.state;

import android.content.Context;
import android.os.StrictMode;
import android.os.SystemClock;

import androidx.annotation.IntDef;
import androidx.annotation.MainThread;
import androidx.annotation.VisibleForTesting;
import androidx.core.util.AtomicFile;

import org.chromium.base.Callback;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.supplier.Supplier;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.LinkedList;
import java.util.Locale;

/**
 * {@link PersistedTabDataStorage} which uses a file for the storage
 */
public class FilePersistedTabDataStorage implements PersistedTabDataStorage {
    private static final String TAG = "FilePTDS";
    protected static final Callback<Integer> NO_OP_CALLBACK = new Callback<Integer>() {
        @Override
        public void onResult(Integer result) {}
    };
    protected static final int DECREMENT_SEMAPHORE_VAL = 1;

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
    private boolean mFirstOperationRecorded;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected LinkedList<StorageRequest> mQueue = new LinkedList<>();

    protected FilePersistedTabDataStorage() {
        mSequencedTaskRunner =
                PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);
    }

    @MainThread
    @Override
    public void save(int tabId, String dataId, Supplier<byte[]> dataSupplier) {
        save(tabId, dataId, dataSupplier, NO_OP_CALLBACK);
    }

    // Callback used for test synchronization between save, restore and delete operations
    @MainThread
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void save(
            int tabId, String dataId, Supplier<byte[]> dataSupplier, Callback<Integer> callback) {
        // TODO(crbug.com/1059637) we should introduce a retry mechanisms
        addSaveRequest(new FileSaveRequest(tabId, dataId, dataSupplier, callback));
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
        addStorageRequestAndProcessNext(new FileRestoreRequest(tabId, dataId, callback));
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
    @MainThread
    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void delete(int tabId, String dataId, Callback<Integer> callback) {
        addStorageRequestAndProcessNext(new FileDeleteRequest(tabId, dataId, callback));
    }

    protected void addStorageRequestAndProcessNext(StorageRequest storageRequest) {
        mQueue.add(storageRequest);
        processNextItemOnQueue();
    }

    /**
     * @return {@link File} serialized {@link CriticalPersistedTabData} is stored in
     * @param tabId tab identifier
     * @param dataId type of data stored for the {@link Tab}
     */
    protected static File getFile(int tabId, String dataId) {
        return new File(getOrCreateBaseStorageDirectory(),
                String.format(Locale.ENGLISH, "%d%s", tabId, dataId));
    }

    public static File getOrCreateBaseStorageDirectory() {
        return BaseStorageDirectoryHolder.sDirectory;
    }

    /**
     * Request for saving, restoring and deleting {@link PersistedTabData}
     */
    protected abstract class StorageRequest<T> {
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
            mFile = FilePersistedTabDataStorage.getFile(tabId, dataId);
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

        /**
         * @return type of storage request (save, restore or delete)
         */
        abstract @StorageRequestType int getStorageRequestType();
    }

    /**
     * Request to save {@link PersistedTabData}
     */
    protected class FileSaveRequest extends StorageRequest<Void> {
        protected Supplier<byte[]> mDataSupplier;
        protected Callback<Integer> mCallback;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param dataSupplier {@link Supplier} containing data to be saved
         */
        FileSaveRequest(int tabId, String dataId, Supplier<byte[]> dataSupplier,
                Callback<Integer> callback) {
            super(tabId, dataId);
            mDataSupplier = dataSupplier;
            mCallback = callback;
        }

        @Override
        public Void executeSyncTask() {
            byte[] data = mDataSupplier.get();
            if (data == null) {
                mDataSupplier = null;
                return null;
            }
            FileOutputStream outputStream = null;
            boolean success = false;
            try {
                long startTime = SystemClock.elapsedRealtime();
                outputStream = new FileOutputStream(mFile);
                outputStream.write(data);
                success = true;
                RecordHistogram.recordTimesHistogram(
                        String.format(Locale.US, "Tabs.PersistedTabData.Storage.SaveTime.%s",
                                getUmaTag()),
                        SystemClock.elapsedRealtime() - startTime);
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

        @Override
        @StorageRequestType
        int getStorageRequestType() {
            return StorageRequestType.SAVE;
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

        @Override
        @StorageRequestType
        int getStorageRequestType() {
            return StorageRequestType.DELETE;
        }
    }

    /**
     * Request to restore saved serialized {@link PersistedTabData}
     */
    protected class FileRestoreRequest extends StorageRequest<byte[]> {
        protected Callback<byte[]> mCallback;

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
                long startTime = SystemClock.elapsedRealtime();
                AtomicFile atomicFile = new AtomicFile(mFile);
                res = atomicFile.readFully();
                success = true;
                RecordHistogram.recordTimesHistogram(
                        String.format(Locale.US, "Tabs.PersistedTabData.Storage.LoadTime.%s",
                                getUmaTag()),
                        SystemClock.elapsedRealtime() - startTime);
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

        @Override
        @StorageRequestType
        int getStorageRequestType() {
            return StorageRequestType.RESTORE;
        }
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    @IntDef({StorageRequestType.SAVE, StorageRequestType.RESTORE, StorageRequestType.DELETE})
    @Retention(RetentionPolicy.SOURCE)
    @interface StorageRequestType {
        int SAVE = 0;
        int RESTORE = 1;
        int DELETE = 2;
        int NUM_ENTRIES = 3;
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void processNextItemOnQueue() {
        if (mQueue.isEmpty()) return;
        StorageRequest storageRequest = mQueue.poll();
        // First operation should be a restore (to restore the active tab) - any other
        // operations coming in before the restore will block restoration of the active
        // tab and hurt startup latency.
        if (!mFirstOperationRecorded) {
            RecordHistogram.recordEnumeratedHistogram("Tabs.PersistedTabData.Storage.Save."
                            + getUmaTag() + ".FirstStorageRequestType",
                    storageRequest.getStorageRequestType(), StorageRequestType.NUM_ENTRIES);
            mFirstOperationRecorded = true;
        }
        storageRequest.getAsyncTask().executeOnTaskRunner(mSequencedTaskRunner);
    }

    @Override
    public String getUmaTag() {
        return "File";
    }

    /**
     * Determines if a {@link Tab} is incognito or not based on the existence of the
     * corresponding {@link CriticalPersistedTabData} file. This involves a disk access
     * and will be slow. This method can be called from the UI thread.
     * @param tabId identifier for the {@link Tab}
     * @return true/false if the {@link Tab} is incognito based on the existence of the
     *         CriticalPersistedTabData file and null if it is not known if the
     *         {@link Tab} is incognito or not.
     */
    public static Boolean isIncognito(int tabId) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            String regularId =
                    PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, false)
                            .getId();
            File regularFile = FilePersistedTabDataStorage.getFile(tabId, regularId);
            if (regularFile.exists()) {
                return false;
            }
            String incognitoId =
                    PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, true).getId();
            File incognitoFile = FilePersistedTabDataStorage.getFile(tabId, incognitoId);
            if (incognitoFile.exists()) {
                return true;
            }
            return null;
        }
    }
}
