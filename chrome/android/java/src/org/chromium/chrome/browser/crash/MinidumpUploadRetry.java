// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.crash;

import android.annotation.SuppressLint;
import android.content.Context;

import org.chromium.base.NonThreadSafe;
import org.chromium.base.task.PostTask;
import org.chromium.components.minidump_uploader.util.CrashReportingPermissionManager;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.net.ConnectionType;
import org.chromium.net.NetworkChangeNotifier;

/**
 * This class listens to network changes and determine when it would good to
 * retry uploading minidumps.
 */
class MinidumpUploadRetry implements NetworkChangeNotifier.ConnectionTypeObserver {
    private final Context mContext;
    private final CrashReportingPermissionManager mPermissionManager;
    @SuppressLint("StaticFieldLeak")
    private static MinidumpUploadRetry sSingleton;

    private static class Scheduler implements Runnable {
        private static NonThreadSafe sThreadCheck;
        private final Context mContext;
        private final CrashReportingPermissionManager mPermissionManager;

        private Scheduler(Context context, CrashReportingPermissionManager permissionManager) {
            this.mContext = context;
            mPermissionManager = permissionManager;
        }

        @Override
        public void run() {
            if (sThreadCheck == null) {
                sThreadCheck = new NonThreadSafe();
            }
            // Make sure this is called on the same thread all the time.
            assert sThreadCheck.calledOnValidThread();
            if (!NetworkChangeNotifier.isInitialized()) {
                return;
            }
            if (sSingleton == null) {
                sSingleton = new MinidumpUploadRetry(mContext, mPermissionManager);
            }
        }
    }

    /**
     * Schedule a retry. If there is already one schedule, this is NO-OP.
     */
    static void scheduleRetry(Context context, CrashReportingPermissionManager permissionManager) {
        // NetworkChangeNotifier is not thread safe. We will post to UI thread
        // instead since that's where it fires off notification changes.
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Scheduler(context, permissionManager));
    }

    private MinidumpUploadRetry(
            Context context, CrashReportingPermissionManager permissionManager) {
        this.mContext = context;
        this.mPermissionManager = permissionManager;
        NetworkChangeNotifier.addConnectionTypeObserver(this);
    }

    @Override
    public void onConnectionTypeChanged(int connectionType) {
        // Early-out if not connected at all - to avoid checking the current network state.
        if (connectionType == ConnectionType.CONNECTION_NONE) {
            return;
        }
        if (!mPermissionManager.isNetworkAvailableForCrashUploads()) {
            return;
        }
        MinidumpUploadService.tryUploadAllCrashDumps();
        NetworkChangeNotifier.removeConnectionTypeObserver(this);
        sSingleton = null;
    }
}
