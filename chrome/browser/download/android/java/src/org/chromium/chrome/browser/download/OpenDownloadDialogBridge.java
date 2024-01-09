// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;

import androidx.annotation.IntDef;

import org.jni_zero.CalledByNative;
import org.jni_zero.NativeMethods;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.chrome.browser.download.dialogs.OpenDownloadDialog;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManagerHolder;

/** Glues open download dialogs UI code and handles the communication to download native backend. */
public class OpenDownloadDialogBridge {
    /**
     * Events related to the open download dialog, used for UMA reporting. These values are
     * persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_SHOW,
        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN,
        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_JUST_ONCE,
        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_DISMISS
    })
    private @interface OpenDownloadDialogEvent {
        int OPEN_DOWNLOAD_DIALOG_SHOW = 0;
        int OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN = 1;
        int OPEN_DOWNLOAD_DIALOG_JUST_ONCE = 2;
        int OPEN_DOWNLOAD_DIALOG_DISMISS = 3;

        int COUNT = 4;
    }

    private long mNativeOpenDownloadDialogBridge;

    /**
     * Constructor, taking a pointer to the native instance.
     *
     * @nativeOpenDownloadDialogBridge Pointer to the native object.
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
     * @param windowAndroid Window to show the dialog.
     * @param guid GUID of the download.
     * @param fileName Name of the download file.
     */
    @CalledByNative
    public void showDialog(WindowAndroid windowAndroid, Profile profile, String guid) {
        Activity activity = windowAndroid.getActivity().get();
        if (activity == null) {
            onCancel(guid, windowAndroid);
            return;
        }

        new OpenDownloadDialog()
                .show(
                        activity,
                        ((ModalDialogManagerHolder) activity).getModalDialogManager(),
                        (result) -> {
                            if (result == DialogDismissalCause.POSITIVE_BUTTON_CLICKED) {
                                UserPrefs.get(profile).setBoolean(Pref.AUTO_OPEN_PDF_ENABLED, true);
                                recordOpenDownloadDialogEvent(
                                        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_ALWAYS_OPEN);
                                onConfirmed(guid);
                            } else if (result == DialogDismissalCause.NEGATIVE_BUTTON_CLICKED) {
                                recordOpenDownloadDialogEvent(
                                        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_JUST_ONCE);
                                onConfirmed(guid);
                            } else {
                                recordOpenDownloadDialogEvent(
                                        OpenDownloadDialogEvent.OPEN_DOWNLOAD_DIALOG_DISMISS);
                                onCancel(guid, windowAndroid);
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

    private void onCancel(String guid, WindowAndroid windowAndroid) {
        OpenDownloadDialogBridgeJni.get()
                .onConfirmed(mNativeOpenDownloadDialogBridge, guid, /* accepted= */ false);
        NewDownloadTab.closeExistingNewDownloadTab(windowAndroid);
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
        void onConfirmed(long nativeOpenDownloadDialogBridge, String guid, boolean accepted);
    }
}
