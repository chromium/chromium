// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.graphics.PointF;
import android.os.Build;

import org.junit.runner.Description;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.vr.rules.VrModuleNotInstalled;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Class for accessing VrShellDelegate internals for testing purposes.
 * This does two things:
 * - Prevents us from needing @VisibleForTesting annotations everywhere in production code.
 * - Allows us to have test-specific behavior if necessary without changing production code.
 */
public class TestVrShellDelegate extends VrShellDelegate {
    private Runnable mOnVSyncPausedCallback;
    private static TestVrShellDelegate sInstance;
    private static Description sTestDescription;
    private boolean mDisableVrBrowsing;
    private boolean mExpectingBroadcast;
    private boolean mExpectingIntent;
    private Boolean mAllow2dIntents;

    public static void createTestVrShellDelegate(final ChromeActivity activity) {
        // Cannot make VrShellDelegate if we are faking that the VR module is not installed.
        if (sTestDescription.getAnnotation(VrModuleNotInstalled.class) != null) return;
        if (sInstance != null) return;
        TestThreadUtils.runOnUiThreadBlocking(
                () -> { sInstance = new TestVrShellDelegate(activity); });
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

    public static boolean isDisplayingUrlForTesting() {
        return TestVrShellDelegate.getInstance().getVrShell().isDisplayingUrlForTesting();
    }

    public static boolean isOnStandalone() {
        return Build.DEVICE.equals("vega");
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

    public void overrideVrCoreVersionCheckerForTesting(VrCoreVersionChecker versionChecker) {
        super.overrideVrCoreVersionChecker(versionChecker);
    }

    public void setFeedbackFrequencyForTesting(int frequency) {
        super.setFeedbackFrequency(frequency);
    }

    @Override
    public boolean isBlackOverlayVisible() {
        return super.isBlackOverlayVisible();
    }

    @Override
    public boolean isVrEntryComplete() {
        return super.isVrEntryComplete();
    }

    @Override
    public boolean isShowingDoff() {
        return super.isShowingDoff();
    }

    public void acceptDoffPromptForTesting() {
        getVrShell().acceptDoffPromptForTesting();
    }

    public void performControllerActionForTesting(
            int elementName, int actionType, PointF position) {
        getVrShell().performControllerActionForTesting(elementName, actionType, position);
    }

    public void performKeyboardInputForTesting(int inputType, String inputString) {
        getVrShell().performKeyboardInputForTesting(inputType, inputString);
    }

    public void registerUiOperationCallbackForTesting(VrShell.UiOperationData operationData) {
        getVrShell().registerUiOperationCallbackForTesting(operationData);
    }

    public void saveNextFrameBufferToDiskForTesting(String filepathBase) {
        getVrShell().saveNextFrameBufferToDiskForTesting(filepathBase);
    }

    public int getLastUiOperationResultForTesting(int actionType) {
        return getVrShell().getLastUiOperationResultForTesting(actionType);
    }

    @Override
    protected boolean isVrBrowsingEnabled() {
        if (mDisableVrBrowsing) return false;
        return super.isVrBrowsingEnabled();
    }

    public void setVrBrowsingDisabled(boolean disabled) {
        mDisableVrBrowsing = disabled;
    }

    public void setVrShellOnVSyncPausedCallback(Runnable callback) {
        mOnVSyncPausedCallback = callback;
    }

    /**
     * The same as the production onResume, except that we set a boolean to avoid cancelling VR
     * entry when we think we're in the DON flow. This is caused by crbug.com/762724.
     * TODO(bsheedy): Remove this when the root cause is fixed.
     */
    @Override
    protected void onResume() {
        if (mExpectingIntent || mExpectingBroadcast) {
            mTestWorkaroundDontCancelVrEntryOnResume = true;
        }
        super.onResume();
        mTestWorkaroundDontCancelVrEntryOnResume = false;
    }

    @Override
    protected void setExpectingIntent(boolean expectingIntent) {
        mExpectingIntent = expectingIntent;
    }

    @Override
    protected void onBroadcastReceived() {
        mExpectingBroadcast = false;
    }

    public void setExpectingBroadcast() {
        mExpectingBroadcast = true;
    }

    public boolean isExpectingBroadcast() {
        return mExpectingBroadcast;
    }

    /**
     * If we need to know when the normal VSync gets paused, we have a small window between when
     * the VrShell is created and we actually enter VR to set the callback. So, do it immediately
     * after creation here.
     */
    @Override
    protected boolean createVrShell() {
        boolean result = super.createVrShell();
        if (result && mOnVSyncPausedCallback != null) {
            getVrShellForTesting().setOnVSyncPausedForTesting(mOnVSyncPausedCallback);
        }
        return result;
    }

    @Override
    protected boolean canLaunch2DIntentsInternal() {
        if (mAllow2dIntents == null) return super.canLaunch2DIntentsInternal();
        return mAllow2dIntents;
    }

    public void setAllow2dIntents(boolean allow) {
        mAllow2dIntents = allow;
    }
}
