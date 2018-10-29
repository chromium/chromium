// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.offlinepages.indicator;

/**
 * Stub of ConnectivityDetector.Delegate for testing purpose.
 */
public class ConnectivityDetectorDelegateStub implements ConnectivityDetector.Delegate {
    private @ConnectivityDetector.ConnectionState int mConnectionStateFromSystem;
    private boolean mSkipHttpProbes;

    public ConnectivityDetectorDelegateStub(
            @ConnectivityDetector.ConnectionState int connectionStateFromSystem,
            boolean skipHttpProbes) {
        mConnectionStateFromSystem = connectionStateFromSystem;
        mSkipHttpProbes = skipHttpProbes;
    }

    @Override
    public @ConnectivityDetector.ConnectionState int inferConnectionStateFromSystem() {
        return mConnectionStateFromSystem;
    }

    @Override
    public boolean shouldSkipHttpProbes() {
        return mSkipHttpProbes;
    }

    public void setConnectionStateFromSystem(
            @ConnectivityDetector.ConnectionState int connectionStateFromSystem) {
        mConnectionStateFromSystem = connectionStateFromSystem;
    }
}
