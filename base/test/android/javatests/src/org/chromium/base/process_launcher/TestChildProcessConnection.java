// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;

import org.chromium.base.ChildBindingState;
import org.chromium.build.annotations.Nullable;

/** An implementation of ChildProcessConnection that does not connect to a real service. */
public class TestChildProcessConnection extends ChildProcessConnection {
    private static class MockChildServiceConnection implements ChildServiceConnection {
        private boolean mBound;

        @Override
        public boolean bindServiceConnection() {
            mBound = true;
            return true;
        }

        @Override
        public void unbindServiceConnection(@Nullable Runnable onStateChangeCallback) {
            mBound = false;
            if (onStateChangeCallback != null) {
                onStateChangeCallback.run();
            }
        }

        @Override
        public boolean isBound() {
            return mBound;
        }

        @Override
        public boolean updateGroupImportance(int group, int importanceInGroup) {
            return true;
        }

        @Override
        public void retire() {}

        @Override
        public void rebindService(int bindFlags) {}
    }

    private int mPid;
    private boolean mConnected;
    private ServiceCallback mServiceCallback;
    private boolean mRebindCalled;

    /**
     * Creates a mock binding corresponding to real ManagedChildProcessConnection after the
     * connection is established: with initial binding bound and no strong binding.
     */
    public TestChildProcessConnection(
            ComponentName serviceName,
            boolean bindToCaller,
            boolean bindAsExternalService,
            Bundle serviceBundle) {
        super(
                /* context= */ null,
                serviceName,
                null,
                bindToCaller,
                bindAsExternalService,
                serviceBundle,
                new ChildServiceConnectionFactory() {
                    @Override
                    public ChildServiceConnection createConnection(
                            Intent bindIntent,
                            int bindFlags,
                            ChildServiceConnectionDelegate delegate,
                            String instanceName) {
                        return new MockChildServiceConnection();
                    }
                },
                /* instanceName= */ null,
                /* independentFallback= */ false,
                /* isSandboxedForHistograms= */ false);
    }

    public void setPid(int pid) {
        mPid = pid;
    }

    @Override
    public int getPid() {
        return mPid;
    }

    // We don't have a real service so we have to mock the connection status.
    @Override
    public void start(@ChildBindingState int initialBindingState, ServiceCallback serviceCallback) {
        super.start(initialBindingState, serviceCallback);
        mConnected = true;
        mServiceCallback = serviceCallback;
    }

    @Override
    public void rebind() {
        super.rebind();
        mRebindCalled = true;
    }

    @Override
    public void stop() {
        super.stop();
        mConnected = false;
    }

    @Override
    public boolean isConnected() {
        return mConnected;
    }

    public ServiceCallback getServiceCallback() {
        return mServiceCallback;
    }

    public boolean getAndResetRebindCalled() {
        boolean called = mRebindCalled;
        mRebindCalled = false;
        return called;
    }
}
