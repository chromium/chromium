// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.evaluation;

import org.jni_zero.CalledByNative;
import org.jni_zero.JNINamespace;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.Callback;
import org.chromium.base.Log;
import org.chromium.base.ObserverList;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.offlinepages.ClientId;
import org.chromium.chrome.browser.offlinepages.OfflinePageItem;
import org.chromium.chrome.browser.offlinepages.SavePageRequest;
import org.chromium.chrome.browser.profiles.Profile;

import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.io.OutputStreamWriter;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/** Class used for offline page evaluation testing tools. */
@JNINamespace("offline_pages::android")
public class OfflinePageEvaluationBridge {
    /**
     * Observer class for notifications on changes to save page requests or offline page model which
     * are used for testing.
     */
    public abstract static class OfflinePageEvaluationObserver {
        /** Event fired when the offline page model is loaded. */
        public void offlinePageModelLoaded() {}

        /**
         * Event fired when a new request is added.
         * @param request The newly added save page request.
         */
        public void savePageRequestAdded(SavePageRequest request) {}

        /**
         * Event fired when a request is completed.
         * @param request The completed request.
         * @param status The status of the completion, see
         * org.chromium.components.offlinepages.BackgroundSavePageResult.
         */
        public void savePageRequestCompleted(SavePageRequest request, int status) {}

        /**
         * Event fired when a new request is changed.
         * @param request The changed request.
         */
        public void savePageRequestChanged(SavePageRequest request) {}
    }

    /**
     * Get the instance of the evaluation bridge.
     * @param profile The profile used to get bridge.
     * @param useEvaluationScheduler True if using the evaluation scheduler instead of the
     *                               GCMNetworkManager one.
     */
    public OfflinePageEvaluationBridge(Profile profile, boolean useEvaluationScheduler) {
        ThreadUtils.assertOnUiThread();
        mNativeOfflinePageEvaluationBridge =
                OfflinePageEvaluationBridgeJni.get()
                        .createBridgeForProfile(
                                OfflinePageEvaluationBridge.this, profile, useEvaluationScheduler);
    }

    private static final String TAG = "OPEvalBridge";

    private static TaskRunner sSequencedTaskRunner =
            PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);

    private long mNativeOfflinePageEvaluationBridge;
    private boolean mIsOfflinePageModelLoaded;
    private ObserverList<OfflinePageEvaluationObserver> mObservers =
            new ObserverList<OfflinePageEvaluationObserver>();

    private OutputStreamWriter mLogOutput;

    /** Destroys the native portion of the bridge. */
    public void destroy() {
        if (mNativeOfflinePageEvaluationBridge != 0) {
            OfflinePageEvaluationBridgeJni.get()
                    .destroy(mNativeOfflinePageEvaluationBridge, OfflinePageEvaluationBridge.this);
            mNativeOfflinePageEvaluationBridge = 0;
            mIsOfflinePageModelLoaded = false;
        }
        mObservers.clear();
    }

    /** Add an observer of the evaluation events. */
    public void addObserver(OfflinePageEvaluationObserver observer) {
        mObservers.addObserver(observer);
    }

    /** Remove an observer of evaluation events. */
    public void removeObserver(OfflinePageEvaluationObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Gets all pages in offline page model.
     * @param callback The callback would be invoked after the action completes and return with a
     *                 list of pages.
     */
    public void getAllPages(final Callback<List<OfflinePageItem>> callback) {
        List<OfflinePageItem> result = new ArrayList<>();
        OfflinePageEvaluationBridgeJni.get()
                .getAllPages(
                        mNativeOfflinePageEvaluationBridge,
                        OfflinePageEvaluationBridge.this,
                        result,
                        callback);
    }

    /**
     * Saves a url as offline page async.
     * @param url The url of the web page.
     * @param namespace The namespace to which the page belongs.
     * @param userRequest True if it's user-requested page.
     */
    public void savePageLater(final String url, final String namespace, boolean userRequested) {
        ClientId clientId = ClientId.createGuidClientIdForNamespace(namespace);
        OfflinePageEvaluationBridgeJni.get()
                .savePageLater(
                        mNativeOfflinePageEvaluationBridge,
                        OfflinePageEvaluationBridge.this,
                        url,
                        namespace,
                        clientId.getId(),
                        userRequested);
    }

    /**
     * Forces request coordinator to process the requests in the queue.
     * @param callback The callback would be invoked after the operation completes.
     * @return True if processing starts successfully and callback is expected to be called, false
     * otherwise.
     */
    public boolean pushRequestProcessing(final Callback<Boolean> callback) {
        return OfflinePageEvaluationBridgeJni.get()
                .pushRequestProcessing(
                        mNativeOfflinePageEvaluationBridge,
                        OfflinePageEvaluationBridge.this,
                        callback);
    }

    /**
     * Gets all requests in the queue.
     * @param callback The callback would be invoked with a list of requests which are in the queue.
     */
    public void getRequestsInQueue(Callback<SavePageRequest[]> callback) {
        OfflinePageEvaluationBridgeJni.get()
                .getRequestsInQueue(
                        mNativeOfflinePageEvaluationBridge,
                        OfflinePageEvaluationBridge.this,
                        callback);
    }

    /**
     * Removes requests from the queue by request ids.
     * @param requestIds The list of request ids to be deleted.
     * @param callback The callback would be invoked with number of successfully deleted ids.
     */
    public void removeRequestsFromQueue(List<Long> requestIds, Callback<Integer> callback) {
        long[] ids = new long[requestIds.size()];
        for (int i = 0; i < requestIds.size(); i++) {
            ids[i] = requestIds.get(i);
        }
        OfflinePageEvaluationBridgeJni.get()
                .removeRequestsFromQueue(
                        mNativeOfflinePageEvaluationBridge,
                        OfflinePageEvaluationBridge.this,
                        ids,
                        callback);
    }

    public void setLogOutputFile(File outputFile) throws IOException {
        // This open file operation shouldn't happen on UI thread.
        assert !ThreadUtils.runningOnUiThread();
        mLogOutput = new FileWriter(outputFile);
    }

    /**
     * @return True if the offline page model has fully loaded.
     */
    public boolean isOfflinePageModelLoaded() {
        return mIsOfflinePageModelLoaded;
    }

    @CalledByNative
    public void log(String sourceTag, String message) {
        Date date = new Date(System.currentTimeMillis());
        SimpleDateFormat formatter =
                new SimpleDateFormat("MM-dd HH:mm:ss.SSS", Locale.getDefault());
        String logString =
                formatter.format(date)
                        + ": "
                        + sourceTag
                        + " | "
                        + message
                        + System.getProperty("line.separator");
        Log.d(TAG, logString);
        sSequencedTaskRunner.execute(
                () -> {
                    try {
                        mLogOutput.write(logString);
                        mLogOutput.flush();
                    } catch (IOException e) {
                        Log.e(TAG, e.getMessage(), e);
                    }
                });
    }

    public void closeLog() {
        try {
            mLogOutput.close();
        } catch (IOException e) {
            Log.e(TAG, e.getMessage(), e);
        }
    }

    @CalledByNative
    void savePageRequestAdded(SavePageRequest request) {
        for (OfflinePageEvaluationObserver observer : mObservers) {
            observer.savePageRequestAdded(request);
        }
    }

    @CalledByNative
    void savePageRequestCompleted(SavePageRequest request, int status) {
        for (OfflinePageEvaluationObserver observer : mObservers) {
            observer.savePageRequestCompleted(request, status);
        }
    }

    @CalledByNative
    void savePageRequestChanged(SavePageRequest request) {
        for (OfflinePageEvaluationObserver observer : mObservers) {
            observer.savePageRequestChanged(request);
        }
    }

    @CalledByNative
    void offlinePageModelLoaded() {
        mIsOfflinePageModelLoaded = true;
        for (OfflinePageEvaluationObserver observer : mObservers) {
            observer.offlinePageModelLoaded();
        }
    }

    @CalledByNative
    private static void createOfflinePageAndAddToList(
            List<OfflinePageItem> offlinePagesList,
            String url,
            long offlineId,
            String clientNamespace,
            String clientId,
            String title,
            String filePath,
            long fileSize,
            long creationTime,
            int accessCount,
            long lastAccessTimeMs,
            String requestOrigin) {
        offlinePagesList.add(
                createOfflinePageItem(
                        url,
                        offlineId,
                        clientNamespace,
                        clientId,
                        title,
                        filePath,
                        fileSize,
                        creationTime,
                        accessCount,
                        lastAccessTimeMs,
                        requestOrigin));
    }

    // This is added as a utility method in the bridge because SavePageRequest_jni.h is supposed
    // only to be included in one bridge (OfflinePageBridge). So as a testing bridge, this will be
    // used to create SavePageRequest on the native side.
    @CalledByNative
    private static SavePageRequest createSavePageRequest(
            int state, long requestId, String url, String clientIdNamespace, String clientIdId) {
        return SavePageRequest.create(state, requestId, url, clientIdNamespace, clientIdId);
    }

    private static OfflinePageItem createOfflinePageItem(
            String url,
            long offlineId,
            String clientNamespace,
            String clientId,
            String title,
            String filePath,
            long fileSize,
            long creationTime,
            int accessCount,
            long lastAccessTimeMs,
            String requestOrigin) {
        return new OfflinePageItem(
                url,
                offlineId,
                clientNamespace,
                clientId,
                title,
                filePath,
                fileSize,
                creationTime,
                accessCount,
                lastAccessTimeMs,
                requestOrigin);
    }

    @NativeMethods
    interface Natives {
        long createBridgeForProfile(
                OfflinePageEvaluationBridge caller,
                @JniType("Profile*") Profile profile,
                boolean useEvaluationScheduler);

        void destroy(long nativeOfflinePageEvaluationBridge, OfflinePageEvaluationBridge caller);

        void getAllPages(
                long nativeOfflinePageEvaluationBridge,
                OfflinePageEvaluationBridge caller,
                List<OfflinePageItem> offlinePages,
                final Callback<List<OfflinePageItem>> callback);

        void savePageLater(
                long nativeOfflinePageEvaluationBridge,
                OfflinePageEvaluationBridge caller,
                String url,
                String clientNamespace,
                String clientId,
                boolean userRequested);

        boolean pushRequestProcessing(
                long nativeOfflinePageEvaluationBridge,
                OfflinePageEvaluationBridge caller,
                Callback<Boolean> callback);

        void getRequestsInQueue(
                long nativeOfflinePageEvaluationBridge,
                OfflinePageEvaluationBridge caller,
                final Callback<SavePageRequest[]> callback);

        void removeRequestsFromQueue(
                long nativeOfflinePageEvaluationBridge,
                OfflinePageEvaluationBridge caller,
                long[] requestIds,
                final Callback<Integer> callback);
    }
}
