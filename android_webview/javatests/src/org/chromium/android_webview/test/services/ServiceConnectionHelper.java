// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.services;

import android.content.ComponentName;
import android.content.Intent;
import android.content.ServiceConnection;
import android.os.IBinder;

import com.google.common.util.concurrent.SettableFuture;

import org.junit.Assert;

import org.chromium.android_webview.common.services.ServiceHelper;
import org.chromium.android_webview.test.AwActivityTestRule;
import org.chromium.base.ContextUtils;

/**
 * An abstraction to manage Service connections with the "try-with-resources" pattern. Instantiate
 * this class to initiate a binding to the specified Service. This will automatically unbind when
 * the try-block exits.
 *
 * <p>This must never be instantiated on the main Looper.
 */
public class ServiceConnectionHelper implements AutoCloseable {
    final SettableFuture<IBinder> mFuture = SettableFuture.create();
    final ServiceConnection mConnection;

    /**
     * Connects to a Service specified by {@code intent} with {@code flags}.
     *
     * @param intent should specify a Service.
     * @param flags should be {@code 0} or a combination of {@code Context#BIND_*}.
     */
    public ServiceConnectionHelper(Intent intent, int flags) {
        mConnection =
                new ServiceConnection() {
                    @Override
                    public void onServiceConnected(ComponentName name, IBinder service) {
                        mFuture.set(service);
                    }

                    @Override
                    public void onServiceDisconnected(ComponentName name) {}
                };

        boolean success =
                ServiceHelper.bindService(
                        ContextUtils.getApplicationContext(), intent, mConnection, flags);
        Assert.assertTrue(
                "Failed to bind to service with "
                        + intent
                        + ". "
                        + "Did you expose it in android_webview/test/shell/AndroidManifest.xml?",
                success);
    }

    /** Returns the IBinder for this connection. */
    public IBinder getBinder() {
        return AwActivityTestRule.waitForFuture(mFuture);
    }

    @Override
    public void close() {
        ContextUtils.getApplicationContext().unbindService(mConnection);
    }
}
