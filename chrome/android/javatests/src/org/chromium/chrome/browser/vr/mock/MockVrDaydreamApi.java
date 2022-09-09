// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr.mock;

import android.app.Activity;
import android.app.PendingIntent;
import android.content.Intent;

import org.chromium.chrome.browser.vr.VrDaydreamApi;

// TODO(bsheedy): Make Mockito work in instrumentation tests and replace uses of this class with
// Mockito spies.
/**
 * "Mock" implementation of VR Shell's VrDaydreamApi that mostly does the same thing as the normal
 * VrDaydreamApiImpl, but allows checking whether methods were called and modifying return values.
 */
public class MockVrDaydreamApi extends VrDaydreamApi {
    private boolean mLaunchInVrCalled;
    private boolean mExitFromVrCalled;
    private boolean mLaunchVrHomescreenCalled;
    private boolean mDoNotForwardLaunchRequests;
    private Boolean mExitFromVrReturnValue;

    @Override
    public boolean launchInVr(final PendingIntent pendingIntent) {
        mLaunchInVrCalled = true;
        if (mDoNotForwardLaunchRequests) return true;
        return super.launchInVr(pendingIntent);
    }

    @Override
    public boolean launchInVr(final Intent intent) {
        mLaunchInVrCalled = true;
        if (mDoNotForwardLaunchRequests) return true;
        return super.launchInVr(intent);
    }

    public boolean getLaunchInVrCalled() {
        return mLaunchInVrCalled;
    }

    @Override
    public boolean launchVrHomescreen() {
        mLaunchVrHomescreenCalled = true;
        return super.launchVrHomescreen();
    }

    public boolean getLaunchVrHomescreenCalled() {
        return mLaunchVrHomescreenCalled;
    }

    @Override
    public boolean exitFromVr(Activity activity, int requestCode, final Intent intent) {
        mExitFromVrCalled = true;
        if (mExitFromVrReturnValue == null) return super.exitFromVr(activity, requestCode, intent);
        return mExitFromVrReturnValue.booleanValue();
    }

    public boolean getExitFromVrCalled() {
        return mExitFromVrCalled;
    }

    public void setExitFromVrReturnValue(Boolean value) {
        mExitFromVrReturnValue = value;
    }
}
