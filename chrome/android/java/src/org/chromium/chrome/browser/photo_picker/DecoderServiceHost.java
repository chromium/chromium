// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.photo_picker;

import android.content.ComponentName;
import android.content.ContentResolver;
import android.content.Context;
import android.content.Intent;
import android.content.ServiceConnection;
import android.content.res.AssetFileDescriptor;
import android.graphics.Bitmap;
import android.net.Uri;
import android.os.Build;
import android.os.Bundle;
import android.os.IBinder;
import android.os.ParcelFileDescriptor;
import android.os.RemoteException;
import android.os.StrictMode;
import android.os.SystemClock;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Log;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.AsyncTask;
import org.chromium.base.task.PostTask;
import org.chromium.chrome.browser.util.ConversionUtils;
import org.chromium.content_public.browser.UiThreadTaskTraits;

import java.io.FileNotFoundException;
import java.io.IOException;
import java.util.ArrayList;
import java.util.LinkedHashMap;
import java.util.List;

/**
 * A class to communicate with the {@link DecoderService}.
 */
public class DecoderServiceHost
        extends IDecoderServiceCallback.Stub implements DecodeVideoTask.VideoDecodingCallback {
    // A tag for logging error messages.
    private static final String TAG = "ImageDecoderHost";

    // A content resolver for providing file descriptors for the images.
    private ContentResolver mContentResolver;

    // The number of successful image decodes (not video), per batch.
    private int mSuccessfulImageDecodes;

    // The number of runtime failures during image decoding (not video), per batch.
    private int mFailedImageDecodesRuntime;

    // The number of out of memory failures during image decoding (not video), per batch.
    private int mFailedImageDecodesMemory;

    // The number of successful video decodes, per batch.
    private int mSuccessfulVideoDecodes;

    // The number of file errors during video decoding, per batch.
    private int mFailedVideoDecodesFile;

    // The number of runtime failures during video decoding, per batch.
    private int mFailedVideoDecodesRuntime;

    // The number of io failures during video decoding, per batch.
    private int mFailedVideoDecodesIo;

    // The number of io failures during video decoding, per batch.
    private int mFailedVideoDecodesUnknown;

    // A worker task for asynchronously handling video decode requests.
    private DecodeVideoTask mWorkerTask;

    // A callback to use for testing to see if decoder is ready.
    static ServiceReadyCallback sReadyCallbackForTesting;

    IDecoderService mIRemoteService;
    private ServiceConnection mConnection = new ServiceConnection() {
        @Override
        public void onServiceConnected(ComponentName className, IBinder service) {
            mIRemoteService = IDecoderService.Stub.asInterface(service);
            mBound = true;
            for (ServiceReadyCallback callback : mCallbacks) {
                callback.serviceReady();
            }
        }

        @Override
        public void onServiceDisconnected(ComponentName className) {
            Log.e(TAG, "Service has unexpectedly disconnected");
            mIRemoteService = null;
            mBound = false;
        }
    };

    /**
     * Interface for notifying clients of the service being ready.
     */
    public interface ServiceReadyCallback {
        /**
         * A function to define to receive a notification once the service is up and running.
         */
        void serviceReady();
    }

    /**
     * An interface notifying clients when all images have finished decoding.
     */
    public interface ImagesDecodedCallback {
        /**
         * A function to define to receive a notification that an image has been decoded.
         * @param filePath The file path for the newly decoded image.
         * @param isVideo Whether the decoding was from a video or not.
         * @param bitmaps The results of the decoding (or placeholder image, if failed).
         * @param videoDuration The time-length of the video (null if not a video).
         */
        void imagesDecodedCallback(
                String filePath, boolean isVideo, List<Bitmap> bitmaps, String videoDuration);
    }

    /**
     * Class for keeping track of the data involved with each request.
     */
    private static class DecoderServiceParams {
        // The URI for the file containing the bitmap to decode.
        public Uri mUri;

        // The requested size (width and height) of the bitmap, once decoded.
        public int mSize;

        // The type of media being decoded.
        @PickerBitmap.TileTypes
        int mFileType;

        // The callback to use to communicate the results of the decoding.
        ImagesDecodedCallback mCallback;

        // The timestamp for when the request was sent for decoding.
        long mTimestamp;

        public DecoderServiceParams(Uri uri, int size, @PickerBitmap.TileTypes int fileType,
                ImagesDecodedCallback callback) {
            mUri = uri;
            mSize = size;
            mFileType = fileType;
            mCallback = callback;
        }
    }

    // Map of file paths to pending decoding requests of high priority.
    private LinkedHashMap<String, DecoderServiceParams> mHighPriorityRequests =
            new LinkedHashMap<>();

    // Map of file paths to pending decoding requests of low priority.
    private LinkedHashMap<String, DecoderServiceParams> mLowPriorityRequests =
            new LinkedHashMap<>();

    // Map of file paths to processing decoding requests.
    private LinkedHashMap<String, DecoderServiceParams> mProcessingRequests = new LinkedHashMap<>();

    // The callbacks used to notify the clients when the service is ready.
    List<ServiceReadyCallback> mCallbacks = new ArrayList<ServiceReadyCallback>();

    // Flag indicating whether we are bound to the service.
    private boolean mBound;

    private final Context mContext;

    /**
     * The DecoderServiceHost constructor.
     * @param callback The callback to use when communicating back to the client.
     */
    public DecoderServiceHost(ServiceReadyCallback callback, Context context) {
        mCallbacks.add(callback);
        if (sReadyCallbackForTesting != null) {
            mCallbacks.add(sReadyCallbackForTesting);
        }
        mContext = context;
        mContentResolver = mContext.getContentResolver();
    }

    /**
     * Initiate binding with the {@link DecoderService}.
     * @param context The context to use.
     */
    public void bind(Context context) {
        Intent intent = new Intent(mContext, DecoderService.class);
        intent.setAction(IDecoderService.class.getName());
        mContext.bindService(intent, mConnection, Context.BIND_AUTO_CREATE);
    }

    /**
     * Unbind from the {@link DecoderService}.
     * @param context The context to use.
     */
    public void unbind(Context context) {
        if (mBound) {
            context.unbindService(mConnection);
            mBound = false;
        }
    }

    /**
     * Accepts a request to decode a single image. Queues up the request and reports back
     * asynchronously on |callback|.
     * @param uri The URI of the file to decode.
     * @param fileType The type of image being sent for decoding.
     * @param size The requested size (width and height) of the resulting bitmap.
     * @param callback The callback to use to communicate the decoding results.
     */
    public void decodeImage(Uri uri, @PickerBitmap.TileTypes int fileType, int size,
            ImagesDecodedCallback callback) {
        DecoderServiceParams params = new DecoderServiceParams(uri, size, fileType, callback);
        mHighPriorityRequests.put(uri.getPath(), params);
        if (mProcessingRequests.size() == 0) dispatchNextDecodeRequest();
    }

    /**
     * Fetches the next high-priority decoding request from the queue and removes it from the queue.
     * If that request is a video decoding request, a request for decoding additional frames is
     * added to the low-priority queue.
     * @return Next high-priority request pending.
     */
    private DecoderServiceParams getNextHighPriority() {
        assert mHighPriorityRequests.size() > 0;
        DecoderServiceParams params = mHighPriorityRequests.entrySet().iterator().next().getValue();
        mHighPriorityRequests.remove(params.mUri.getPath());
        if (params.mFileType == PickerBitmap.TileTypes.VIDEO) {
            // High-priority decoding requests for videos are requests for first frames (see
            // dispatchDecodeVideoRequest). Adding another low-priority request is a request for
            // decoding the rest of the frames.
            DecoderServiceParams lowPriorityRequest = new DecoderServiceParams(
                    params.mUri, params.mSize, params.mFileType, params.mCallback);
            mLowPriorityRequests.put(params.mUri.getPath(), lowPriorityRequest);
        }
        return params;
    }

    /**
     * Fetches the next low-priority decoding request from the queue and removes it from the queue.
     * @return Next low-priority request pending, or null. Null can be returned in two scenarios:
     *         If no requests remain or if the only request remaining is a low-priority request
     *         where it's high-priority counterpart is still being processed.
     */
    private DecoderServiceParams getNextLowPriority() {
        for (DecoderServiceParams request : mLowPriorityRequests.values()) {
            String filePath = request.mUri.getPath();
            if (mProcessingRequests.get(filePath) != null) continue;
            mLowPriorityRequests.remove(filePath);
            return request;
        }
        return null;
    }

    /**
     * Dispatches the next image/video for decoding (from the queue).
     */
    private void dispatchNextDecodeRequest() {
        boolean highPriority = mHighPriorityRequests.entrySet().iterator().hasNext();
        DecoderServiceParams params = highPriority ? getNextHighPriority() : getNextLowPriority();
        if (params != null) {
            mProcessingRequests.put(params.mUri.getPath(), params);

            params.mTimestamp = SystemClock.elapsedRealtime();
            if (params.mFileType != PickerBitmap.TileTypes.VIDEO) {
                dispatchDecodeImageRequest(params);
            } else {
                dispatchDecodeVideoRequest(params, highPriority);
            }
            return;
        }

        if (mProcessingRequests.entrySet().iterator().hasNext()) return;

        int totalImageRequests =
                mSuccessfulImageDecodes + mFailedImageDecodesRuntime + mFailedImageDecodesMemory;
        if (totalImageRequests > 0) {
            // Calculate and transmit UMA for image decoding.
            int runtimeFailures = 100 * mFailedImageDecodesRuntime / totalImageRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostFailureRuntime", runtimeFailures);

            int memoryFailures = 100 * mFailedImageDecodesMemory / totalImageRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostFailureOutOfMemory", memoryFailures);

            mSuccessfulImageDecodes = 0;
            mFailedImageDecodesRuntime = 0;
            mFailedImageDecodesMemory = 0;
        }

        int totalVideoRequests = mSuccessfulVideoDecodes + mFailedVideoDecodesFile
                + mFailedVideoDecodesRuntime + mFailedVideoDecodesIo + mFailedVideoDecodesUnknown;
        if (totalVideoRequests > 0) {
            // Calculate and transmit UMA for video decoding.
            int videoFileFailures = 100 * mFailedVideoDecodesFile / totalVideoRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostVideoFileError", videoFileFailures);

            int videoRuntimeFailures = 100 * mFailedVideoDecodesRuntime / totalVideoRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostVideoRuntimeError", videoRuntimeFailures);

            int videoIoFailures = 100 * mFailedVideoDecodesIo / totalVideoRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostVideoIoError", videoIoFailures);

            int videoUnknownFailures = 100 * mFailedVideoDecodesUnknown / totalVideoRequests;
            RecordHistogram.recordPercentageHistogram(
                    "Android.PhotoPicker.DecoderHostVideoUnknownError", videoUnknownFailures);

            mSuccessfulVideoDecodes = 0;
            mFailedVideoDecodesFile = 0;
            mFailedVideoDecodesRuntime = 0;
            mFailedVideoDecodesIo = 0;
            mFailedVideoDecodesUnknown = 0;
        }
    }

    /**
     * A callback that receives the results of the video decoding.
     * @param uri The uri of the decoded video.
     * @param bitmaps The thumbnails representing the decoded video.
     * @param duration The video duration (a formatted human-readable string, for example "3:00").
     * @param decodingResult Whether the decoding was successful.
     */
    @Override
    public void videoDecodedCallback(Uri uri, List<Bitmap> bitmaps, String duration,
            @DecodeVideoTask.DecodingResult int decodingResult) {
        switch (decodingResult) {
            case DecodeVideoTask.DecodingResult.SUCCESS:
                if (bitmaps == null || bitmaps.size() == 0) {
                    mFailedVideoDecodesUnknown++;
                } else {
                    mSuccessfulVideoDecodes++;
                }
                break;
            case DecodeVideoTask.DecodingResult.FILE_ERROR:
                mFailedVideoDecodesFile++;
                break;
            case DecodeVideoTask.DecodingResult.RUNTIME_ERROR:
                mFailedVideoDecodesRuntime++;
                break;
            case DecodeVideoTask.DecodingResult.IO_ERROR:
                mFailedVideoDecodesIo++;
                break;
        }

        closeRequest(uri.getPath(), true, bitmaps, duration, -1);
    }

    @Override
    public void onDecodeImageDone(final Bundle payload) {
        // As per the Android documentation, AIDL callbacks can (and will) happen on any thread, so
        // make sure the code runs on the UI thread, since further down the callchain the code will
        // end up creating UI objects.
        PostTask.runOrPostTask(UiThreadTaskTraits.DEFAULT, () -> {
            String filePath = "";
            List<Bitmap> bitmaps = null;
            String videoDuration = null;
            long decodeTime = -1;
            try {
                // Read the reply back from the service.
                filePath = payload.getString(DecoderService.KEY_FILE_PATH);
                Boolean success = payload.getBoolean(DecoderService.KEY_SUCCESS);
                Bitmap bitmap = success
                        ? (Bitmap) payload.getParcelable(DecoderService.KEY_IMAGE_BITMAP)
                        : null;
                decodeTime = payload.getLong(DecoderService.KEY_DECODE_TIME);
                mSuccessfulImageDecodes++;
                bitmaps = new ArrayList<>(1);
                bitmaps.add(bitmap);
            } catch (RuntimeException e) {
                mFailedImageDecodesRuntime++;
            } catch (OutOfMemoryError e) {
                mFailedImageDecodesMemory++;
            } finally {
                closeRequest(
                        filePath, /*isVideo=*/false, bitmaps, /*videoDuration=*/null, decodeTime);
            }
        });
    }

    /**
     * Ties up all the loose ends from the decoding request (communicates the results of the
     * decoding process back to the client, and takes care of house-keeping chores regarding
     * the request queue).
     * @param filePath The path to the image that was just decoded.
     * @param isVideo True if the request was for video decoding.
     * @param bitmaps The resulting decoded bitmaps, or null if decoding fails.
     * @param decodeTime The length of time it took to decode the bitmap.
     */
    public void closeRequest(String filePath, boolean isVideo, @Nullable List<Bitmap> bitmaps,
            String videoDuration, long decodeTime) {
        DecoderServiceParams params = mProcessingRequests.get(filePath);
        if (params != null) {
            long endRpcCall = SystemClock.elapsedRealtime();
            if (isVideo) {
                if (bitmaps.size() > 1) {
                    RecordHistogram.recordTimesHistogram(
                            "Android.PhotoPicker.RequestProcessTimeAnimation",
                            endRpcCall - params.mTimestamp);
                } else {
                    RecordHistogram.recordTimesHistogram(
                            "Android.PhotoPicker.RequestProcessTimeThumbnail",
                            endRpcCall - params.mTimestamp);
                }
            } else {
                RecordHistogram.recordTimesHistogram(
                        "Android.PhotoPicker.RequestProcessTime", endRpcCall - params.mTimestamp);
            }

            params.mCallback.imagesDecodedCallback(filePath, isVideo, bitmaps, videoDuration);

            if (decodeTime != -1 && bitmaps != null && bitmaps.get(0) != null) {
                int sizeInKB = bitmaps.get(0).getByteCount() / ConversionUtils.BYTES_PER_KILOBYTE;
                if (isVideo) {
                    if (bitmaps.size() > 1) {
                        RecordHistogram.recordTimesHistogram(
                                "Android.PhotoPicker.VideoDecodeTimeAnimation", decodeTime);
                    } else {
                        RecordHistogram.recordTimesHistogram(
                                "Android.PhotoPicker.VideoDecodeTimeThumbnail", decodeTime);
                        RecordHistogram.recordCustomCountHistogram(
                                "Android.PhotoPicker.VideoByteCount", sizeInKB, 1, 100000, 50);
                    }
                } else {
                    RecordHistogram.recordTimesHistogram(
                            "Android.PhotoPicker.ImageDecodeTime", decodeTime);
                    RecordHistogram.recordCustomCountHistogram(
                            "Android.PhotoPicker.ImageByteCount", sizeInKB, 1, 100000, 50);
                }
            }
            mProcessingRequests.remove(filePath);
        }

        dispatchNextDecodeRequest();
    }

    /**
     * Communicates with the utility process to decode a single video.
     * @param params The information about the decoding request.
     * @param highPriority True if the decoding request is a high-priority request.
     */
    private void dispatchDecodeVideoRequest(DecoderServiceParams params, boolean highPriority) {
        // Videos are decoded by the system (on N+) using a restricted helper process, so
        // there's no need to use our custom sandboxed process.
        assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;

        int frames = highPriority ? 1 : 10;
        int intervalMs = 2000;
        mWorkerTask = new DecodeVideoTask(
                this, mContentResolver, params.mUri, params.mSize, frames, intervalMs);
        mWorkerTask.executeOnExecutor(AsyncTask.THREAD_POOL_EXECUTOR);
    }

    /**
     * Communicates with the server to decode a single bitmap.
     * @param params The information about the decoding request.
     */
    private void dispatchDecodeImageRequest(DecoderServiceParams params) {
        // Obtain a file descriptor to send over to the sandboxed process.
        ParcelFileDescriptor pfd = null;
        Bundle bundle = new Bundle();

        // The restricted utility process can't open the file to read the
        // contents, so we need to obtain a file descriptor to pass over.
        StrictMode.ThreadPolicy oldPolicy = StrictMode.allowThreadDiskReads();
        try {
            AssetFileDescriptor afd = null;
            try {
                afd = mContentResolver.openAssetFileDescriptor(params.mUri, "r");
            } catch (FileNotFoundException e) {
                Log.e(TAG, "Unable to obtain FileDescriptor: " + e);
                closeRequest(params.mUri.getPath(), false, null, null, -1);
                return;
            }
            pfd = afd.getParcelFileDescriptor();
            if (pfd == null) {
                closeRequest(params.mUri.getPath(), false, null, null, -1);
                return;
            }
        } finally {
            StrictMode.setThreadPolicy(oldPolicy);
        }

        // Prepare and send the data over.
        bundle.putString(DecoderService.KEY_FILE_PATH, params.mUri.getPath());
        bundle.putParcelable(DecoderService.KEY_FILE_DESCRIPTOR, pfd);
        bundle.putInt(DecoderService.KEY_SIZE, params.mSize);
        try {
            mIRemoteService.decodeImage(bundle, this);
            pfd.close();
        } catch (RemoteException e) {
            Log.e(TAG, "Communications failed (Remote): " + e);
            closeRequest(params.mUri.getPath(), false, null, null, -1);
        } catch (IOException e) {
            Log.e(TAG, "Communications failed (IO): " + e);
            closeRequest(params.mUri.getPath(), false, null, null, -1);
        }
    }

    /**
     * Cancels a request to decode an image (if it hasn't already been dispatched).
     * @param filePath The path to the image to cancel decoding.
     */
    public void cancelDecodeImage(String filePath) {
        mHighPriorityRequests.remove(filePath);
        mLowPriorityRequests.remove(filePath);
        mProcessingRequests.remove(filePath);
    }

    /** Sets a callback to use when the service is ready. For testing use only. */
    @VisibleForTesting
    public static void setReadyCallback(ServiceReadyCallback callback) {
        sReadyCallbackForTesting = callback;
    }
}
