// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.view.Surface;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.tab.Tab;

/**
 * This view extends from GvrLayout which wraps a GLSurfaceView that renders VR shell.
 */
@JNINamespace("vr")
public class VrShell {

    // Returns true if Chrome has permission to use audio input.
    @CalledByNative
    public boolean hasRecordAudioPermission() {
        return false;
    }

    // Returns true if Chrome has not been permanently denied audio input permission.
    @CalledByNative
    public boolean canRequestRecordAudioPermission() {
        return false;
    }

    // Exits VR, telling the user to remove their headset, and returning to Chromium.
    @CalledByNative
    public void forceExitVr() {
    }

    // Called when the user clicks on the security icon in the URL bar.
    @CalledByNative
    public void showPageInfo() {
    }

    // Called because showing audio permission dialog isn't supported in VR. This happens when
    // the user wants to do a voice search.
    @CalledByNative
    public void onUnhandledPermissionPrompt() {
    }

    // Called when the user has an older GVR Keyboard installed on their device and we need them to
    // have a newer one.
    @CalledByNative
    public void onNeedsKeyboardUpdate() {
    }

    // Close the current hosted Dialog in VR
    @CalledByNative
    public void closeCurrentDialog() {
    }

    @CalledByNative
    public void setContentCssSize(float width, float height, float dpr) {
    }

    @CalledByNative
    public void contentSurfaceCreated(Surface surface) {
    }

    @CalledByNative
    public void contentOverlaySurfaceCreated(Surface surface) {
    }

    @CalledByNative
    public void dialogSurfaceCreated(Surface surface) {
    }

    @CalledByNative
    public boolean hasDaydreamSupport() {
        return false;
    }

    @CalledByNative
    private void onExitVrRequestResult(boolean shouldExit) {
    }

    @CalledByNative
    private void loadUrl(String url) {
    }

    @CalledByNative
    public void reloadTab() {
    }

    @CalledByNative
    public void openNewTab(boolean incognito) {
    }

    @CalledByNative
    public void openBookmarks() {
    }

    @CalledByNative
    public void openRecentTabs() {
    }

    @CalledByNative
    public void openHistory() {
    }

    @CalledByNative
    public void openDownloads() {
    }

    @CalledByNative
    public void openShare() {
    }

    @CalledByNative
    public void openSettings() {
    }

    @CalledByNative
    public void closeAllIncognitoTabs() {
    }

    @CalledByNative
    public void openFeedback() {
    }

    @CalledByNative
    public void reportUiOperationResultForTesting(int actionType, int result) {
    }

    @NativeMethods
    interface Natives {
        long init(VrShell caller, VrShellDelegate delegate, boolean forWebVR,
                boolean browsingDisabled, boolean hasOrCanRequestRecordAudioPermission, long gvrApi,
                boolean reprojectedRendering, float displayWidthMeters, float displayHeightMeters,
                int displayWidthPixels, int displayHeightPixels, boolean pauseContent,
                boolean lowDensity, boolean isStandaloneVrDevice);
        boolean hasUiFinishedLoading(long nativeVrShell, VrShell caller);
        void setSurface(long nativeVrShell, VrShell caller, Surface surface);
        void swapContents(long nativeVrShell, VrShell caller, Tab tab);
        void setAndroidGestureTarget(
                long nativeVrShell, VrShell caller, AndroidUiGestureTarget androidUiGestureTarget);
        void setDialogGestureTarget(
                long nativeVrShell, VrShell caller, AndroidUiGestureTarget dialogGestureTarget);
        void destroy(long nativeVrShell, VrShell caller);
        void onTriggerEvent(long nativeVrShell, VrShell caller, boolean touched);
        void onPause(long nativeVrShell, VrShell caller);
        void onResume(long nativeVrShell, VrShell caller);
        void onLoadProgressChanged(long nativeVrShell, VrShell caller, double progress);
        void bufferBoundsChanged(long nativeVrShell, VrShell caller, int contentWidth,
                int contentHeight, int overlayWidth, int overlayHeight);
        void setWebVrMode(long nativeVrShell, VrShell caller, boolean enabled);
        boolean getWebVrMode(long nativeVrShell, VrShell caller);
        boolean isDisplayingUrlForTesting(long nativeVrShell, VrShell caller);
        void onTabListCreated(
                long nativeVrShell, VrShell caller, Tab[] mainTabs, Tab[] incognitoTabs);
        void onTabUpdated(
                long nativeVrShell, VrShell caller, boolean incognito, int id, String title);
        void onTabRemoved(long nativeVrShell, VrShell caller, boolean incognito, int id);
        void closeAlertDialog(long nativeVrShell, VrShell caller);
        void setAlertDialog(long nativeVrShell, VrShell caller, float width, float height);
        void setDialogBufferSize(long nativeVrShell, VrShell caller, int width, int height);
        void setAlertDialogSize(long nativeVrShell, VrShell caller, float width, float height);
        void setDialogLocation(long nativeVrShell, VrShell caller, float x, float y);
        void setDialogFloating(long nativeVrShell, VrShell caller, boolean floating);
        void showToast(long nativeVrShell, VrShell caller, String text);
        void cancelToast(long nativeVrShell, VrShell caller);
        void setHistoryButtonsEnabled(
                long nativeVrShell, VrShell caller, boolean canGoBack, boolean canGoForward);
        void requestToExitVr(long nativeVrShell, VrShell caller, int reason);
        void showSoftInput(long nativeVrShell, VrShell caller, boolean show);
        void updateWebInputIndices(long nativeVrShell, VrShell caller, int selectionStart,
                int selectionEnd, int compositionStart, int compositionEnd);
        VrInputConnection getVrInputConnectionForTesting(long nativeVrShell, VrShell caller);
        void acceptDoffPromptForTesting(long nativeVrShell, VrShell caller);
        void performControllerActionForTesting(long nativeVrShell, VrShell caller, int elementName,
                int actionType, float x, float y);
        void performKeyboardInputForTesting(
                long nativeVrShell, VrShell caller, int inputType, String inputString);
        void setUiExpectingActivityForTesting(
                long nativeVrShell, VrShell caller, int quiescenceTimeoutMs);
        void saveNextFrameBufferToDiskForTesting(
                long nativeVrShell, VrShell caller, String filepathBase);
        void watchElementForVisibilityStatusForTesting(long nativeVrShell, VrShell caller,
                int elementName, int timeoutMs, boolean visibility);
        void resumeContentRendering(long nativeVrShell, VrShell caller);
        void onOverlayTextureEmptyChanged(long nativeVrShell, VrShell caller, boolean empty);
        void requestRecordAudioPermissionResult(
                long nativeVrShell, VrShell caller, boolean canRecordAudio);
    }
}
