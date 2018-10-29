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

import org.chromium.base.task.AsyncTask;

import java.util.ArrayList;
import java.util.HashMap;

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

        public IBinder getService() {
            return mBinder;
        }

        public void addCallback(ConnectionCallback callback) {
            mCallbacks.add(callback);
        }

        public Connection(WebApkServiceConnectionManager manager) {
            mConnectionManager = manager;
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

    private static final String TAG = "cr_WebApkService";

    /** The category of the service to connect to. */
    private String mCategory;

    /** The action of the service to connect to. */
    private String mAction;

    /** Mapping of WebAPK package to WebAPK service connection. */
    private HashMap<String, Connection> mConnections = new HashMap<>();

    /** Called when a WebAPK service connection is disconnected. */
    private void onServiceDisconnected(String webApkName) {
        mConnections.remove(webApkName);
    }

    /**
     * Connects Chrome application to WebAPK service. Can be called from any thread.
     *
     * @param appContext Application context.
     * @param webApkPackage WebAPK package to create connection for.
     * @param callback Callback to call after connection has been established. Called synchronously
     */
    @SuppressLint("StaticFieldLeak")
    public void connect(final Context appContext, final String webApkPackage,
            final ConnectionCallback callback) {
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

        new AsyncTask<Connection>() {
            @Override
            protected Connection doInBackground() {
                Connection newConnection = new Connection(WebApkServiceConnectionManager.this);
                newConnection.addCallback(callback);
                Intent intent = createConnectIntent(webApkPackage);
                try {
                    if (appContext.bindService(intent, newConnection, Context.BIND_AUTO_CREATE)) {
                        return newConnection;
                    } else {
                        appContext.unbindService(newConnection);
                        return null;
                    }
                } catch (SecurityException e) {
                    Log.w(TAG, "Security failed binding.", e);
                    return null;
                }
            }

            @Override
            protected void onPostExecute(Connection connection) {
                if (connection == null) {
                    callback.onConnected(null);
                } else {
                    mConnections.put(webApkPackage, connection);
                }
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Disconnect from all of the WebAPK services. Can be called from any thread.
     *
     * @param appContext The application context.
     */
    @SuppressLint("StaticFieldLeak")
    public void disconnectAll(final Context appContext) {
        if (mConnections.isEmpty()) return;

        final Connection[] values =
                mConnections.values().toArray(new Connection[mConnections.size()]);
        mConnections.clear();

        new AsyncTask<Void>() {
            @Override
            protected final Void doInBackground() {
                for (Connection connection : values) {
                    if (connection.getService() != null) {
                        appContext.unbindService(connection);
                    }
                }
                return null;
            }
        }
                .executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    public WebApkServiceConnectionManager(String category, String action) {
        mCategory = category;
        mAction = action;
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
}
