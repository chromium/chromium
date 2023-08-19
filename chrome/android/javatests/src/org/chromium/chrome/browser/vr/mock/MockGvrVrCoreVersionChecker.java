// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.mock;

import org.chromium.chrome.browser.vr.VrCoreVersionChecker;

/**
 * Mock version of VrCoreVersionCheckerImpl that allows setting of the return
 * value.
 */
public class MockGvrVrCoreVersionChecker extends VrCoreVersionChecker {
    private boolean mUseActualImplementation;
    private @VrCoreCompatibility int mMockReturnValue = VrCoreCompatibility.VR_READY;
    private @VrCoreCompatibility int mLastReturnValue;

    @Override
    public @VrCoreCompatibility int getVrCoreCompatibility() {
        mLastReturnValue =
                mUseActualImplementation ? super.getVrCoreCompatibility() : mMockReturnValue;
        return mLastReturnValue;
    }

    public @VrCoreCompatibility int getLastReturnValue() {
        return mLastReturnValue;
    }

    public void setMockReturnValue(@VrCoreCompatibility int value) {
        mMockReturnValue = value;
    }

    public void setUseActualImplementation(boolean useActual) {
        mUseActualImplementation = useActual;
    }
}
