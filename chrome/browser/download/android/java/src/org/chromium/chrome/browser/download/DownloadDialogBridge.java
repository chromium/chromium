// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.widget.Toast;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.Log;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.chrome.browser.download.DownloadLocationDialogType;
import org.chromium.chrome.browser.download.interstitial.NewDownloadTab;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.prefs.PrefService;
import org.chromium.components.user_prefs.UserPrefs;
import org.chromium.net.ConnectionType;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;

/**
 * Glues download dialogs UI code and handles the communication to download native backend.
 * When {@link ChromeFeatureList#DOWNLOAD_LATER} is enabled, the following dialogs will be shown in
 * a sequence.
 * Download later dialog ==> (optional) Download date time picker ==> Download location dialog
 * When {@link ChromeFeatureList#DOWNLOAD_LATER} is disabled, only the download location dialog will
 * be shown.
 */
public class DownloadDialogBridge {
    private static final long INVALID_START_TIME = -1;
    private long mNativeDownloadDialogBridge;

    private Context mContext;
    private WindowAndroid mWindowAndroid;
    private long mTotalBytes;
    private @ConnectionType
    int mConnectionType = ConnectionType.CONNECTION_NONE;
    private String mSuggestedPath;
    private PrefService mPrefService;

    // Whether the user clicked the edit text to open download location dialog.
    private boolean mEditLocation;

    // Whether to show the edit location text in download later dialog.
    private boolean mShowEditLocation;

    private long mDownloadLaterTime = INVALID_START_TIME;


    private static DownloadDialogFactory sFactory;

    public interface IDownloadDialog {

        IDownloadDialog show(Context context);

    }

    public interface DownloadDialogFactory {

        void showDialog(DownloadDialogBridge downloadDialogBridge,
                                     @NonNull Activity activity,
                                     long totalBytes,
                                     @ConnectionType int connectionType,
                                     @DownloadLocationDialogType int dialogType,
                                     String suggestedPath, boolean supportsLaterDialog,
                                     boolean isIncognito);

    }

    public static void setDownloadDialogFactory(DownloadDialogFactory factory) {
        sFactory = factory;
    }

    public DownloadDialogBridge(long nativeDownloadDialogBridge) {
        mNativeDownloadDialogBridge = nativeDownloadDialogBridge;
    }

    @CalledByNative
    private static DownloadDialogBridge create(long nativeDownloadDialogBridge) {
        DownloadDialogBridge bridge = new DownloadDialogBridge(nativeDownloadDialogBridge);
        return bridge;
    }

    @CalledByNative
    void destroy() {
        mNativeDownloadDialogBridge = 0;
    }

    @CalledByNative
    private void showDialog(@Nullable WindowAndroid windowAndroid, long totalBytes,
                            @ConnectionType int connectionType, @DownloadLocationDialogType int dialogType,
                            String suggestedPath, boolean supportsLaterDialog, boolean isIncognito) {


        mWindowAndroid = windowAndroid;
        Activity activity;
//        if (activity == null) {
//            activity = ApplicationStatus.getLastTrackedFocusedActivity();
//            Log.e("DownloadDialogBridge", "showDialog getLastTrackedFocusedActivity activity=" + activity);
//            if (activity == null) {
//                onCancel();
//                return;
//            }
//        }

        if (windowAndroid == null) {
            activity = ApplicationStatus.getLastTrackedFocusedActivity();
        } else {
            activity = windowAndroid.getActivity().get();
        }
        Log.e("DownloadDialogBridge", "showDialog windowAndroid="
                + windowAndroid + " activity=" + activity);

        if (activity == null) {
            onCancel();
            return;
        }

        sFactory.showDialog(this, activity, totalBytes,
                        connectionType, dialogType, suggestedPath,
                        supportsLaterDialog, isIncognito);

//        this.mSuggestedPath = suggestedPath;
//        this.mConnectionType = connectionType;
//        this.mTotalBytes = totalBytes;
//
//        Log.e("DownloadDialogBridge",
//                "showDialog totalBytes=%s, connectionType=%s, dialogType=%s, supportsLaterDialog=%s, isIncognito=%s, suggestedPath=%s",
//                totalBytes, connectionType, dialogType, supportsLaterDialog, isIncognito, suggestedPath);
//
//        AlertDialog dialog;
//        if (dialogType == DownloadLocationDialogType.DANGEROUS) {
//            dialog = new AlertDialog.Builder(activity)
//                    .setTitle("Download Dangerous File?")
//                    .setMessage("Are you sure to download this dangerous file? ")
//                    .setPositiveButton(org.chromium.chrome.browser.download.R.string.ok, (dialog13, which) -> {
//                        onComplete(mSuggestedPath);
//                        dialog13.dismiss();
//                    })
//                    .setNegativeButton(org.chromium.chrome.browser.download.R.string.cancel, (dialog12, which) -> {
//                        onCancel();
//                        dialog12.dismiss();
//                    })
//                    .setOnCancelListener(dialog1 -> DownloadDialogBridge.this.onCancel())
//                    .create();
//        } else {
//            dialog = new AlertDialog.Builder(activity)
//                    .setTitle("Download File?")
//                    .setMessage("Are you sure to download file? ")
//                    .setPositiveButton(org.chromium.chrome.browser.download.R.string.ok, (dialog13, which) -> {
//                        File targetFile = new File(mSuggestedPath);
//                        if (targetFile.exists()) {
//                            File parentFile = targetFile.getParentFile();
//                            if (parentFile == null) {
//                                parentFile = new File(activity.getExternalCacheDir(), "download");
//                            }
//                            String parent = parentFile.getAbsolutePath();
//                            String name = targetFile.getName();
//                            int i = name.lastIndexOf('.');
//                            if (i < 0) {
//                                mSuggestedPath = generateNewFilePath(parent, name, null);
//                            } else {
//                                mSuggestedPath = generateNewFilePath(parent, name.substring(0, i), name.substring(i));
//                            }
//                        }
//                        Log.e("DownloadDialogBridge", "setPositiveButton mSuggestedPath=%s", mSuggestedPath);
//                        onComplete(mSuggestedPath);
//                        dialog13.dismiss();
//                    })
//                    .setNegativeButton(org.chromium.chrome.browser.download.R.string.cancel, (dialog12, which) -> {
//                        onCancel();
//                        dialog12.dismiss();
//                    })
//                    .setOnCancelListener(dialog1 -> DownloadDialogBridge.this.onCancel())
//                    .create();
//        }
//
//
//        dialog.show();
    }

    private static String generateNewFilePath(String parent, String name, @Nullable String suffix) {
        if (suffix == null) {
            suffix = "";
        }
        int index = name.length() - 1;
        char last = name.charAt(index);
        String newName = name;
        int num = 1;
        if (last == ')') {
            while (--index >= 0) {
                last = name.charAt(index);
                if (last == '(') {
                    try {
                        num = Integer.parseInt(name.substring(index + 1, name.length() - 1)) + 1;
                        newName = name.substring(0, index);
                    } catch (Exception ignore) {
                    }
                } else if (!Character.isDigit(last)) {
                    break;
                }
            }
        }

        File newFile;
        do {
            String newNameWithSuffix = String.format("%s(%s)%s", newName, num++, suffix);
            newFile = new File(parent, newNameWithSuffix);
        } while (newFile.exists());
        return newFile.getAbsolutePath();
    }

    public void onComplete(String suggestedPath) {
        if (mNativeDownloadDialogBridge == 0) return;

        DownloadDialogBridgeJni.get().onComplete(mNativeDownloadDialogBridge,
                DownloadDialogBridge.this, suggestedPath, false, mDownloadLaterTime);
        if (mWindowAndroid != null) {
            mWindowAndroid = null;
        }
    }

    public void onCancel() {
        if (mNativeDownloadDialogBridge == 0) return;
        DownloadDialogBridgeJni.get().onCanceled(
                mNativeDownloadDialogBridge, DownloadDialogBridge.this);
        if (mWindowAndroid != null) {
            mWindowAndroid = null;
        }
    }

    /**
     * @return The stored download default directory.
     */
    public static String getDownloadDefaultDirectory() {
        return DownloadDialogBridgeJni.get().getDownloadDefaultDirectory();
    }

    /**
     * @param directory New directory to set as the download default directory.
     */
    public static void setDownloadAndSaveFileDefaultDirectory(String directory) {
        DownloadDialogBridgeJni.get().setDownloadAndSaveFileDefaultDirectory(directory);
    }

    /**
     * @return The status of prompt for download pref, defined by {@link DownloadPromptStatus}.
     */
    @DownloadPromptStatus
    public static int getPromptForDownloadAndroid() {
        return getPrefService().getInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID);
    }

    /**
     * @param status New status to update the prompt for download preference.
     */
    public static void setPromptForDownloadAndroid(@DownloadPromptStatus int status) {
        getPrefService().setInteger(Pref.PROMPT_FOR_DOWNLOAD_ANDROID, status);
    }

    /**
     * @return The value for {@link Pref#PROMPT_FOR_DOWNLOAD}. This is currently only used by
     * enterprise policy.
     */
    public static boolean getPromptForDownloadPolicy() {
        return getPrefService().getBoolean(Pref.PROMPT_FOR_DOWNLOAD);
    }

    /**
     * @return whether to prompt the download location dialog is controlled by enterprise policy.
     */
    public static boolean isLocationDialogManaged() {
        return DownloadDialogBridgeJni.get().isLocationDialogManaged();
    }

    public static boolean shouldShowDateTimePicker() {
        return DownloadDialogBridgeJni.get().shouldShowDateTimePicker();
    }

    private static PrefService getPrefService() {
        return UserPrefs.get(Profile.getLastUsedRegularProfile());
    }

    @NativeMethods
    public interface Natives {
        void onComplete(long nativeDownloadDialogBridge, DownloadDialogBridge caller,
                        String returnedPath, boolean onWifi, long startTime);

        void onCanceled(long nativeDownloadDialogBridge, DownloadDialogBridge caller);

        String getDownloadDefaultDirectory();

        void setDownloadAndSaveFileDefaultDirectory(String directory);

        long getDownloadLaterMinFileSize();

        boolean shouldShowDateTimePicker();

        boolean isLocationDialogManaged();
    }
}
