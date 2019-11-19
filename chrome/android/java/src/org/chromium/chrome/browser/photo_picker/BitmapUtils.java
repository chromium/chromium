// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.graphics.Bitmap;
import android.graphics.BitmapFactory;
import android.graphics.Matrix;
import android.media.ExifInterface;
import android.media.MediaMetadataRetriever;
import android.os.Build;

import org.chromium.base.metrics.RecordHistogram;

import java.io.FileDescriptor;
import java.io.IOException;
import java.util.ArrayList;
import java.util.List;

/**
 * A collection of utility functions for dealing with bitmaps.
 */
class BitmapUtils {
    // Constants used to log UMA enum histogram, must stay in sync with the
    // ExifOrientation enum in enums.xml. Further actions can only be appended,
    // existing entries must not be overwritten.
    private static final int EXIF_ORIENTATION_NORMAL = 0;
    private static final int EXIF_ORIENTATION_ROTATE_90 = 1;
    private static final int EXIF_ORIENTATION_ROTATE_180 = 2;
    private static final int EXIF_ORIENTATION_ROTATE_270 = 3;
    private static final int EXIF_ORIENTATION_TRANSPOSE = 4;
    private static final int EXIF_ORIENTATION_TRANSVERSE = 5;
    private static final int EXIF_ORIENTATION_FLIP_HORIZONTAL = 6;
    private static final int EXIF_ORIENTATION_FLIP_VERTICAL = 7;
    private static final int EXIF_ORIENTATION_UNDEFINED = 8;
    private static final int EXIF_ORIENTATION_ACTION_BOUNDARY = 9;

    /**
     * Takes a |bitmap| and returns a square thumbnail of |size|x|size| from the center of the
     * bitmap specified, rotating it according to the Exif information, if needed (on Nougat and
     * up only).
     * @param bitmap The bitmap to adjust.
     * @param size The desired size (width and height).
     * @param descriptor The file descriptor to read the Exif information from.
     * @return The new bitmap thumbnail.
     */
    private static Bitmap sizeBitmap(Bitmap bitmap, int size, FileDescriptor descriptor) {
        // TODO(finnur): Investigate options that require fewer bitmaps to be created.
        bitmap = ensureMinSize(bitmap, size);
        bitmap = rotateAndCropToSquare(bitmap, size, descriptor);
        return bitmap;
    }

    /**
     * Given a FileDescriptor, decodes the contents and returns a bitmap of
     * dimensions |size|x|size|.
     * @param descriptor The FileDescriptor for the file to read.
     * @param size The width and height of the bitmap to return.
     * @return The resulting bitmap.
     */
    public static Bitmap decodeBitmapFromFileDescriptor(FileDescriptor descriptor, int size) {
        BitmapFactory.Options options = new BitmapFactory.Options();
        options.inJustDecodeBounds = true;
        BitmapFactory.decodeFileDescriptor(descriptor, null, options);
        options.inSampleSize = calculateInSampleSize(options.outWidth, options.outHeight, size);
        options.inJustDecodeBounds = false;
        Bitmap bitmap = BitmapFactory.decodeFileDescriptor(descriptor, null, options);

        if (bitmap == null) return null;

        return sizeBitmap(bitmap, size, descriptor);
    }

    /**
     * Given a FileDescriptor, decodes the video and returns a bitmap of dimensions |size|x|size|.
     * @param retriever The MediaMetadataRetriever to use (must have source already set).
     * @param descriptor The FileDescriptor for the file to read.
     * @param size The width and height of the bitmap to return.
     * @param frames The number of frames to extract.
     * @param intervalMs The interval between frames (in milliseconds).
     * @return A list of extracted frames.
     */
    public static List<Bitmap> decodeVideoFromFileDescriptor(MediaMetadataRetriever retriever,
            FileDescriptor descriptor, int size, int frames, long intervalMs) {
        List<Bitmap> bitmaps = new ArrayList<Bitmap>();
        Bitmap bitmap = null;
        for (int frame = 0; frame < frames; ++frame) {
            bitmap = retriever.getFrameAtTime(frame * intervalMs * 1000);
            if (bitmap == null) continue;

            bitmap = sizeBitmap(bitmap, size, descriptor);
            bitmaps.add(bitmap);
        }

        return bitmaps;
    }

    /**
     * Calculates the sub-sampling factor {@link BitmapFactory#inSampleSize} option for a given
     * image dimensions, which will be used to create a bitmap of a pre-determined size (as small as
     * possible without either dimension shrinking below |minSize|.
     * @param width The calculated width of the image to decode.
     * @param height The calculated height of the image to decode.
     * @param minSize The maximum size the image should be (in either dimension).
     * @return The sub-sampling factor (power of two: 1 = no change, 2 = half-size, etc).
     */
    private static int calculateInSampleSize(int width, int height, int minSize) {
        int inSampleSize = 1;
        if (width > minSize && height > minSize) {
            inSampleSize = Math.min(width, height) / minSize;
        }
        return inSampleSize;
    }

    /**
     * Ensures a |bitmap| is at least |size| in both width and height.
     * @param bitmap The bitmap to modify.
     * @param size The minimum size (width and height).
     * @return The resulting (scaled) bitmap.
     */
    private static Bitmap ensureMinSize(Bitmap bitmap, int size) {
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        if (width == size && height == size) return bitmap;

        if (width > size && height > size) {
            // Both sides are larger than requested, which will lead to excessive amount of
            // cropping. Shrink to a more manageable amount (shorter side becomes |size| in length).
            float scale = (width < height) ? (float) width / size : (float) height / size;
            width = Math.round(width / scale);
            height = Math.round(height / scale);
            return Bitmap.createScaledBitmap(bitmap, width, height, true);
        }

        if (width < size) {
            float scale = (float) size / width;
            width = size;
            height = (int) (height * scale);
        }

        if (height < size) {
            float scale = (float) size / height;
            height = size;
            width = (int) (width * scale);
        }

        return Bitmap.createScaledBitmap(bitmap, width, height, true);
    }

    /**
     * Records the Exif histogram value for a photo.
     * @param sample The sample to record.
     */
    private static void recordExifHistogram(int sample) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.PhotoPicker.ExifOrientation", sample, EXIF_ORIENTATION_ACTION_BOUNDARY);
    }

    /**
     * Crops a |bitmap| to a certain square |size| and rotates it according to the Exif information,
     * if needed (on Nougat and up only).
     * @param bitmap The bitmap to crop.
     * @param size The size desired (width and height).
     * @return The resulting (square) bitmap.
     */
    private static Bitmap rotateAndCropToSquare(
            Bitmap bitmap, int size, FileDescriptor descriptor) {
        Matrix matrix = new Matrix();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
            try {
                ExifInterface exif = new ExifInterface(descriptor);
                int rotation = exif.getAttributeInt(
                        ExifInterface.TAG_ORIENTATION, ExifInterface.ORIENTATION_UNDEFINED);
                switch (rotation) {
                    case ExifInterface.ORIENTATION_NORMAL:
                        recordExifHistogram(EXIF_ORIENTATION_NORMAL);
                        break;
                    case ExifInterface.ORIENTATION_ROTATE_90:
                        matrix.postRotate(90);
                        recordExifHistogram(EXIF_ORIENTATION_ROTATE_90);
                        break;
                    case ExifInterface.ORIENTATION_ROTATE_180:
                        matrix.postRotate(180);
                        recordExifHistogram(EXIF_ORIENTATION_ROTATE_180);
                        break;
                    case ExifInterface.ORIENTATION_ROTATE_270:
                        matrix.postRotate(-90);
                        recordExifHistogram(EXIF_ORIENTATION_ROTATE_270);
                        break;
                    case ExifInterface.ORIENTATION_TRANSPOSE:
                        matrix.setRotate(90);
                        matrix.postScale(-1, 1);
                        recordExifHistogram(EXIF_ORIENTATION_TRANSPOSE);
                        break;
                    case ExifInterface.ORIENTATION_TRANSVERSE:
                        matrix.setRotate(-90);
                        matrix.postScale(-1, 1);
                        recordExifHistogram(EXIF_ORIENTATION_TRANSVERSE);
                        break;
                    case ExifInterface.ORIENTATION_FLIP_HORIZONTAL:
                        matrix.setScale(-1, 1);
                        recordExifHistogram(EXIF_ORIENTATION_FLIP_HORIZONTAL);
                        break;
                    case ExifInterface.ORIENTATION_FLIP_VERTICAL:
                        matrix.setScale(1, -1);
                        recordExifHistogram(EXIF_ORIENTATION_FLIP_VERTICAL);
                        break;
                    case ExifInterface.ORIENTATION_UNDEFINED:
                        recordExifHistogram(EXIF_ORIENTATION_UNDEFINED);
                        break;
                    default:
                        break;
                }
            } catch (IOException e) {
            }
        }

        int x = 0;
        int y = 0;
        int width = bitmap.getWidth();
        int height = bitmap.getHeight();
        if (width == size && height == size) return bitmap;

        if (width > size) x = (width - size) / 2;
        if (height > size) y = (height - size) / 2;
        return Bitmap.createBitmap(bitmap, x, y, size, size, matrix, true);
    }

    /**
     * Scales a |bitmap| to a certain size.
     * @param bitmap The bitmap to scale.
     * @param scaleMaxSize What to scale it to.
     * @param filter True if the source should be filtered.
     * @return The resulting scaled bitmap.
     */
    public static Bitmap scale(Bitmap bitmap, float scaleMaxSize, boolean filter) {
        float ratio = Math.min((float) scaleMaxSize / bitmap.getWidth(),
                (float) scaleMaxSize / bitmap.getHeight());
        int height = Math.round(ratio * bitmap.getHeight());
        int width = Math.round(ratio * bitmap.getWidth());

        return Bitmap.createScaledBitmap(bitmap, width, height, filter);
    }
}
