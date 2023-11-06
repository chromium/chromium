// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.junit.runner.Description;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.vr.rules.VrModuleNotInstalled;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Class for accessing VrShellDelegate internals for testing purposes. This does two things: -
 * Prevents us from needing @VisibleForTesting annotations everywhere in production code. - Allows
 * us to have test-specific behavior if necessary without changing production code.
 */
public class TestVrShellDelegate extends VrShellDelegate {
    private Runnable mOnVSyncPausedCallback;
    private static TestVrShellDelegate sInstance;
    private static Description sTestDescription;
    private boolean mExpectingBroadcast;
    private boolean mExpectingIntent;
    private Boolean mAllow2dIntents;

    public static void createTestVrShellDelegate(final ChromeActivity activity) {
        // Cannot make VrShellDelegate if we are faking that the VR module is not installed.
        if (sTestDescription.getAnnotation(VrModuleNotInstalled.class) != null) return;
        if (sInstance != null) return;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> {
                    sInstance = new TestVrShellDelegate(activity);
                });
    }

    // TODO(bsheedy): Maybe remove this and switch to setting a VrShellDelegateFactory instead.
    public static void setDescription(Description desc) {
        sTestDescription = desc;
    }

    public static TestVrShellDelegate getInstance() {
        return sInstance;
    }

    public static VrShell getVrShellForTesting() {
        return TestVrShellDelegate.getInstance().getVrShell();
    }

    public static void enableTestVrShellDelegateOnStartupForTesting() {
        VrShellDelegate.enableTestVrShellDelegateOnStartupForTesting();
    }

    protected TestVrShellDelegate(ChromeActivity activity) {
        super(activity);
    }

    public void overrideDaydreamApiForTesting(VrDaydreamApi api) {
        super.overrideDaydreamApi(api);
    }

    @Override
    public boolean isVrEntryComplete() {
        return super.isVrEntryComplete();
    }

    public void setVrShellOnVSyncPausedCallback(Runnable callback) {
        mOnVSyncPausedCallback = callback;
    }

    /**
     * The same as the production onResume, except that we set a boolean to avoid cancelling VR
     * entry when we think we're in the DON flow. This is caused by crbug.com/762724. TODO(bsheedy):
     * Remove this when the root cause is fixed.
     */
    @Override
    protected void onResume() {
        if (mExpectingIntent || mExpectingBroadcast) {
            mTestWorkaroundDontCancelVrEntryOnResume = true;
        }
        super.onResume();
        mTestWorkaroundDontCancelVrEntryOnResume = false;
    }

    /**
     * If we need to know when the normal VSync gets paused, we have a small window between when the
     * VrShell is created and we actually enter VR to set the callback. So, do it immediately after
     * creation here.
     */
    @Override
    protected boolean createVrShell() {
        boolean result = super.createVrShell();
        if (result && mOnVSyncPausedCallback != null) {
            getVrShellForTesting().setOnVSyncPausedForTesting(mOnVSyncPausedCallback);
        }
        return result;
    }
}
