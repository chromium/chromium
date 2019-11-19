// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.provider.MediaStore;
import android.text.TextUtils;

import androidx.annotation.NonNull;

import org.chromium.base.BuildInfo;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.task.AsyncTask;

import java.io.File;
import java.io.FileNotFoundException;

/**
 * Includes helper methods to interact with Android media store.
 */
public class MediaStoreHelper {
    private static final String TAG = "MediaStoreHelper";

    private MediaStoreHelper() {}

    /**
     * Adds an image file on external SD card to media store to show in the Android gallery app.
     * The images on primary storage will automatically scanned by media store. On external SD card,
     * usually .nomedia file will block the media store scan request.
     * The media store will decode, compress the image and maintain a copy on disk.
     * Does nothing if the file is not an image or the file is not on external SD card.
     * @param filePath The file path of the image file.
     * @param mimeType The mime type of the image file.
     */
    public static void addImageToGalleryOnSDCard(String filePath, String mimeType) {
        // TODO(xingliu): Support Android Q when we have available device with SD card slot.
        if (TextUtils.isEmpty(filePath) || mimeType == null || !mimeType.startsWith("image/")
                || BuildInfo.isAtLeastQ()) {
            return;
        }

        DownloadDirectoryProvider.getInstance().getAllDirectoriesOptions((dirs) -> {
            for (DirectoryOption dir : dirs) {
                // Scan the media file if it is on SD card.
                if (dir.type == DirectoryOption.DownloadLocationDirectoryType.ADDITIONAL
                        && filePath.contains(dir.location)) {
                    addImageOnBlockingThread(filePath);
                    return;
                }
            }
        });
    }

    /**
     * Adds the image to media store on a blocking thread.
     * @param filePath The file path of the image file.
     */
    private static void addImageOnBlockingThread(@NonNull String filePath) {
        new AsyncTask<Void>() {
            @Override
            protected Void doInBackground() {
                try {
                    // The media store will decode the image to bitmap, compress, and maintain a
                    // copy on disk.
                    File file = new File(filePath);
                    MediaStore.Images.Media.insertImage(
                            ContextUtils.getApplicationContext().getContentResolver(), filePath,
                            file.getName(), null);
                } catch (FileNotFoundException e) {
                    Log.e(TAG, "Cannot find image file to add to gallery.", e);
                }
                return null;
            }

            @Override
            protected void onPostExecute(Void aVoid) {}
        }.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }
}
