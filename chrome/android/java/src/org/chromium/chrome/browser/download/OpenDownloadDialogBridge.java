// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.content.pm.ResolveInfo;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.app.download.home.DownloadActivityLauncher;
import org.chromium.chrome.browser.download.dialogs.OpenDownloadDialog;
import org.chromium.chrome.browser.download.dialogs.OpenDownloadDialog.OpenDownloadDialogEvent;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

import java.util.List;

/** Glues open download dialogs UI code and handles the communication to download native backend. */
public class OpenDownloadDialogBridge {
    private long mNativeOpenDownloadDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @param nativeOpenDownloadDialogBridge Pointer to the native object.
     */
    public OpenDownloadDialogBridge(long nativeOpenDownloadDialogBridge) {
        mNativeOpenDownloadDialogBridge = nativeOpenDownloadDialogBridge;
    }

    @CalledByNative
    public static OpenDownloadDialogBridge create(long nativeDialog) {
        return new OpenDownloadDialogBridge(nativeDialog);
    }

    /**
     * Called to show a warning dialog for opening a download.
     *
     * @param profile Profile of the user.
     * @param guid GUID of the download.
     */
    @CalledByNative
    public void showDialog(Profile profile, @JniType("std::string") String guid) {
        List<ResolveInfo> result = MimeUtils.getPdfIntentHandlers();
        if (result.size() == 0) {
            onCancel(guid);
            return;
        }
        DownloadActivityLauncher.getInstance()
                .getActivityForOpenDialog(
                        (activity) -> {
                            if (activity == null) {
                                onCancel(guid);
                                return;
                            }
                            showOpenDownloadDialog(
                                    profile,
                                    guid,
                                    activity,
                                    result.size() > 1 ? null : MimeUtils.getDefaultPdfViewerName());
                        });
    }

    /**
     * Show a warning dialog for opening a download.
     *
     * @param profile Profile of the user.
     * @param guid GUID of the download.
     * @param activity Activity that displays the dialog.
     * @param appName Name of the app to open the file, null if there are multiple apps.
     */
    private void showOpenDownloadDialog(
            Profile profile, String guid, Activity activity, String appName) {
        new OpenDownloadDialog()
                .show(
                        activity,
                        ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                        UserPrefs.get(profile).getBoolean(Pref.AUTO_OPEN_PDF_ENABLED),
                        appName,
                        (result) -> {
                            if (result
                                    == OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN) {
                                UserPrefs.get(profile).setBoolean(Pref.AUTO_OPEN_PDF_ENABLED, true);
                                onConfirmed(guid);
                                recordOpenDownloadDialogEvent(
                                        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN);
                            } else if (result
                                    == OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_JUST_ONCE) {
                                onConfirmed(guid);
                                recordOpenDownloadDialogEvent(
                                        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_JUST_ONCE);
                            } else {
                                onCancel(guid);
                                recordOpenDownloadDialogEvent(
                                        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_DISMISS);
                            }
                        });
        recordOpenDownloadDialogEvent(OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_SHOW);
    }

    @CalledByNative
    private void destroy() {
        mNativeOpenDownloadDialogBridge = 0;
    }

    private void onConfirmed(String guid) {
        OpenDownloadDialogBridgeJni.get()
                .onConfirmed(mNativeOpenDownloadDialogBridge, guid, /* accepted= */ true);
    }

    private void onCancel(String guid) {
        OpenDownloadDialogBridgeJni.get()
                .onConfirmed(mNativeOpenDownloadDialogBridge, guid, /* accepted= */ false);
    }

    /**
     * Collects open download dialog UI event metrics.
     *
     * @param event The UI event to collect.
     */
    private static void recordOpenDownloadDialogEvent(@OpenDownloadDialogEvent int event) {
        RecordHistogram.recordEnumeratedHistogram(
                "Download.Android.OpenDialog.Events", event, OpenDownloadDialogEvent.COUNT);
    }

    @NativeMethods
    interface Natives {
        void onConfirmed(
                long nativeOpenDownloadDialogBridge,
                @JniType("std::string") String guid,
                boolean accepted);
    }
}
