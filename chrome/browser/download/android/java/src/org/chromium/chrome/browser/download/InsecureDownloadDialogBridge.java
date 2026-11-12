// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniPtr;
import org.jni_zero.JniRawPtr;
import org.jni_zero.JniType;
import org.jni_zero.JniTypeToken;
import org.jni_zero.NativeMethods;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.dialogs.InsecureDownloadDialog;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.util.ArrayList;
import java.util.List;

/**
 * Glues insecure download dialogs UI code and handles the communication to download native backend.
 */
@NullMarked
public class InsecureDownloadDialogBridge {
    @JniType("::InsecureDownloadDialogBridge")
    interface NativeInsecureDownloadDialogBridge extends JniTypeToken {}

    private static class PendingDialog {
        final String mFileName;
        final long mTotalBytes;
        final long mCallbackId;

        PendingDialog(String fileName, long totalBytes, long callbackId) {
            this.mFileName = fileName;
            this.mTotalBytes = totalBytes;
            this.mCallbackId = callbackId;
        }
    }

    private @Nullable JniRawPtr<NativeInsecureDownloadDialogBridge>
            mNativeInsecureDownloadDialogBridge;
    private final List<PendingDialog> mPendingDialogs = new ArrayList<>();
    private @Nullable ActivityStateListener mActivityStateListener;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeInsecureDownloadDialogBridge Pointer to the native object.
     */
    public InsecureDownloadDialogBridge(
            JniRawPtr<NativeInsecureDownloadDialogBridge> nativeInsecureDownloadDialogBridge) {
        mNativeInsecureDownloadDialogBridge = nativeInsecureDownloadDialogBridge;
    }

    @CalledByNative
    private static InsecureDownloadDialogBridge create(
            JniRawPtr<NativeInsecureDownloadDialogBridge> nativeDialog) {
        return new InsecureDownloadDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for insecure download.
     *
     * @param windowAndroid Window to show the dialog, or null if no window is currently available.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param callbackId Native callback Id to invoke.
     */
    @CalledByNative
    private void showDialog(
            @Nullable WindowAndroid windowAndroid,
            @JniType("std::u16string") String fileName,
            long totalBytes,
            long callbackId) {
        PendingDialog pending = new PendingDialog(fileName, totalBytes, callbackId);
        Activity activity = getValidResumedActivity(windowAndroid);
        if (activity != null) {
            showDialogInternal(pending, activity);
        } else {
            mPendingDialogs.add(pending);
            ensureActivityStateListener();
        }
    }

    private @Nullable Activity getValidResumedActivity(@Nullable WindowAndroid windowAndroid) {
        Activity activity = ApplicationStatus.getLastTrackedFocusedActivity();
        if (activity != null && isValidResumedActivity(activity)) {
            return activity;
        }
        if (windowAndroid != null && windowAndroid.getActivity() != null) {
            Activity windowActivity = windowAndroid.getActivity().get();
            if (windowActivity != null && isValidResumedActivity(windowActivity)) {
                return windowActivity;
            }
        }
        for (Activity runningActivity : ApplicationStatus.getRunningActivities()) {
            if (isValidResumedActivity(runningActivity)) {
                return runningActivity;
            }
        }
        return null;
    }

    private static boolean isValidResumedActivity(@Nullable Activity activity) {
        if (activity == null || !(activity instanceof ModalDialogManagerHolder)) {
            return false;
        }
        if (activity.isFinishing() || activity.isDestroyed()) {
            return false;
        }
        int state = ApplicationStatus.getStateForActivity(activity);
        if (state != ActivityState.RESUMED && state != ActivityState.STARTED) {
            return false;
        }
        return ((ModalDialogManagerHolder) activity).getModalDialogManager() != null;
    }

    private void ensureActivityStateListener() {
        if (mActivityStateListener == null) {
            mActivityStateListener =
                    (activity, newState) -> {
                        if (newState == ActivityState.RESUMED && isValidResumedActivity(activity)) {
                            List<PendingDialog> toShow = new ArrayList<>(mPendingDialogs);
                            mPendingDialogs.clear();
                            cleanUpListenerIfEmpty();
                            for (PendingDialog pending : toShow) {
                                showDialogInternal(pending, activity);
                            }
                        }
                    };
            ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
        }
    }

    private void cleanUpListenerIfEmpty() {
        if (mPendingDialogs.isEmpty() && mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }
    }

    private void showDialogInternal(PendingDialog pending, Activity activity) {
        ModalDialogManager dialogManager =
                ((ModalDialogManagerHolder) activity).getModalDialogManager();
        if (dialogManager == null) {
            onConfirmed(pending.mCallbackId, false);
            return;
        }

        new InsecureDownloadDialog()
                .show(
                        activity,
                        dialogManager,
                        pending.mFileName,
                        pending.mTotalBytes,
                        (accepted) -> {
                            onConfirmed(pending.mCallbackId, accepted);
                        });
    }

    @CalledByNative
    private void destroy() {
        if (mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }
        for (PendingDialog pending : mPendingDialogs) {
            onConfirmed(pending.mCallbackId, false);
        }
        mPendingDialogs.clear();
        if (mNativeInsecureDownloadDialogBridge != null) {
            mNativeInsecureDownloadDialogBridge.release();
            mNativeInsecureDownloadDialogBridge = null;
        }
    }

    private void onConfirmed(long callbackId, boolean accepted) {
        if (mNativeInsecureDownloadDialogBridge != null) {
            InsecureDownloadDialogBridgeJni.get()
                    .onConfirmed(mNativeInsecureDownloadDialogBridge, callbackId, accepted);
        }
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(
                JniPtr<NativeInsecureDownloadDialogBridge> self,
                long callbackId,
                boolean accepted);
    }
}
