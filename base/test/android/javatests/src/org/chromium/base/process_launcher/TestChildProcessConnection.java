// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.process_launcher;

import android.content.ComponentName;
import android.content.Intent;
import android.os.Bundle;

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
        public void unbindServiceConnection() {
            mBound = false;
        }

        @Override
        public boolean isBound() {
            return mBound;
        }

        @Override
        public void updateGroupImportance(int group, int importanceInGroup) {}

        @Override
        public void retire() {}
    }

    private int mPid;
    private boolean mConnected;
    private ServiceCallback mServiceCallback;
    private boolean mRebindCalled;

    /**
     * Creates a mock binding corresponding to real ManagedChildProcessConnection after the
     * connection is established: with initial binding bound and no strong binding.
     */
    public TestChildProcessConnection(ComponentName serviceName, boolean bindToCaller,
            boolean bindAsExternalService, Bundle serviceBundle) {
        super(null /* context */, serviceName, null, bindToCaller, bindAsExternalService,
                serviceBundle, new ChildServiceConnectionFactory() {
                    @Override
                    public ChildServiceConnection createConnection(Intent bindIntent, int bindFlags,
                            ChildServiceConnectionDelegate delegate, String instanceName) {
                        return new MockChildServiceConnection();
                    }
                }, null /* instanceName */);
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
    public void start(boolean useStrongBinding, ServiceCallback serviceCallback) {
        super.start(useStrongBinding, serviceCallback);
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
