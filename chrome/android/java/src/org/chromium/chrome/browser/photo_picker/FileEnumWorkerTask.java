// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.Manifest;
import android.content.Intent;
import android.os.Environment;
import android.provider.MediaStore;

import org.chromium.base.ThreadUtils;
import org.chromium.base.task.AsyncTask;
import org.chromium.ui.base.WindowAndroid;

import java.io.File;
import java.util.ArrayList;
import java.util.Collections;
import java.util.List;

/**
 * A worker task to enumerate image files on disk.
 */
class FileEnumWorkerTask extends AsyncTask<List<PickerBitmap>> {
    /**
     * An interface to use to communicate back the results to the client.
     */
    public interface FilesEnumeratedCallback {
        /**
         * A callback to define to receive the list of all images on disk.
         * @param files The list of images.
         */
        void filesEnumeratedCallback(List<PickerBitmap> files);
    }

    private final WindowAndroid mWindowAndroid;

    // The callback to use to communicate the results.
    private FilesEnumeratedCallback mCallback;

    // The filter to apply to the list.
    private MimeTypeFileFilter mFilter;

    // The camera directory undir DCIM.
    private static final String SAMPLE_DCIM_SOURCE_SUB_DIRECTORY = "Camera";

    /**
     * A FileEnumWorkerTask constructor.
     * @param windowAndroid The window wrapper associated with the current activity.
     * @param callback The callback to use to communicate back the results.
     * @param filter The file filter to apply to the list.
     */
    public FileEnumWorkerTask(WindowAndroid windowAndroid, FilesEnumeratedCallback callback,
            MimeTypeFileFilter filter) {
        mWindowAndroid = windowAndroid;
        mCallback = callback;
        mFilter = filter;
    }

    /**
     * Retrieves the DCIM/camera directory.
     */
    private File getCameraDirectory() {
        return new File(Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DCIM),
                SAMPLE_DCIM_SOURCE_SUB_DIRECTORY);
    }

    /**
     * Recursively enumerate files in a directory (and subdirectories) and add them to a list.
     * @param directory The parent directory to recursively traverse.
     * @param pickerBitmaps The list to add the results to.
     * @return True if traversing can continue, false if traversing was aborted and should stop.
     */
    private boolean traverseDir(File directory, List<PickerBitmap> pickerBitmaps) {
        File[] files = directory.listFiles(mFilter);
        if (files == null) return true;

        for (File file : files) {
            if (isCancelled()) return false;

            if (file.isDirectory()) {
                if (!traverseDir(file, pickerBitmaps)) return false;
            } else {
                pickerBitmaps.add(new PickerBitmap(
                        file.getPath(), file.lastModified(), PickerBitmap.TileTypes.PICTURE));
            }
        }

        return true;
    }

    /**
     * Enumerates (in the background) the image files on disk. Called on a non-UI thread
     * @param params Ignored, do not use.
     * @return A sorted list of images (by last-modified first).
     */
    @Override
    protected List<PickerBitmap> doInBackground() {
        assert !ThreadUtils.runningOnUiThread();

        if (isCancelled()) return null;

        List<PickerBitmap> pickerBitmaps = new ArrayList<>();

        // TODO(finnur): Figure out which directories to scan and stop hard coding "Camera" above.
        File[] sourceDirs = new File[] {
                getCameraDirectory(),
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_PICTURES),
                Environment.getExternalStoragePublicDirectory(Environment.DIRECTORY_DOWNLOADS),
        };

        for (File directory : sourceDirs) {
            if (!traverseDir(directory, pickerBitmaps)) return null;
        }

        Collections.sort(pickerBitmaps);

        pickerBitmaps.add(0, new PickerBitmap("", 0, PickerBitmap.TileTypes.GALLERY));
        boolean hasCameraAppAvailable =
                mWindowAndroid.canResolveActivity(new Intent(MediaStore.ACTION_IMAGE_CAPTURE));
        boolean hasOrCanRequestCameraPermission =
                mWindowAndroid.hasPermission(Manifest.permission.CAMERA)
                || mWindowAndroid.canRequestPermission(Manifest.permission.CAMERA);
        if (hasCameraAppAvailable && hasOrCanRequestCameraPermission) {
            pickerBitmaps.add(0, new PickerBitmap("", 0, PickerBitmap.TileTypes.CAMERA));
        }

        return pickerBitmaps;
    }

    /**
     * Communicates the results back to the client. Called on the UI thread.
     * @param files The resulting list of files on disk.
     */
    @Override
    protected void onPostExecute(List<PickerBitmap> files) {
        if (isCancelled()) {
            return;
        }

        mCallback.filesEnumeratedCallback(files);
    }
}
