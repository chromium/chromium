// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feature_engagement;

import android.Manifest;
import android.content.Context;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.os.Handler;
import android.provider.MediaStore;
import android.provider.MediaStore.Images.Media;
import android.support.v4.content.ContextCompat;
import android.util.DisplayMetrics;
import android.view.WindowManager;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * This class detects screenshots by monitoring the screenshots directory on internal and external
 * storages, and notifies the ScreenshotMonitorDelegate. The caller should use
 * @{link ScreenshotMonitor#create(ScreenshotMonitorDelegate)} to create an instance.
 */
public class ScreenshotMonitor {
    private static final String TAG = "ScreenshotMonitor";
    private final ScreenshotMonitorDelegate mDelegate;
    private final ScreenshotMonitorContentObserver mContentObserver;

    /**
     * This tracks whether monitoring is on (i.e. started but not stopped). It must only be accessed
     * on the UI thread.
     */
    private boolean mIsMonitoring;

    private boolean mSkipOsCallsForUnitTesting;

    /**
     * Observe content changes in the Media database looking for screenshots.
     */
    class ScreenshotMonitorContentObserver extends ContentObserver {
        ScreenshotMonitorContentObserver(Handler handler, ScreenshotMonitor screenshotMonitor) {
            super(handler);
            mHandler = handler;
            mScreenshotMonitor = screenshotMonitor;
        }

        private final Handler mHandler;
        private final ScreenshotMonitor mScreenshotMonitor;

        /**
         * This override is used for older version of the OS that did not have the two argument
         * version.
         * @param selfChange True if this is a self change notification.
         */
        @Override
        public void onChange(boolean selfChange) {
            onChange(selfChange, null);
        }

        /**
         * Detect changes to the media store, and filter out ones that look like screenshots.
         * @param selfChange True if this is a self change notification. Unused.
         * @param uri The URI of the changed item in the media database.
         */
        @Override
        public void onChange(boolean selfChange, Uri uri) {
            Log.d(TAG, "Detected change to the media database " + uri);
            String uriPath = uri.toString();
            // Sanity check the uri before processing it.
            if (uri == null || !uri.toString().startsWith(Media.EXTERNAL_CONTENT_URI.toString())) {
                Log.w(TAG, "uri: %s is not valid. Returning without processing screenshot", uri);
                return;
            }

            if (!doesChangeLookLikeScreenshot(uri)) return;

            PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
                @Override
                public void run() {
                    if (mScreenshotMonitor == null) return;
                    mScreenshotMonitor.onEventOnUiThread(uriPath);
                }
            });
        }
    }

    // Returns true if the uri appears to correspond to a screenshot.  This will look at the
    // location of the file in storage by looking for the word "Screenshot", and the width and
    // height of the image.  We do this to differentiate between screenshots and downloaded images.
    private boolean doesChangeLookLikeScreenshot(Uri storeUri) {
        // Unit tests do not have a media database to query, so return true here.
        if (mSkipOsCallsForUnitTesting) return true;

        Cursor cursor = null;
        String foundPath = "";
        String imageWidthString = "";
        String imageHeightString = "";

        String[] mediaProjection = new String[] {MediaStore.Images.ImageColumns.DATE_TAKEN,
                MediaStore.MediaColumns.DATA, MediaStore.MediaColumns.HEIGHT,
                MediaStore.MediaColumns.WIDTH, MediaStore.MediaColumns._ID};

        // Check if READ_EXTERNAL_STORAGE permission are enabled.
        if (ContextCompat.checkSelfPermission(
                    ContextUtils.getApplicationContext(), Manifest.permission.READ_EXTERNAL_STORAGE)
                != PackageManager.PERMISSION_GRANTED) {
            RecordUserAction.record("Tab.Screenshot.WithoutStoragePermission");
            return false;
        }

        try {
            cursor = ContextUtils.getApplicationContext().getContentResolver().query(
                    storeUri, mediaProjection, null, null, null);
        } catch (SecurityException se) {
            // This happens on some exotic devices.
            Log.e(TAG, "Cannot query media store.", se);
        }

        if (cursor == null) {
            return false;
        }

        try {
            while (cursor.moveToNext()) {
                foundPath = cursor.getString(
                        cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA));
                imageHeightString = cursor.getString(
                        cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.HEIGHT));
                imageWidthString = cursor.getString(
                        cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.WIDTH));
                break;
            }
        } finally {
            cursor.close();
        }

        // Verify that it is in a screenshot directory.  We don't check the file extension because
        // we already know that we have an image from the image database. This directory name does
        // not get localized.
        int index = foundPath.indexOf("Screenshot");
        if (index == -1) {
            return false;
        }

        // Check width and height.
        DisplayMetrics displayMetrics = new DisplayMetrics();
        WindowManager windowManager =
                (WindowManager) ContextUtils.getApplicationContext().getSystemService(
                        Context.WINDOW_SERVICE);
        windowManager.getDefaultDisplay().getMetrics(displayMetrics);
        int screenHeight = displayMetrics.heightPixels;
        int screenWidth = displayMetrics.widthPixels;
        int imageHeight = Integer.parseInt(imageHeightString);
        int imageWidth = Integer.parseInt(imageWidthString);

        // Note that the height of the system bar and the status bar are not counted in the values
        // returned by displayMetrics.  If the device is in portrait, the height reported by this
        // API will be smaller than the actual screen size.  In landscape mode, the reported width
        // will be smaller. This means that either the height or width will match (the other will be
        // a bit short).  So, we return that the image looks like a screenshot if either matches.
        if (screenHeight == imageHeight || screenWidth == imageWidth) return true;
        // Just in case the device gets rotated after the snapshot and before the event, check
        // width against height instead of width.
        if (screenHeight == imageWidth || screenWidth == imageHeight) return true;

        // Otherwise assume this is not a screenshot.
        return false;
    }

    @VisibleForTesting
    void setSkipOsCallsForUnitTesting() {
        mSkipOsCallsForUnitTesting = true;
    }

    public ScreenshotMonitor(ScreenshotMonitorDelegate delegate) {
        mDelegate = delegate;
        // null means use the default thread instead of a new thread for the observer.
        mContentObserver = new ScreenshotMonitorContentObserver(null, this);
    }

    /**
     * Start monitoring the screenshot directory.
     */
    public void startMonitoring() {
        ThreadUtils.assertOnUiThread();

        // Register the content observer for the Media database to watch the media database.
        ContextUtils.getApplicationContext().getContentResolver().registerContentObserver(
                Media.EXTERNAL_CONTENT_URI, true, mContentObserver);

        mIsMonitoring = true;
    }

    /**
     * Stop monitoring the screenshot directory.
     */
    public void stopMonitoring() {
        ThreadUtils.assertOnUiThread();
        mIsMonitoring = false;

        // Unregister the content observer for the Media database.
        if (mContentObserver == null) {
            Log.d(TAG, "Cannot stop detecting that hasn't been started: ContentObserver is null.");
        } else {
            ContextUtils.getApplicationContext().getContentResolver().unregisterContentObserver(
                    mContentObserver);
        }
    }

    @VisibleForTesting
    ScreenshotMonitorContentObserver getContentObserver() {
        return mContentObserver;
    }

    private void onEventOnUiThread(final String uri) {
        if (!mIsMonitoring || uri == null) return;
        mDelegate.onScreenshotTaken();
    }
}
