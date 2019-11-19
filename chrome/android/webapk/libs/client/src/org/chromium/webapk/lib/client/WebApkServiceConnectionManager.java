// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import android.annotation.SuppressLint;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;
import android.util.Log;

import org.chromium.base.Callback;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskRunner;
import org.chromium.base.task.TaskTraits;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.concurrent.Callable;

/**
 * Each WebAPK has several services. This class manages static global connections between the
 * Chrome application and the "WebAPK services."
 */
public class WebApkServiceConnectionManager {
    /**
     * Interface for getting notified once Chrome is connected to a WebAPK service.
     */
    public interface ConnectionCallback {
        /**
         * Called once Chrome is connected to the WebAPK service.
         *
         * @param service The WebAPK service.
         */
        void onConnected(IBinder service);
    }

    /** Managed connection to WebAPK service. */
    private static class Connection implements ServiceConnection {
        /** The connection manager who owns this connection. */
        private WebApkServiceConnectionManager mConnectionManager;

        /** Callbacks to call once the connection is established. */
        private ArrayList<ConnectionCallback> mCallbacks = new ArrayList<>();

        /** WebAPK IBinder interface. */
        private IBinder mBinder;

        public Connection(WebApkServiceConnectionManager manager) {
            mConnectionManager = manager;
        }

        public IBinder getService() {
            return mBinder;
        }

        public void addCallback(ConnectionCallback callback) {
            mCallbacks.add(callback);
        }

        public boolean didAllCallbacksRun() {
            return mCallbacks.isEmpty();
        }

        @Override
        public void onServiceDisconnected(ComponentName name) {
            mBinder = null;
            mConnectionManager.onServiceDisconnected(name.getPackageName());
        }

        @Override
        public void onServiceConnected(ComponentName name, IBinder service) {
            mBinder = service;
            Log.d(TAG, String.format("Got IBinder Service: %s", mBinder));
            for (ConnectionCallback callback : mCallbacks) {
                callback.onConnected(mBinder);
            }
            mCallbacks.clear();
        }
    }

    private static final String TAG = "WebApkService";

    /** The category of the service to connect to. */
    private String mCategory;

    /** The action of the service to connect to. */
    private String mAction;

    private TaskTraits mUiThreadTaskTraits;

    private TaskRunner mTaskRunner;

    /** Number of tasks posted via {@link #postTaskAndReply()} whose reply has not yet been run. */
    private int mNumPendingPostedTasks;

    /** Mapping of WebAPK package to WebAPK service connection. */
    private HashMap<String, Connection> mConnections = new HashMap<>();

    public WebApkServiceConnectionManager(
            TaskTraits uiThreadTaskTraits, String category, String action) {
        mUiThreadTaskTraits = uiThreadTaskTraits;
        mCategory = category;
        mAction = action;
    }

    /** Called when a WebAPK service connection is disconnected. */
    private void onServiceDisconnected(String webApkName) {
        mConnections.remove(webApkName);
        if (mConnections.isEmpty() && mNumPendingPostedTasks == 0) {
            destroyTaskRunner();
        }
    }

    /** Returns whether the callbacks for all of the {@link #connect()} calls have been run. */
    public boolean didAllConnectCallbacksRun() {
        for (Connection connection : mConnections.values()) {
            if (!connection.didAllCallbacksRun()) return false;
        }
        return true;
    }

    /**
     * Connects Chrome application to WebAPK service. Can be called from any thread.
     *
     * @param appContext Application context.
     * @param webApkPackage WebAPK package to create connection for.
     * @param callback Callback to call after connection has been established. Called synchronously
     */
    @SuppressLint("StaticFieldLeak")
    public void connect(
            final Context appContext, final String webApkPackage, ConnectionCallback callback) {
        Connection connection = mConnections.get(webApkPackage);
        if (connection != null) {
            IBinder service = connection.getService();
            if (service != null) {
                callback.onConnected(service);
            } else {
                connection.addCallback(callback);
            }
            return;
        }

        final Connection newConnection = new Connection(this);
        mConnections.put(webApkPackage, newConnection);
        newConnection.addCallback(callback);

        Callable<Boolean> backgroundTask = () -> {
            Intent intent = createConnectIntent(webApkPackage);
            try {
                if (appContext.bindService(intent, newConnection, Context.BIND_AUTO_CREATE)) {
                    return true;
                } else {
                    appContext.unbindService(newConnection);
                }
            } catch (SecurityException e) {
                Log.w(TAG, "Security exception binding.", e);
            }
            return false;
        };
        Callback<Boolean> uiThreadReply = (bindSuccessful) -> {
            if (!bindSuccessful) {
                newConnection.onServiceConnected(null, null);
            }
        };

        postTaskAndReply(backgroundTask, uiThreadReply);
    }

    /**
     * Disconnect from all of the WebAPK services. Can be called from any thread.
     *
     * @param appContext The application context.
     */
    @SuppressLint("StaticFieldLeak")
    public void disconnectAll(final Context appContext) {
        if (mConnections.isEmpty()) return;

        final Connection[] connectionsToDisconnect =
                mConnections.values().toArray(new Connection[mConnections.size()]);
        mConnections.clear();

        // Notify any waiting ConnectionCallbacks that the connection failed.
        for (Connection connectionToDisconnect : connectionsToDisconnect) {
            connectionToDisconnect.onServiceConnected(null, null);
        }

        Callable<Boolean> backgroundTask = () -> {
            for (Connection connectionToDisconnect : connectionsToDisconnect) {
                appContext.unbindService(connectionToDisconnect);
            }
            return true;
        };
        Callback<Boolean> uiThreadReply = (unused) -> {
            if (mConnections.isEmpty() && mNumPendingPostedTasks == 0) {
                destroyTaskRunner();
            }
        };

        postTaskAndReply(backgroundTask, uiThreadReply);
    }

    /**
     * Runs {@link backgroundTask} on the task runner. Calls {@link uiThreadReply} on the UI thread
     * with the result of running the background task.
     */
    private void postTaskAndReply(
            final Callable<Boolean> backgroundTask, final Callback<Boolean> uiThreadReply) {
        ++mNumPendingPostedTasks;
        getTaskRunner().postTask(() -> {
            Boolean result = false;
            try {
                result = backgroundTask.call();
            } catch (Exception e) {
            }

            final Boolean finalResult = result;
            PostTask.postTask(mUiThreadTaskTraits, () -> {
                --mNumPendingPostedTasks;
                uiThreadReply.onResult(finalResult);
            });
        });
    }

    /**
     * Creates intent to connect to WebAPK service.
     *
     * @param webApkPackage The package name of the WebAPK to connect to.
     */
    private Intent createConnectIntent(String webApkPackage) {
        Intent intent = new Intent();
        if (mCategory != null) intent.addCategory(mCategory);
        if (mAction != null) intent.setAction(mAction);
        intent.setPackage(webApkPackage);
        return intent;
    }

    private TaskRunner getTaskRunner() {
        if (mTaskRunner == null) {
            mTaskRunner = PostTask.createSequencedTaskRunner(TaskTraits.BEST_EFFORT_MAY_BLOCK);
        }
        return mTaskRunner;
    }

    private void destroyTaskRunner() {
        if (mTaskRunner == null) return;

        mTaskRunner.destroy();
        mTaskRunner = null;
    }
}
