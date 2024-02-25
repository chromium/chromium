// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Notification;
import android.content.ComponentName;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Bundle;
import android.os.RemoteException;

import androidx.browser.trusted.Token;
import androidx.browser.trusted.TrustedWebActivityCallback;
import androidx.browser.trusted.TrustedWebActivityServiceConnection;
import androidx.browser.trusted.TrustedWebActivityServiceConnectionPool;

import com.google.common.util.concurrent.ListenableFuture;

import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.browser.browserservices.TrustedWebActivityClient.ExecutionCallback;
import org.chromium.components.embedder_support.util.Origin;

import java.util.Set;
import java.util.concurrent.ExecutionException;
import java.util.concurrent.Executor;

/** Provides wrappers for AndroidX classes that can be mocked in tests. */
public class TrustedWebActivityClientWrappers {
    private static final String TAG = "TWAClient";
    private static final Executor UI_THREAD_EXECUTOR =
            (Runnable r) -> PostTask.postTask(TaskTraits.UI_USER_VISIBLE, r);

    /** Wrapper around {@link TrustedWebActivityServiceConnection}. */
    public interface Connection {
        /** See implementation on {@link TrustedWebActivityServiceConnection}. */
        ComponentName getComponentName();

        /** See implementation on {@link TrustedWebActivityServiceConnection}. */
        boolean areNotificationsEnabled(String channelName) throws RemoteException;

        /** See implementation on {@link TrustedWebActivityServiceConnection}. */
        boolean notify(
                String platformTag, int platformId, Notification notification, String channel)
                throws RemoteException;

        /** See implementation on {@link TrustedWebActivityServiceConnection}. */
        void cancel(String platformTag, int platformId) throws RemoteException;

        /** See implementation on {@link TrustedWebActivityServiceConnection}. */
        int getSmallIconId() throws RemoteException;

        /** See implementation on {@link TrustedWebActivityServiceConnection}. */
        Bitmap getSmallIconBitmap() throws RemoteException;

        /** See implementation on {@link TrustedWebActivityServiceConnection}. */
        Bundle sendExtraCommand(
                String commandName, Bundle args, TrustedWebActivityCallback callback)
                throws RemoteException;
    }

    /** Wrapper around {@link TrustedWebActivityServiceConnectionPool}. */
    public interface ConnectionPool {
        /** See implementation on {@link TrustedWebActivityServiceConnectionPool}. */
        boolean serviceExistsForScope(Uri scope, Set<Token> possiblePackages);

        /** See implementation on {@link TrustedWebActivityServiceConnectionPool}. */
        void connectAndExecute(
                Uri scope, Origin origin, Set<Token> possiblePackages, ExecutionCallback callback);
    }

    public static ConnectionPool wrap(TrustedWebActivityServiceConnectionPool pool) {
        return new ConnectionPool() {
            @Override
            public boolean serviceExistsForScope(Uri scope, Set<Token> possiblePackages) {
                return pool.serviceExistsForScope(scope, possiblePackages);
            }

            @Override
            public void connectAndExecute(
                    Uri scope,
                    Origin origin,
                    Set<Token> possiblePackages,
                    ExecutionCallback callback) {
                ListenableFuture<TrustedWebActivityServiceConnection> connection =
                        pool.connect(scope, possiblePackages, AsyncTask.THREAD_POOL_EXECUTOR);

                connection.addListener(
                        () -> {
                            try {
                                callback.onConnected(origin, wrap(connection.get()));
                            } catch (RemoteException | InterruptedException e) {
                                // These failures could be transient - a RemoteException indicating
                                // that the TWA got killed as it was answering and an
                                // InterruptedException to do with threading on our side. In this
                                // case, there's not anything necessarily wrong with the TWA.
                                Log.w(TAG, "Failed to execute TWA command.", e);
                            } catch (ExecutionException | SecurityException e) {
                                // An ExecutionException means that we could not find a TWA to
                                // connect to and a SecurityException means that the TWA doesn't
                                // trust this app. In either cases we consider that there is no
                                // TWA for the scope.
                                Log.w(TAG, "Failed to connect to TWA to execute command", e);
                                callback.onNoTwaFound();
                            }
                        },
                        UI_THREAD_EXECUTOR);
            }
        };
    }

    public static Connection wrap(TrustedWebActivityServiceConnection connection) {
        return new Connection() {
            @Override
            public ComponentName getComponentName() {
                return connection.getComponentName();
            }

            @Override
            public boolean areNotificationsEnabled(String channelName) throws RemoteException {
                return connection.areNotificationsEnabled(channelName);
            }

            @Override
            public boolean notify(
                    String platformTag, int platformId, Notification notification, String channel)
                    throws RemoteException {
                return connection.notify(platformTag, platformId, notification, channel);
            }

            @Override
            public void cancel(String platformTag, int platformId) throws RemoteException {
                connection.cancel(platformTag, platformId);
            }

            @Override
            public int getSmallIconId() throws RemoteException {
                return connection.getSmallIconId();
            }

            @Override
            public Bitmap getSmallIconBitmap() throws RemoteException {
                return connection.getSmallIconBitmap();
            }

            @Override
            public Bundle sendExtraCommand(
                    String commandName, Bundle args, TrustedWebActivityCallback callback)
                    throws RemoteException {
                return connection.sendExtraCommand(commandName, args, callback);
            }
        };
    }
}
