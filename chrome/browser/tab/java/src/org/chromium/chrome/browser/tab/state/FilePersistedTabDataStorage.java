// Copyright 2020 The Chromium Authors
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
import org.chromium.base.ResettersForTesting;
import org.chromium.base.StreamUtil;
import org.chromium.base.StrictModeContext;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.SequencedTaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.flags.ChromeFeatureList;

import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.nio.ByteBuffer;
import java.nio.channels.FileChannel;
import java.nio.channels.FileChannel.MapMode;
import java.util.LinkedList;
import java.util.List;
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
    private static final String DELAY_SAVES_UNTIL_DEFERRED_STARTUP =
            "delay_saves_until_deferred_startup";
    public static final BooleanCachedFieldTrialParameter DELAY_SAVES_UNTIL_DEFERRED_STARTUP_PARAM =
            new BooleanCachedFieldTrialParameter(ChromeFeatureList.CRITICAL_PERSISTED_TAB_DATA,
                    DELAY_SAVES_UNTIL_DEFERRED_STARTUP, false);

    private SequencedTaskRunner mSequencedTaskRunner;
    private boolean mFirstOperationRecorded;
    private boolean mDeferredStartupComplete;

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected LinkedList<StorageRequest> mQueue = new LinkedList<>();
    private LinkedList<FileSaveRequest> mDelayedSaveRequests = new LinkedList<>();
    private FileSaveRequest mExecutingSaveRequest;

    protected FilePersistedTabDataStorage() {
        mSequencedTaskRunner =
                PostTask.createSequencedTaskRunner(TaskTraits.USER_BLOCKING_MAY_BLOCK);
    }

    @MainThread
    @Override
    public void save(int tabId, String dataId, Serializer<ByteBuffer> serializer) {
        save(tabId, dataId, serializer, NO_OP_CALLBACK);
    }

    // Callback used for test synchronization between save, restore and delete operations
    @MainThread
    @Override
    public void save(int tabId, String dataId, Serializer<ByteBuffer> serializer,
            Callback<Integer> callback) {
        // TODO(crbug.com/1059637) we should introduce a retry mechanisms
        if (isDelaySavesUntilDeferredStartup() && !mDeferredStartupComplete) {
            addSaveRequestToDelayedSaveQueue(
                    new FileSaveRequest(tabId, dataId, serializer, callback));
            return;
        }
        addSaveRequest(new FileSaveRequest(tabId, dataId, serializer, callback));
        processNextItemOnQueue();
    }

    @VisibleForTesting(otherwise = VisibleForTesting.PRIVATE)
    protected void addSaveRequest(FileSaveRequest fileSaveRequest) {
        // FileSaveRequest for the same tabid/data id will get overwritten
        // by new FileSaveRequest so remove if it exists in the queue.
        mQueue.remove(fileSaveRequest);
        mQueue.add(fileSaveRequest);
    }

    private void addSaveRequestToDelayedSaveQueue(FileSaveRequest fileSaveRequest) {
        // FileSaveRequest for the same tabid/data id will get overwritten
        // by new FileSaveRequest so remove if it exists in the queue.
        mDelayedSaveRequests.remove(fileSaveRequest);
        mDelayedSaveRequests.add(fileSaveRequest);
    }

    @MainThread
    @Override
    public void restore(int tabId, String dataId, Callback<ByteBuffer> callback) {
        addStorageRequestAndProcessNext(new FileRestoreRequest(tabId, dataId, callback));
    }

    @MainThread
    @Override
    public ByteBuffer restore(int tabId, String dataId) {
        return new FileRestoreRequest(tabId, dataId, null).executeSyncTask();
    }

    @MainThread
    @Override
    public <U extends PersistedTabDataResult> U restore(
            int tabId, String dataId, PersistedTabDataMapper<U> mapper) {
        return new FileRestoreAndMapRequest<U>(tabId, dataId, null, mapper).executeSyncTask();
    }

    @MainThread
    @Override
    public <U extends PersistedTabDataResult> void restore(
            int tabId, String dataId, Callback<U> callback, PersistedTabDataMapper<U> mapper) {
        addStorageRequestAndProcessNext(
                new FileRestoreAndMapRequest<U>(tabId, dataId, callback, mapper));
    }

    @MainThread
    @Override
    public void delete(int tabId, String dataId) {
        delete(tabId, dataId, NO_OP_CALLBACK);
    }

    public List<StorageRequest> getStorageRequestQueueForTesting() {
        return mQueue;
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

    protected void setExecutingSaveRequestForTesting(FileSaveRequest fileSaveRequest) {
        var oldValue = mExecutingSaveRequest;
        mExecutingSaveRequest = fileSaveRequest;
        ResettersForTesting.register(() -> mExecutingSaveRequest = oldValue);
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

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         */
        StorageRequest(int tabId, String dataId) {
            mTabId = tabId;
            mDataId = dataId;
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
                    && mDataId.equals(otherStorageRequest.mDataId);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + mTabId;
            result = 31 * result + mDataId.hashCode();
            return result;
        }

        /**
         * @return type of storage request (save, restore or delete)
         */
        abstract @StorageRequestType int getStorageRequestType();

        protected File getFile() {
            return FilePersistedTabDataStorage.getFile(mTabId, mDataId);
        }
    }

    /**
     * Request to save {@link PersistedTabData}
     */
    protected class FileSaveRequest extends StorageRequest<Void> {
        protected Serializer<ByteBuffer> mSerializer;
        protected Callback<Integer> mCallback;
        private boolean mFinished;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param serializer {@link Serializer} containing data to be saved
         */
        FileSaveRequest(int tabId, String dataId, Serializer<ByteBuffer> serializer,
                Callback<Integer> callback) {
            super(tabId, dataId);
            mSerializer = serializer;
            mCallback = callback;
        }

        @Override
        public Void executeSyncTask() {
            ByteBuffer data = null;
            try {
                data = mSerializer.get();
            } catch (OutOfMemoryError e) {
                // Log and exit FileSaveRequest early on OutOfMemoryError.
                // Not saving a Tab is better than crashing the app.
                Log.e(TAG, "OutOfMemoryError. Details: " + e.getMessage());
            }
            if (data == null) {
                mSerializer = null;
                return null;
            }
            FileOutputStream outputStream = null;
            AtomicFile atomicFile = null;
            boolean success = false;
            File file = getFile();
            try {
                long startTime = SystemClock.elapsedRealtime();
                atomicFile = new AtomicFile(file);
                outputStream = atomicFile.startWrite();
                FileChannel fileChannel = outputStream.getChannel();
                fileChannel.write(data);
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
                                file, e.getMessage()));
            } catch (IOException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "IOException while attempting to save for file %s. "
                                        + " Details: %s",
                                file, e.getMessage()));
            } finally {
                StreamUtil.closeQuietly(outputStream);
                if (atomicFile != null) {
                    if (success) {
                        atomicFile.finishWrite(outputStream);
                    } else {
                        atomicFile.failWrite(outputStream);
                    }
                }
            }
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Save." + getUmaTag(), success);
            mFinished = true;
            return null;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<Void>() {
                @Override
                protected void onPreExecute() {
                    mSerializer.preSerialize();
                }

                @Override
                protected Void doInBackground() {
                    return executeSyncTask();
                }

                @Override
                protected void onPostExecute(Void result) {
                    mExecutingSaveRequest = null;
                    PostTask.postTask(TaskTraits.UI_DEFAULT,
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

        public void preSerialize() {
            mSerializer.preSerialize();
        }

        public boolean isFinished() {
            return mFinished;
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
            File file = getFile();
            boolean exists = file.exists();
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Exists." + getUmaTag(), exists);
            if (!exists) {
                return null;
            }
            boolean success = file.delete();
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Delete." + getUmaTag(), success);
            if (!success) {
                Log.e(TAG, String.format(Locale.ENGLISH, "Error deleting file %s", file));
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
                    PostTask.postTask(TaskTraits.UI_DEFAULT,
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
    protected class FileRestoreRequest extends StorageRequest<ByteBuffer> {
        protected Callback<ByteBuffer> mCallback;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param callback - callback to return the retrieved serialized
         * {@link PersistedTabData} in
         */
        FileRestoreRequest(int tabId, String dataId, Callback<ByteBuffer> callback) {
            super(tabId, dataId);
            mCallback = callback;
        }

        @Override
        public ByteBuffer executeSyncTask() {
            boolean success = false;
            ByteBuffer res = null;
            FileInputStream fileInputStream = null;
            File file = getFile();
            try {
                long startTime = SystemClock.elapsedRealtime();
                AtomicFile atomicFile = new AtomicFile(file);
                fileInputStream = atomicFile.openRead();
                FileChannel channel = fileInputStream.getChannel();
                res = channel.map(MapMode.READ_ONLY, channel.position(), channel.size());
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
                                file, e.getMessage()));
            } catch (IOException e) {
                Log.e(TAG,
                        String.format(Locale.ENGLISH,
                                "IOException while attempting to restore "
                                        + "%s. Details: %s",
                                file, e.getMessage()));
            } finally {
                StreamUtil.closeQuietly(fileInputStream);
            }
            RecordHistogram.recordBooleanHistogram(
                    "Tabs.PersistedTabData.Storage.Restore." + getUmaTag(), success);
            return success ? res : null;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<ByteBuffer>() {
                @Override
                protected ByteBuffer doInBackground() {
                    return executeSyncTask();
                }

                @Override
                protected void onPostExecute(ByteBuffer res) {
                    PostTask.runOrPostTask(
                            TaskTraits.UI_DEFAULT, () -> { mCallback.onResult(res); });
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

    protected class FileRestoreAndMapRequest<U extends PersistedTabDataResult>
            extends StorageRequest<U> {
        protected Callback<U> mCallback;
        protected PersistedTabDataMapper<U> mMapper;

        /**
         * @param tabId identifier for the {@link Tab}
         * @param dataId identifier for the {@link PersistedTabData}
         * @param callback - callback to return the retrieved serialized
         * {@link PersistedTabData} in
         */
        FileRestoreAndMapRequest(
                int tabId, String dataId, Callback<U> callback, PersistedTabDataMapper<U> mapper) {
            super(tabId, dataId);
            mCallback = callback;
            mMapper = mapper;
        }

        @Override
        public U executeSyncTask() {
            long startTime = SystemClock.elapsedRealtime();
            ByteBuffer restoredData =
                    new FileRestoreRequest(mTabId, mDataId, null).executeSyncTask();
            long mapStartTime = SystemClock.elapsedRealtime();
            U mappedResult = mMapper.map(restoredData);
            long finishTime = SystemClock.elapsedRealtime();
            // Only loading and mapping a non-empty ByteBuffer should be recorded in
            // the metrics. Adding in a empty ByteBuffer will skew the metrics.
            if (restoredData != null && restoredData.limit() > 0) {
                RecordHistogram.recordTimesHistogram(
                        "Tabs.PersistedTabData.Storage.LoadAndMapTime.File",
                        finishTime - startTime);
                RecordHistogram.recordTimesHistogram(
                        "Tabs.PersistedTabData.Storage.MapTime.File", finishTime - mapStartTime);
            }
            return mappedResult;
        }

        @Override
        public AsyncTask getAsyncTask() {
            return new AsyncTask<U>() {
                @Override
                protected U doInBackground() {
                    return executeSyncTask();
                }

                @Override
                protected void onPostExecute(U res) {
                    PostTask.postTask(TaskTraits.UI_DEFAULT, () -> { mCallback.onResult(res); });
                    processNextItemOnQueue();
                }
            };
        }
        @Override
        public boolean equals(Object other) {
            if (!(other instanceof FileRestoreAndMapRequest)) return false;
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
        if (storageRequest instanceof FileSaveRequest) {
            mExecutingSaveRequest = (FileSaveRequest) storageRequest;
        }
        storageRequest.getAsyncTask().executeOnTaskRunner(mSequencedTaskRunner);
    }

    @Override
    public String getUmaTag() {
        return "File";
    }

    @Override
    public void performMaintenance(List<Integer> tabIds, String dataId) {
        assert false : "Maintenance is not available in FilePersistedTabDataStorage";
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

    /**
     * @param tabId {@link Tab} identifier
     * @param isIncognito if the {@link Tab} is incognito
     * @return true if a file exists for this {@link Tab}
     */
    @VisibleForTesting
    public static boolean exists(int tabId, boolean isIncognito) {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            String dataId =
                    PersistedTabDataConfiguration.get(CriticalPersistedTabData.class, isIncognito)
                            .getId();
            File file = FilePersistedTabDataStorage.getFile(tabId, dataId);
            return file != null && file.exists();
        }
    }

    /**
     * Used for cleaning up files between batched tests.
     */
    protected static void deleteFilesForTesting() {
        for (File file : getOrCreateBaseStorageDirectory().listFiles()) {
            file.delete();
        }
    }

    private static boolean isDelaySavesUntilDeferredStartup() {
        return ChromeFeatureList.sCriticalPersistedTabData.isEnabled()
                && DELAY_SAVES_UNTIL_DEFERRED_STARTUP_PARAM.getValue();
    }

    /**
     * Signal to {@link FilePersistedTabDataStorage} that deferred startup
     * is complete.
     */
    protected void onDeferredStartup() {
        mDeferredStartupComplete = true;
        for (FileSaveRequest saveRequest : mDelayedSaveRequests) {
            addSaveRequest(saveRequest);
            processNextItemOnQueue();
        }
        mDelayedSaveRequests.clear();
    }

    public LinkedList<FileSaveRequest> getDelayedSaveRequestsForTesting() {
        return mDelayedSaveRequests;
    }

    /**
     * System is shutting down - finish any pending saves.
     */
    public void onShutdown() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
            // Cancel existing request on background thread and finish
            // all saves.
            if (mExecutingSaveRequest != null) {
                mExecutingSaveRequest.getAsyncTask().cancel(false);
                if (!mExecutingSaveRequest.isFinished()) {
                    mQueue.addFirst(mExecutingSaveRequest);
                }
            }
            for (StorageRequest storageRequest : mQueue) {
                if (storageRequest instanceof FileSaveRequest) {
                    ((FileSaveRequest) storageRequest).preSerialize();
                    storageRequest.executeSyncTask();
                }
            }
            mQueue.clear();
        }
    }
}
