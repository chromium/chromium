// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webauth.authenticator;

import android.content.DialogInterface;
import android.graphics.ImageFormat;
import android.hardware.Camera;
import android.os.Bundle;
import android.util.SparseArray;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.CheckBox;
import android.widget.LinearLayout;

import androidx.fragment.app.DialogFragment;

import com.google.android.gms.vision.Frame;
import com.google.android.gms.vision.barcode.Barcode;
import com.google.android.gms.vision.barcode.BarcodeDetector;

import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.nio.ByteBuffer;

/**
 * Displays a preview of what the default (rear) camera can see and processes images for QR
 * codes. Closes once an applicable QR code has been found.
 *
 * (Needs to be public because of the way that the Android system works.)
 */
public class QRScanDialog extends DialogFragment implements Camera.PreviewCallback {
    /**
     * FIDO QR codes begin with this prefix. This class will ignore QR codes that don't match
     * this.
     */
    public static final String FIDO_QR_PREFIX = "fido://";
    private static final String TAG = "QRScanDialog";
    /**
     * Receives a single call containing the decoded QR value. It will
     * begin with FIDO_QR_PREFIX.
     */
    public static interface Callback { void onQRCode(String value, boolean link); }

    private final Callback mCallback;
    private BarcodeDetector mQRScanner;
    private CheckBox mLinkCheckbox;
    private CameraView mCameraView;
    private ByteBuffer mBuffer;
    private boolean mDismissed;

    QRScanDialog(Callback callback) {
        super();
        mCallback = callback;
    }

    @Override
    public View onCreateView(
            LayoutInflater inflater, ViewGroup container, Bundle savedInstanceState) {
        View v = inflater.inflate(R.layout.cablev2_qr_dialog, container, false);
        // CameraView is not referenced from the XML in order to avoid issues
        // with reflection of custom Views inside of split and isolated
        // modules.
        mCameraView = new CameraView(getContext(), null);
        ViewGroup.LayoutParams params = new ViewGroup.LayoutParams(
                ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.WRAP_CONTENT);

        ((LinearLayout) v.findViewById(R.id.qr_dialog_layout))
                .addView(mCameraView, /*index=*/0, params);

        mCameraView.setCallback(this);
        mCameraView.setDisplay(getActivity().getWindowManager().getDefaultDisplay());

        mLinkCheckbox = v.findViewById(R.id.link_checkbox);
        return v;
    }

    @Override
    public void onPreviewFrame(byte[] data, Camera camera) {
        if (mBuffer == null || mBuffer.capacity() < data.length) {
            mBuffer = ByteBuffer.allocate(data.length);
        } else {
            mBuffer.clear();
        }
        mBuffer.put(data);

        PostTask.postTask(TaskTraits.USER_VISIBLE_MAY_BLOCK,
                () -> findQRCodes(mBuffer, camera.getParameters()));
    }

    @Override
    public void onDismiss(DialogInterface dialog) {
        mDismissed = true;
    }

    /**
     * Potentially loads GmsCore modules for QR detection and performs QR detection on the image in
     * {@link #mBuffer}. Runs on a background thread.
     */
    private void findQRCodes(ByteBuffer buffer, Camera.Parameters cameraParams) {
        if (mQRScanner == null) {
            // This can trigger a load of GmsCore modules, which is too
            // much to do on the main thread.
            mQRScanner = new BarcodeDetector.Builder(getContext()).build();
            if (mQRScanner == null) {
                Log.i(TAG, "BarcodeDetector failed to load");
                PostTask.postTask(UiThreadTaskTraits.DEFAULT, this::dismiss);
                return;
            }
        }

        // From
        // https://developer.android.com/reference/android/hardware/Camera.PreviewCallback.html#onPreviewFrame(byte%5B%5D,%20android.hardware.Camera)
        // "If Camera.Parameters.setPreviewFormat(int) is never called, the default will be
        // the YCbCr_420_SP (NV21) format."
        Frame frame = new Frame.Builder()
                              .setImageData(buffer, cameraParams.getPreviewSize().width,
                                      cameraParams.getPreviewSize().height, ImageFormat.NV21)
                              .build();
        SparseArray<Barcode> barcodes = mQRScanner.detect(frame);
        PostTask.postTask(UiThreadTaskTraits.DEFAULT, () -> handleQRCodes(barcodes));
    }

    /**
     * Handles the results of QR detection. Runs on the UI thread.
     */
    private void handleQRCodes(SparseArray<Barcode> barcodes) {
        ThreadUtils.assertOnUiThread();

        // This dialog may have been dismissed while background QR detection was
        // running.
        if (mDismissed) {
            return;
        }

        for (int i = 0; i < barcodes.size(); i++) {
            String value = barcodes.valueAt(i).rawValue;
            if (value.startsWith(FIDO_QR_PREFIX)) {
                mCallback.onQRCode(value, mLinkCheckbox.isChecked());
                dismiss();
                return;
            }
        }

        mCameraView.rearmCallback();
    }
}
