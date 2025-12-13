// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_import;

import android.os.ParcelFileDescriptor;

import androidx.annotation.VisibleForTesting;

import com.google.protobuf.ByteString;

import io.grpc.Context;
import io.grpc.Status;
import io.grpc.StatusRuntimeException;
import io.grpc.stub.StreamObserver;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.profiles.ProfileManager;

import java.util.HashMap;
import java.util.Map;

import javax.annotation.concurrent.GuardedBy;

/** Implements the gRPC service for importing user data from other browsers. */
@NullMarked
public class TargetService extends TargetServiceGrpc.TargetServiceImplBase {
    private static final String TAG = "TargetService";

    // Helper "struct" to accumulate import results for a given session ID, to be returned to
    // the API caller in `importItemsDone()`.
    static class ImportResults {
        // Note: The counts are in number of files, *not* number of individual entries.
        public int successItemCount;
        public int failedItemCount;
        public int ignoredItemCount;

        // Used to record the time it took to import a BrowserFileType.
        public Map<BrowserFileType, Long> startTimes = new HashMap<>();
    }

    @GuardedBy("mPendingImportsLock")
    private final Map<ByteString, ImportResults> mPendingImports = new HashMap<>();

    private final Object mPendingImportsLock = new Object();

    // Must only be created and used on the UI thread.
    @VisibleForTesting @Nullable DataImporterBridge mBridge;

    @Override
    public void handshake(
            TargetHandshakeRequest request,
            StreamObserver<TargetHandshakeResponse> responseObserver) {
        TargetHandshakeResponse.Builder response = TargetHandshakeResponse.newBuilder();
        switch (request.getItemType()) {
            case SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA:
                response.setSupported(true);
                response.setDataFormatVersion(1);
                break;
            case SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED:
            case UNRECOGNIZED:
                response.setSupported(false);
                break;
        }
        ByteString sessionId = request.getSessionId();
        if (sessionId.isEmpty()) {
            responseObserver.onError(
                    new StatusRuntimeException(
                            Status.INVALID_ARGUMENT.withDescription("Missing session_id")));
            return;
        }
        // Note: No need to actually do anything with the `sessionId` here.

        responseObserver.onNext(response.build());
        responseObserver.onCompleted();
    }

    @Override
    public void importItem(
            ImportItemRequest request, StreamObserver<ImportItemResponse> responseObserver) {
        switch (request.getItemType()) {
            case SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA:
                // Supported type - continue below.
                break;
            case SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED:
            case UNRECOGNIZED:
                responseObserver.onError(
                        new StatusRuntimeException(
                                Status.INVALID_ARGUMENT.withDescription(
                                        "Invalid or unsupported item type")));
                return;
        }

        ByteString sessionId = request.getSessionId();
        if (sessionId.isEmpty()) {
            responseObserver.onError(
                    new StatusRuntimeException(
                            Status.INVALID_ARGUMENT.withDescription("Missing session_id")));
            return;
        }
        synchronized (mPendingImportsLock) {
            ImportResults importResults = mPendingImports.get(sessionId);
            if (importResults == null) {
                importResults = new ImportResults();
                mPendingImports.put(sessionId, importResults);
            }
        }

        ParcelFileDescriptor pfd = DataImporterServiceImpl.PFD_CONTEXT_KEY.get(Context.current());
        if (pfd == null) {
            responseObserver.onError(
                    new StatusRuntimeException(
                            Status.INVALID_ARGUMENT.withDescription(
                                    "Missing ParcelFileDescriptor")));
            return;
        }

        BrowserFileType fileType;
        try {
            BrowserFileMetadata fileMetadata =
                    BrowserFileMetadata.parseFrom(request.getFileMetadata().getValue());
            fileType = fileMetadata.getFileType();
        } catch (com.google.protobuf.InvalidProtocolBufferException e) {
            responseObserver.onError(
                    new StatusRuntimeException(
                            Status.INVALID_ARGUMENT.withDescription(
                                    "Invalid or missing file_metadata")));
            return;
        }

        recordImportStartMetrics(sessionId, fileType);

        switch (fileType) {
            case BROWSER_FILE_TYPE_BOOKMARKS:
                {
                    // Note: Use `detachFd()` (rather than `getFd()`) in order to pass ownership
                    // to the native side.
                    final StreamObserver<ImportItemResponse> responseObserverForUiThread =
                            responseObserver;
                    // `responseObserver` is now "owned by" the UI thread, so must not be used
                    // here anymore.
                    responseObserver = null;
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () ->
                                    importBookmarksOnUiThread(
                                            sessionId,
                                            pfd.detachFd(),
                                            responseObserverForUiThread));
                    return;
                }
            case BROWSER_FILE_TYPE_READING_LIST:
                {
                    // Note: Use `detachFd()` (rather than `getFd()`) in order to pass ownership
                    // to the native side.
                    final StreamObserver<ImportItemResponse> responseObserverForUiThread =
                            responseObserver;
                    // `responseObserver` is now "owned by" the UI thread, so must not be used
                    // here anymore.
                    responseObserver = null;
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () ->
                                    importReadingListOnUiThread(
                                            sessionId,
                                            pfd.detachFd(),
                                            responseObserverForUiThread));
                    return;
                }
            case BROWSER_FILE_TYPE_BROWSING_HISTORY:
                {
                    // Note: Use `detachFd()` (rather than `getFd()`) in order to pass ownership
                    // to the native side.
                    final StreamObserver<ImportItemResponse> responseObserverForUiThread =
                            responseObserver;
                    // `responseObserver` is now "owned by" the UI thread, so must not be used
                    // here anymore.
                    responseObserver = null;
                    PostTask.postTask(
                            TaskTraits.UI_DEFAULT,
                            () ->
                                    importHistoryOnUiThread(
                                            sessionId,
                                            pfd.detachFd(),
                                            responseObserverForUiThread));
                    return;
                }
            case UNRECOGNIZED:
            case BROWSER_FILE_TYPE_UNSPECIFIED:
                responseObserver.onError(
                        new StatusRuntimeException(
                                Status.INVALID_ARGUMENT.withDescription(
                                        "Invalid or unrecognized file type")));
                return;
        }
    }

    @Override
    public void importItemsDone(
            ImportItemsDoneRequest request,
            StreamObserver<ImportItemsDoneResponse> responseObserver) {
        switch (request.getItemType()) {
            case SYSTEM_APP_API_ITEM_TYPE_BROWSER_DATA:
                // Supported type - continue below.
                break;
            case SYSTEM_APP_API_ITEM_TYPE_UNSPECIFIED:
            case UNRECOGNIZED:
                responseObserver.onError(
                        new StatusRuntimeException(
                                Status.INVALID_ARGUMENT.withDescription(
                                        "Invalid or unsupported item type")));
                return;
        }

        ByteString sessionId = request.getSessionId();
        if (sessionId.isEmpty()) {
            responseObserver.onError(
                    new StatusRuntimeException(
                            Status.INVALID_ARGUMENT.withDescription("Missing session_id")));
            return;
        }
        ImportItemsDoneResponse.Builder responseBuilder = ImportItemsDoneResponse.newBuilder();
        synchronized (mPendingImportsLock) {
            ImportResults importResults = mPendingImports.get(sessionId);
            if (importResults == null) {
                responseObserver.onError(
                        new StatusRuntimeException(
                                Status.INVALID_ARGUMENT.withDescription("Unknown session_id")));
                return;
            }

            responseBuilder
                    .setSuccessItemCount(importResults.successItemCount)
                    .setFailedItemCount(importResults.failedItemCount)
                    .setIgnoredItemCount(importResults.ignoredItemCount);

            // This import session is completed; clean up the corresponding stats.
            mPendingImports.remove(sessionId);

            // If this was the last (most likely, only) import session ongoing, the bridge isn't
            // needed anymore
            if (mPendingImports.isEmpty()) {
                PostTask.postTask(TaskTraits.UI_DEFAULT, this::destroyBridgeOnUiThread);
            }
        }

        responseObserver.onNext(responseBuilder.build());
        responseObserver.onCompleted();
    }

    private void prepareForImport() {
        ThreadUtils.assertOnUiThread();

        // The browser must be initialized before accessing ProfileManager or calling into
        // native.
        ChromeBrowserInitializer browserInitializer = ChromeBrowserInitializer.getInstance();
        if (!browserInitializer.isFullBrowserInitialized()) {
            ChromeBrowserInitializer.getInstance().handleSynchronousStartup();
        }
        assert ProfileManager.isInitialized();

        if (mBridge == null) {
            mBridge = new DataImporterBridge(ProfileManager.getLastUsedRegularProfile());
        }
    }

    private void importBookmarksOnUiThread(
            ByteString sessionId,
            int ownedFd,
            StreamObserver<ImportItemResponse> responseObserver) {
        ThreadUtils.assertOnUiThread();

        prepareForImport();
        assert mBridge != null;

        mBridge.importBookmarks(
                ownedFd,
                (count) -> {
                    recordImportDoneMetrics(
                            sessionId, BrowserFileType.BROWSER_FILE_TYPE_BOOKMARKS, count);
                    Log.i(TAG, "Bookmarks imported: %d", count);
                    updateImportResults(sessionId, count);
                    responseObserver.onNext(ImportItemResponse.newBuilder().build());
                    responseObserver.onCompleted();
                });
    }

    private void importReadingListOnUiThread(
            ByteString sessionId,
            int ownedFd,
            StreamObserver<ImportItemResponse> responseObserver) {
        ThreadUtils.assertOnUiThread();

        prepareForImport();
        assert mBridge != null;

        mBridge.importReadingList(
                ownedFd,
                (count) -> {
                    recordImportDoneMetrics(
                            sessionId, BrowserFileType.BROWSER_FILE_TYPE_READING_LIST, count);
                    Log.i(TAG, "ReadingList imported: %d", count);
                    updateImportResults(sessionId, count);
                    responseObserver.onNext(ImportItemResponse.newBuilder().build());
                    responseObserver.onCompleted();
                });
    }

    private void importHistoryOnUiThread(
            ByteString sessionId,
            int ownedFd,
            StreamObserver<ImportItemResponse> responseObserver) {
        ThreadUtils.assertOnUiThread();

        prepareForImport();
        assert mBridge != null;

        mBridge.importHistory(
                ownedFd,
                (count) -> {
                    recordImportDoneMetrics(
                            sessionId, BrowserFileType.BROWSER_FILE_TYPE_BROWSING_HISTORY, count);
                    Log.i(TAG, "History imported: %d", count);
                    updateImportResults(sessionId, count);
                    responseObserver.onNext(ImportItemResponse.newBuilder().build());
                    responseObserver.onCompleted();
                });
    }

    private void destroyBridgeOnUiThread() {
        ThreadUtils.assertOnUiThread();

        if (mBridge != null) {
            mBridge.destroy();
            mBridge = null;
        }
    }

    private String getHistogramSuffix(BrowserFileType fileType) {
        switch (fileType) {
            case BROWSER_FILE_TYPE_BOOKMARKS:
                return "Bookmarks";
            case BROWSER_FILE_TYPE_READING_LIST:
                return "ReadingList";
            case BROWSER_FILE_TYPE_BROWSING_HISTORY:
                return "History";
            case UNRECOGNIZED:
            case BROWSER_FILE_TYPE_UNSPECIFIED:
                return "NotSupported";
            default:
                assert false;
                return "";
        }
    }

    private void recordImportStartMetrics(ByteString sessionId, BrowserFileType fileType) {
        // Record `False` to report the `Started` bucket.
        RecordHistogram.recordBooleanHistogram(
                "UserDataImporter.OSMigration." + getHistogramSuffix(fileType) + ".Flow", false);

        synchronized (mPendingImportsLock) {
            // Save the start time of the import for the datatype.
            ImportResults importResults = mPendingImports.get(sessionId);
            assert (importResults != null);
            importResults.startTimes.put(fileType, System.currentTimeMillis());
        }
    }

    private long getImportDuration(ByteString sessionId, BrowserFileType fileType) {
        synchronized (mPendingImportsLock) {
            ImportResults importResults = mPendingImports.get(sessionId);
            assert (importResults != null);
            Long startTime = importResults.startTimes.get(fileType);
            assert (startTime != null);
            return System.currentTimeMillis() - startTime;
        }
    }

    private void recordImportDoneMetrics(
            ByteString sessionId, BrowserFileType fileType, int count) {
        RecordHistogram.recordCount1000Histogram(
                "UserDataImporter.OSMigration." + getHistogramSuffix(fileType) + ".ImportedCount",
                count);
        RecordHistogram.recordTimesHistogram(
                "UserDataImporter.OSMigration." + getHistogramSuffix(fileType) + ".FlowDuration",
                getImportDuration(sessionId, fileType));

        // Record `True` to report the `Completed` bucket.
        RecordHistogram.recordBooleanHistogram(
                "UserDataImporter.OSMigration." + getHistogramSuffix(fileType) + ".Flow", true);
    }

    private void updateImportResults(ByteString sessionId, int count) {
        synchronized (mPendingImportsLock) {
            ImportResults importResults = mPendingImports.get(sessionId);
            assert (importResults != null);
            if (count >= 0) {
                importResults.successItemCount++;
            } else {
                importResults.failedItemCount++;
            }
        }
    }
}
