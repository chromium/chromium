// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.content.Context;
import android.hardware.Camera;
import android.util.AttributeSet;
import android.view.Display;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.IOException;

/**
 * Provides a SurfaceView and adapts it for use as a camera preview target
 * so that the current camera image can be displayed.
 *
 * TODO: locking and unlocking the screen seems to stop the camera preview because, on unlock,
 * multiple of these Views end up getting created and only one wins the race to the camera.
 */
public class CameraView extends SurfaceView implements SurfaceHolder.Callback {
    private static final String TAG = "CameraView";
    private Camera.PreviewCallback mCallback;
    private Display mDisplay;

    /**
     * Holds a reference to the camera. Only referenced from the UI thread.
     */
    private Camera mCamera;
    /**
     * Contains the number of degrees that the image from the selected camera
     * will be rotated. Only referenced from the UI thread.
     */
    private int mCameraRotation;
    /**
     * True if a background thread is trying to open the camera. Only referenced from the UI thread.
     */
    private boolean mAmOpeningCamera;
    /**
      True if this View is currently detached. If this occurs while the camera is being opened
      then it needs to immediately be closed again. Only referenced from the UI thread.
    */
    private boolean mDetached;
    private SurfaceHolder mHolder;

    public CameraView(Context context, AttributeSet attributeSet) {
        super(context, attributeSet);
    }

    public void setCallback(Camera.PreviewCallback callback) {
        mCallback = callback;
    }

    public void setDisplay(Display display) {
        mDisplay = display;
    }

    /**
     * Called to indicate that the callback that was passed to the constructor has finished
     * processing and thus is free to receive another camera frame.
     */
    void rearmCallback() {
        ThreadUtils.assertOnUiThread();

        if (mCamera != null) {
            mCamera.setOneShotPreviewCallback(mCallback);
        }
    }

    @Override
    protected void onAttachedToWindow() {
        ThreadUtils.assertOnUiThread();

        super.onAttachedToWindow();
        mDetached = false;
        getHolder().addCallback(this);
    }

    @Override
    protected void onDetachedFromWindow() {
        ThreadUtils.assertOnUiThread();

        super.onDetachedFromWindow();
        mDetached = true;
        getHolder().removeCallback(this);
        if (mCamera != null) {
            mCamera.release();
            mCamera = null;
        }
    }

    @Override
    protected void onMeasure(int widthMeasureSpec, int heightMeasureSpec) {
        // This view is only used in a context where the width is set by the
        // container.
        int width = MeasureSpec.getSize(widthMeasureSpec);

        // The aspect ratio of the camera's preview is only available after
        // opening it. But that's a slow operation and is performed on a
        // background thread, while the layout needs to be done immediately.
        // Therefore assume a 16:9 aspect ratio, which is the most common.
        // If the ratio turns out to be 4:3, there will be some slight
        // distortion in the image, but it'll still work.
        setMeasuredDimension(width, (16 * width) / 9);
    }

    private void openCamera() {
        // We want to find the first, rear-facing camera. This is what
        // Camera.open() gives us, but then we don't get the camera ID and we
        // need that to get the rotation amount. Thus the need to iterate
        // over the cameras to find the right one.
        final int numCameras = Camera.getNumberOfCameras();
        if (numCameras == 0) {
            // TODO: indicate in UI when QR scanning fails.
            return;
        }

        Camera.CameraInfo info = new Camera.CameraInfo();
        boolean found = false;
        int cameraId;
        for (cameraId = 0; cameraId < numCameras; cameraId++) {
            Camera.getCameraInfo(cameraId, info);
            if (info.facing == Camera.CameraInfo.CAMERA_FACING_BACK) {
                found = true;
                break;
            }
        }

        if (!found) {
            // No rear facing cameras available. Just use the first camera.
            cameraId = 0;
            Camera.getCameraInfo(cameraId, info);
        }

        Camera camera;
        try {
            camera = Camera.open(cameraId);
        } catch (RuntimeException e) {
            Log.w(TAG, "Failed to open camera", e);
            // TODO: indicate in UI when QR scanning fails.
            return;
        }

        // This logic is based on
        // https://developer.android.com/reference/android/hardware/Camera.html#setDisplayOrientation(int)
        // But the sample code there appears to be wrong in practice.
        int rotation;
        if (info.facing == Camera.CameraInfo.CAMERA_FACING_BACK) {
            rotation = info.orientation;
        } else {
            rotation = (360 - info.orientation) % 360;
        }

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> startCamera(camera, rotation));
    }

    private void startCamera(Camera camera, int cameraRotation) {
        ThreadUtils.assertOnUiThread();

        mAmOpeningCamera = false;

        if (mDetached) {
            // View was detached while the camera was being opened.
            camera.release();
            return;
        }

        mCamera = camera;
        mCameraRotation = cameraRotation;

        if (mHolder == null) {
            // Surface was lost while the camera was being opened.
            return;
        }

        try {
            mCamera.setPreviewDisplay(mHolder);
            // Use a one-shot callback so that callbacks don't happen faster
            // they're processed.
            mCamera.setOneShotPreviewCallback(mCallback);
            Camera.Parameters parameters = mCamera.getParameters();
            parameters.setFocusMode(Camera.Parameters.FOCUS_MODE_CONTINUOUS_PICTURE);
            mCamera.setParameters(parameters);

            int displayRotation = 0;
            // getRotation returns the opposite of the rotation of the physical
            // display. (I.e. it returns the rotation that needs to be applied
            // in order to correct for the rotation of the screen.) Thus 90/270
            // are swapped.
            switch (mDisplay.getRotation()) {
                case Surface.ROTATION_0:
                    displayRotation = 0;
                    break;
                case Surface.ROTATION_90:
                    displayRotation = 270;
                    break;
                case Surface.ROTATION_180:
                    displayRotation = 180;
                    break;
                case Surface.ROTATION_270:
                    displayRotation = 90;
                    break;
            }
            mCamera.setDisplayOrientation((mCameraRotation + displayRotation) % 360);

            mCamera.startPreview();
        } catch (IOException e) {
            Log.w(TAG, "Exception while starting camera", e);
        }
    }

    /** SurfaceHolder.Callback implementation. */
    @Override
    public void surfaceCreated(SurfaceHolder holder) {
        ThreadUtils.assertOnUiThread();

        mHolder = holder;
        if (mAmOpeningCamera) {
            return;
        }

        if (mCamera == null) {
            mAmOpeningCamera = true;
            PostTask.postTask(TaskTraits.USER_VISIBLE_MAY_BLOCK, this::openCamera);
        } else {
            startCamera(mCamera, mCameraRotation);
        }
    }

    @Override
    public void surfaceDestroyed(SurfaceHolder holder) {
        ThreadUtils.assertOnUiThread();

        mHolder = null;
        if (mCamera != null) {
            mCamera.setOneShotPreviewCallback(null);
            mCamera.stopPreview();
        }
    }

    @Override
    public void surfaceChanged(SurfaceHolder holder, int format, int w, int h) {
        surfaceDestroyed(holder);
        surfaceCreated(holder);
    }
}
