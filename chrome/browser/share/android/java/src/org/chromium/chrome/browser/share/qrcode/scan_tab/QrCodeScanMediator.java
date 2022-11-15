// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.Manifest.permission;
import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.net.Uri;
import android.os.Process;
import android.provider.Browser;
import android.util.SparseArray;
import android.webkit.URLUtil;

import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;

import org.chromium.base.BuildInfo;
import org.chromium.base.Callback;
import org.chromium.base.IntentUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.AsyncTask;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.document.ChromeLauncherActivity;
import org.chromium.components.permissions.AndroidPermissionRequester;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modaldialog.DialogDismissalCause;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.permissions.ActivityAndroidPermissionDelegate;
import org.chromium.ui.permissions.AndroidPermissionDelegate;
import org.chromium.ui.permissions.PermissionCallback;
import org.chromium.ui.widget.Toast;

import java.lang.ref.WeakReference;
import java.nio.ByteBuffer;
import java.util.function.Consumer;

/**
 * QrCodeScanMediator is in charge of calculating and setting values for QrCodeScanViewProperties.
 */
public class QrCodeScanMediator implements Camera.PreviewCallback {
    /** Interface used for notifying in the event of navigation to a URL. */
    public interface NavigationObserver { void onNavigation(); }

    private final Context mContext;
    private final PropertyModel mPropertyModel;
    private final NavigationObserver mNavigationObserver;
    private final AndroidPermissionDelegate mPermissionDelegate;

    private BarcodeDetector mDetector;
    private Toast mToast;
    private WindowAndroid mWindowAndroid;

    /**
     * The QrCodeScanMediator constructor.
     *
     * @param context The context to use for user permissions.
     * @param propertyModel The property modelto use to communicate with views.
     * @param observer The observer for navigation event.
     */
    QrCodeScanMediator(Context context, PropertyModel propertyModel, NavigationObserver observer,
            WindowAndroid windowAndroid) {
        mContext = context;
        mPropertyModel = propertyModel;
        mPermissionDelegate = new ActivityAndroidPermissionDelegate(
                new WeakReference<Activity>((Activity) mContext));
        updatePermissionSettings();
        mNavigationObserver = observer;
        mWindowAndroid = windowAndroid;

        // Set detector to null until it gets initialized asynchronously.
        mDetector = null;
        initBarcodeDetectorAsync();
    }

    /** Returns whether the user has granted camera permissions. */
    private Boolean hasCameraPermission() {
        return mContext.checkPermission(permission.CAMERA, Process.myPid(), Process.myUid())
                == PackageManager.PERMISSION_GRANTED;
    }

    /** Returns whether the user can be prompted for camera permissions. */
    private Boolean canPromptForPermission() {
        return mPermissionDelegate.canRequestPermission(permission.CAMERA);
    }

    /** Updates the permission settings with the latest values. */
    private void updatePermissionSettings() {
        mPropertyModel.set(
                QrCodeScanViewProperties.CAN_PROMPT_FOR_PERMISSION, canPromptForPermission());
        mPropertyModel.set(QrCodeScanViewProperties.HAS_CAMERA_PERMISSION, hasCameraPermission());
    }

    /**
     * Sets whether QrCode UI is on foreground.
     *
     * @param isOnForeground Indicates whether this component UI is current on foreground.
     */
    public void setIsOnForeground(boolean isOnForeground) {
        // If the app is in the foreground, the permissions need to be checked again to ensure
        // the user is seeing the right thing.
        if (isOnForeground) {
            updatePermissionSettings();
        }
        // This is intentionally done last so that the view is updated according to the latest
        // permissions.
        mPropertyModel.set(QrCodeScanViewProperties.IS_ON_FOREGROUND, isOnForeground);
    }

    /**
     * Prompts the user for camera permission and processes the results.
     */
    public void promptForCameraPermission() {
        requestCameraAccessPermissionHelper(mWindowAndroid, granted -> {
            if (granted) {
                mPropertyModel.set(QrCodeScanViewProperties.HAS_CAMERA_PERMISSION, true);
            } else {
                // The order in which these fields are important because it causes updates to
                // the view. CanPromptForPermission must be updated first so that it doesn't
                // cause the view to be updated twice creating a flicker effect.
                if (!mPermissionDelegate.canRequestPermission(permission.CAMERA)) {
                    mPropertyModel.set(QrCodeScanViewProperties.CAN_PROMPT_FOR_PERMISSION, false);
                }
                mPropertyModel.set(QrCodeScanViewProperties.HAS_CAMERA_PERMISSION, false);
            }
        });
    }

    private void requestCameraAccessPermissionHelper(
            WindowAndroid windowAndroid, final Callback<Boolean> callback) {
        AndroidPermissionDelegate permissionDelegate = windowAndroid;

        if (windowAndroid.hasPermission(permission.CAMERA)) {
            callback.onResult(true);
            return;
        }

        if (mContext == null) {
            callback.onResult(false);
            return;
        }

        Consumer<PropertyModel> requestPermissions = (model) -> {
            final PermissionCallback permissionCallback = (permissions, grantResults) -> {
                ModalDialogManager modalDialogManager = windowAndroid.getModalDialogManager();
                if (modalDialogManager != null && model != null) {
                    modalDialogManager.dismissDialog(
                            model, DialogDismissalCause.POSITIVE_BUTTON_CLICKED);
                }
                callback.onResult(grantResults.length > 0
                        && grantResults[0] == PackageManager.PERMISSION_GRANTED);
            };

            permissionDelegate.requestPermissions(
                    new String[] {permission.CAMERA}, permissionCallback);
        };

        if (windowAndroid.getModalDialogManager() != null) {
            AndroidPermissionRequester.showMissingPermissionDialog(windowAndroid,
                    mContext.getString(
                            org.chromium.chrome.R.string.infobar_missing_camera_permission_text,
                            BuildInfo.getInstance().hostPackageLabel),
                    requestPermissions, callback.bind(false));
        }
    }

    /**
     * Processes data received from the camera preview to detect QR/barcode containing URLs. If
     * found, navigates to it by creating a new tab. If not found registers for camera preview
     * callback again. Runs on the same thread that was used to open the camera.
     */
    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        if (mDetector == null) {
            return;
        }

        ByteBuffer buffer = ByteBuffer.allocate(data.length);
        buffer.put(data);
        Frame frame =
                new Frame.Builder()
                        .setImageData(buffer, camera.getParameters().getPreviewSize().width,
                                camera.getParameters().getPreviewSize().height, ImageFormat.NV21)
                        .build();
        SparseArray<Barcode> barcodes = mDetector.detect(frame);
        if (!mPropertyModel.get(QrCodeScanViewProperties.IS_ON_FOREGROUND)) {
            return;
        }
        if (barcodes.size() == 0) {
            camera.setOneShotPreviewCallback(this);
            return;
        }

        Barcode firstCode = barcodes.valueAt(0);
        if (!URLUtil.isValidUrl(firstCode.rawValue)) {
            String toastMessage =
                    mContext.getString(R.string.qr_code_not_a_url_label, firstCode.rawValue);
            if (mToast != null) {
                mToast.cancel();
            }
            mToast = Toast.makeText(mContext, toastMessage, Toast.LENGTH_LONG);
            mToast.show();
            RecordUserAction.record("SharingQRCode.ScannedNonURL");
            camera.setOneShotPreviewCallback(this);
            return;
        }

        openUrl(firstCode.rawValue);
        mNavigationObserver.onNavigation();
        RecordUserAction.record("SharingQRCode.ScannedURL");
    }

    private void openUrl(String url) {
        Intent intent =
                new Intent()
                        .setAction(Intent.ACTION_VIEW)
                        .setData(Uri.parse(url))
                        .setClass(mContext, ChromeLauncherActivity.class)
                        .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK | Intent.FLAG_ACTIVITY_NEW_DOCUMENT)
                        .putExtra(Browser.EXTRA_APPLICATION_ID, mContext.getPackageName())
                        .putExtra(WebappConstants.REUSE_URL_MATCHING_TAB_ELSE_NEW_TAB, true);
        IntentUtils.addTrustedIntentExtras(intent);
        mContext.startActivity(intent);
    }

    private void initBarcodeDetectorAsync() {
        new AsyncTask<BarcodeDetector>() {
            @Override
            protected BarcodeDetector doInBackground() {
                return new BarcodeDetector.Builder(mContext).build();
            }

            @Override
            protected void onPostExecute(BarcodeDetector detector) {
                mDetector = detector;
            }
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }
}
