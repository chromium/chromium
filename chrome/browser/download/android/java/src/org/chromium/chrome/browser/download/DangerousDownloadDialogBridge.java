// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.download.dialogs.DangerousDownloadDialog;
import org.chromium.chrome.browser.download.dialogs.DangerousDownloadDialog.DangerousDownloadDialogEvent;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Map;
import java.util.Set;

/** Glues download dialogs UI code and handles the communication to download native backend. */
@NullMarked
public class DangerousDownloadDialogBridge {
    private static class PendingDialog {
        final WindowAndroid mWindowAndroid;
        final String mGuid;
        final String mFileName;
        final long mTotalBytes;
        final String mDownloadDomain;
        final int mIconId;
        final boolean mIsDangerous;

        PendingDialog(
                WindowAndroid windowAndroid,
                String guid,
                String fileName,
                long totalBytes,
                String downloadDomain,
                int iconId,
                boolean isDangerous) {
            this.mWindowAndroid = windowAndroid;
            this.mGuid = guid;
            this.mFileName = fileName;
            this.mTotalBytes = totalBytes;
            this.mDownloadDomain = downloadDomain;
            this.mIconId = iconId;
            this.mIsDangerous = isDangerous;
        }
    }

    private long mNativeDangerousDownloadDialogBridge;
    private final Map<String, PendingDialog> mPendingDialogs = new HashMap<>();
    private final Set<String> mShowingDialogGuids = new HashSet<>();
    private @Nullable ActivityStateListener mActivityStateListener;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeDangerousDownloadDialogBridge Pointer to the native object.
     */
    public DangerousDownloadDialogBridge(long nativeDangerousDownloadDialogBridge) {
        mNativeDangerousDownloadDialogBridge = nativeDangerousDownloadDialogBridge;
    }

    @CalledByNative
    public static DangerousDownloadDialogBridge create(long nativeDialog) {
        return new DangerousDownloadDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for download.
     *
     * @param windowAndroid Window to show the dialog.
     * @param guid GUID of the download.
     * @param fileName Name of the download file.
     * @param totalBytes Total bytes of the file.
     * @param downloadDomain Domain name to associate with the downloaded file.
     * @param iconId The icon resource for the warning dialog.
     * @param isDangerous The danger status of the download file.
     */
    @CalledByNative
    public void showDialog(
            WindowAndroid windowAndroid,
            @JniType("std::string") String guid,
            @JniType("std::u16string") String fileName,
            long totalBytes,
            @JniType("std::u16string") String downloadDomain,
            int iconId,
            boolean isDangerous) {
        if (!ChromeFeatureList.sMaliciousApkDownloadCheck.isEnabled()) {
            Activity activity =
                    windowAndroid.getActivity() != null ? windowAndroid.getActivity().get() : null;
            if (!(activity instanceof ModalDialogManagerHolder)) {
                onCancel(guid, windowAndroid);
                return;
            }

            new DangerousDownloadDialog()
                    .show(
                            activity,
                            ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                            fileName,
                            totalBytes,
                            downloadDomain,
                            iconId,
                            (result) -> {
                                if (result
                                        == DangerousDownloadDialogEvent
                                                .DANGEROUS_DOWNLOAD_DIALOG_CONFIRM) {
                                    onAccepted(guid);
                                } else {
                                    onCancel(guid, windowAndroid);
                                }
                            },
                            isDangerous);
            return;
        }

        PendingDialog pending =
                new PendingDialog(
                        windowAndroid,
                        guid,
                        fileName,
                        totalBytes,
                        downloadDomain,
                        iconId,
                        isDangerous);
        mPendingDialogs.put(guid, pending);

        Activity activity = getValidResumedActivity(windowAndroid);
        if (activity != null) {
            showDialogInternal(pending, activity);
        } else {
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
                            showAllPendingDialogs(activity);
                        }
                    };
            ApplicationStatus.registerStateListenerForAllActivities(mActivityStateListener);
        }
    }

    private void showAllPendingDialogs(Activity activity) {
        for (PendingDialog pending : new ArrayList<>(mPendingDialogs.values())) {
            if (!mShowingDialogGuids.contains(pending.mGuid)) {
                showDialogInternal(pending, activity);
            }
        }
    }

    private void showDialogInternal(PendingDialog pending, Activity activity) {
        ModalDialogManager dialogManager =
                ((ModalDialogManagerHolder) activity).getModalDialogManager();
        if (dialogManager == null) return;

        mShowingDialogGuids.add(pending.mGuid);
        new DangerousDownloadDialog()
                .show(
                        activity,
                        dialogManager,
                        pending.mFileName,
                        pending.mTotalBytes,
                        pending.mDownloadDomain,
                        pending.mIconId,
                        (result) -> {
                            mShowingDialogGuids.remove(pending.mGuid);
                            if (result
                                    == DangerousDownloadDialogEvent
                                            .DANGEROUS_DOWNLOAD_DIALOG_CONFIRM) {
                                mPendingDialogs.remove(pending.mGuid);
                                cleanUpListenerIfEmpty();
                                onAccepted(pending.mGuid);
                            } else if (result
                                            == DangerousDownloadDialogEvent
                                                    .DANGEROUS_DOWNLOAD_DIALOG_CANCEL
                                    || !ChromeFeatureList.sMaliciousApkDownloadCheck.isEnabled()) {
                                mPendingDialogs.remove(pending.mGuid);
                                cleanUpListenerIfEmpty();
                                onCancel(pending.mGuid, pending.mWindowAndroid);
                            } else {
                                Activity nextActivity =
                                        getValidResumedActivity(pending.mWindowAndroid);
                                if (nextActivity != null && nextActivity != activity) {
                                    showDialogInternal(pending, nextActivity);
                                } else {
                                    ensureActivityStateListener();
                                }
                            }
                        },
                        pending.mIsDangerous);
    }

    private void cleanUpListenerIfEmpty() {
        if (mPendingDialogs.isEmpty() && mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }
    }

    @CalledByNative
    private void destroy() {
        mNativeDangerousDownloadDialogBridge = 0;
        if (mActivityStateListener != null) {
            ApplicationStatus.unregisterActivityStateListener(mActivityStateListener);
            mActivityStateListener = null;
        }
        mPendingDialogs.clear();
        mShowingDialogGuids.clear();
    }

    private void onAccepted(String guid) {
        if (mNativeDangerousDownloadDialogBridge != 0) {
            DangerousDownloadDialogBridgeJni.get()
                    .accepted(mNativeDangerousDownloadDialogBridge, guid);
        }
    }

    private void onCancel(String guid, @Nullable WindowAndroid windowAndroid) {
        if (mNativeDangerousDownloadDialogBridge != 0) {
            DangerousDownloadDialogBridgeJni.get()
                    .cancelled(mNativeDangerousDownloadDialogBridge, guid);
        }
        if (windowAndroid != null) {
            NewDownloadTab.closeExistingNewDownloadTab(windowAndroid);
        }
    }

    @NativeMethods
    interface Natives {
        void accepted(
                long nativeDangerousDownloadDialogBridge, @JniType("std::string") String guid);

        void cancelled(
                long nativeDangerousDownloadDialogBridge, @JniType("std::string") String guid);
    }
}
