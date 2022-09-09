// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode.scan_tab;

import android.app.Activity;
import android.content.Context;
import android.content.Intent;
import android.hardware.Camera;
import android.hardware.Camera.ErrorCallback;
import android.hardware.Camera.PreviewCallback;
import android.net.Uri;
import android.provider.Settings;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.ViewGroup.LayoutParams;
import android.widget.FrameLayout;
import android.widget.TextView;

import org.chromium.base.task.PostTask;
import org.chromium.chrome.R;
import org.chromium.content_public.browser.UiThreadTaskTraits;
import org.chromium.ui.widget.ButtonCompat;

/**
 * Manages the Android View representing the QrCode scan panel.
 */
class QrCodeScanView {
    public interface PermissionPrompter { void promptForCameraPermission(); }

    public interface PermissionPromptAllowedChecker { Boolean canPromptForPermission(); }

    private final Context mContext;
    private final FrameLayout mView;
    private final PreviewCallback mCameraPreviewCallback;

    private boolean mHasCameraPermission;
    private boolean mCanPromptForPermission;
    private boolean mIsOnForeground;
    private CameraPreview mCameraPreview;
    private View mPermissionsView;
    private View mCameraErrorView;
    private View mOpenSettingsView;

    /**
     * The QrCodeScanView constructor.
     *
     * @param context The context to use for user permissions.
     * @param cameraCallback The callback to processing camera preview.
     */
    public QrCodeScanView(Context context, PreviewCallback cameraCallback,
            PermissionPrompter permissionPrompter) {
        mContext = context;
        mCameraPreviewCallback = cameraCallback;
        mView = new FrameLayout(context);
        mOpenSettingsView = createOpenSettingsView(context);
        mView.setLayoutParams(
                new FrameLayout.LayoutParams(LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT));
        mPermissionsView = createPermissionView(context, permissionPrompter);
        mCameraErrorView = createCameraErrorView(context);
    }

    private View createPermissionView(Context context, PermissionPrompter permissionPrompter) {
        View permissionView = (View) LayoutInflater.from(context).inflate(
                R.layout.qrcode_permission_layout, null, false);

        ButtonCompat cameraPermissionPrompt = permissionView.findViewById(R.id.ask_for_permission);
        cameraPermissionPrompt.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                permissionPrompter.promptForCameraPermission();
            }
        });
        return permissionView;
    }

    private View createCameraErrorView(Context context) {
        return (View) LayoutInflater.from(context).inflate(
                R.layout.qrcode_camera_error_layout, null, false);
    }

    private final ErrorCallback mCameraErrorCallback = new ErrorCallback() {
        @Override
        public void onError(int error, Camera camera) {
            int stringResource;
            switch (error) {
                case Camera.CAMERA_ERROR_EVICTED:
                case Camera.CAMERA_ERROR_SERVER_DIED:
                case CameraPreview.CAMERA_IN_USE_ERROR:
                    stringResource = R.string.qr_code_in_use_camera_error;
                    break;
                case CameraPreview.NO_CAMERA_FOUND_ERROR:
                    stringResource = R.string.qr_code_no_camera_error;
                    break;
                case CameraPreview.CAMERA_DISABLED_ERROR:
                    stringResource = R.string.qr_code_disabled_camera_error;
                    break;
                default:
                    stringResource = R.string.qr_code_hardware_camera_error;
            }
            if (mCameraPreview != null) {
                mCameraPreview.stopCamera();
                mCameraPreview = null;
            }

            String errorString = mContext.getResources().getString(stringResource);
            // displayCameraErrorDialog should be called from the UI thread.
            PostTask.runOrPostTask(
                    UiThreadTaskTraits.DEFAULT, () -> displayCameraErrorDialog(errorString));
        }
    };

    public View getView() {
        return mView;
    }

    /**
     * Creates a view that opens the settings page for the app and allows the user to to update
     * permissions including give the app camera permission.
     */
    private View createOpenSettingsView(Context context) {
        View openSettingsView = (View) LayoutInflater.from(context).inflate(
                R.layout.qrcode_open_settings_layout, null, false);

        ButtonCompat cameraPermissionPrompt =
                openSettingsView.findViewById(R.id.open_settings_button);
        cameraPermissionPrompt.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                Intent openSettingsIntent = getAppInfoIntent(context.getPackageName());
                ((Activity) context).startActivity(openSettingsIntent);
            }
        });
        return openSettingsView;
    }

    /**
     * Sets camera if possible.
     *
     * @param hasCameraPermission Indicates whether camera permissions were granted.
     */
    public void cameraPermissionsChanged(Boolean hasCameraPermission) {
        // No change, nothing to do here
        // We need to make sure mHasCameraPermission was not set to false already as that
        // is the default value and therefore nothing will get rendered the first time.
        if (mHasCameraPermission && hasCameraPermission) {
            return;
        }
        mHasCameraPermission = hasCameraPermission;
        updateView();
    }

    /**
     * Update the view based on the latest environment: - app is in the foreground - user has given
     * camera permission - user can be prompted for camera permission
     */
    private void updateView() {
        // The scan tab is not in the foreground so don't do any rendering.
        if (!mIsOnForeground) {
            return;
        }

        // Check that the camera permission has changed and that it is now set to true.
        if (mHasCameraPermission && mCameraPreview == null) {
            setCameraPreview();
        } else if (mHasCameraPermission && mCameraPreview != null) {
            updateCameraPreviewState();
        } else if (mCanPromptForPermission) {
            displayPermissionDialog();
        } else {
            displayOpenSettingsDialog();
        }
    }

    /**
     * Checks whether Chrome can prompt the user for Camera permission. Updates the view accordingly
     * to let the user know if the permission has been permanently denied.
     *
     * @param canPromptForPermission Indicates whether the user can be prompted for camera
     *            permission
     */
    public void canPromptForPermissionChanged(Boolean canPromptForPermission) {
        mCanPromptForPermission = canPromptForPermission;
        updateView();
    }

    /**
     * Applies changes necessary to camera preview.
     *
     * @param isOnForeground Indicates whether this component UI is currently on foreground.
     */
    public void onForegroundChanged(Boolean isOnForeground) {
        mIsOnForeground = isOnForeground;
        if (!mIsOnForeground && mCameraPreview != null) {
            mCameraPreview.stopCamera();
        } else {
            updateView();
        }
    }

    /** Creates and sets the camera preview. */
    private void setCameraPreview() {
        mView.removeAllViews();
        stopCamera();

        if (mHasCameraPermission) {
            mCameraPreview =
                    new CameraPreview(mContext, mCameraPreviewCallback, mCameraErrorCallback);
            mView.addView(mCameraPreview);
            mView.addView(new CameraPreviewOverlay(mContext));

            updateCameraPreviewState();
        }
    }

    /** Starts or stops camera if necessary. */
    private void updateCameraPreviewState() {
        if (mCameraPreview == null) {
            return;
        }

        if (mIsOnForeground && mHasCameraPermission) {
            mCameraPreview.startCamera();
        } else {
            mCameraPreview.stopCamera();
        }
    }

    /**
     * Displays the permission dialog. Caller should check that the user can be prompted and hasn't
     * permanently denied permission.
     */
    private void displayPermissionDialog() {
        mView.removeAllViews();
        mView.addView(mPermissionsView);
    }

    /** Displays the camera error dialog. */
    private void displayCameraErrorDialog(String errorString) {
        TextView cameraErrorTextView =
                (TextView) mCameraErrorView.findViewById(R.id.qrcode_camera_error_text);
        cameraErrorTextView.setText(errorString);

        mView.removeAllViews();
        mView.addView(mCameraErrorView);
    }

    /**
     * Returns an Intent to show the App Info page for the current app.
     */
    private Intent getAppInfoIntent(String packageName) {
        Intent intent = new Intent(Settings.ACTION_APPLICATION_DETAILS_SETTINGS);
        intent.setData(new Uri.Builder().scheme("package").opaquePart(packageName).build());
        return intent;
    }

    /**
     * Displays the open settings dialog.
     */
    private void displayOpenSettingsDialog() {
        mView.removeAllViews();
        mView.addView(mOpenSettingsView);
    }

    /**
     * Stop the camera.
     */
    public void stopCamera() {
        if (mCameraPreview != null) {
            mCameraPreview.stopCamera();
            mCameraPreview = null;
        }
    }
}
