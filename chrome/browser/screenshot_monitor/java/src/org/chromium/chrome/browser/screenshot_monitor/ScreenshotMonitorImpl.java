// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.screenshot_monitor;

import android.app.Activity;
import android.content.ContentResolver;
import android.content.pm.PackageManager;
import android.database.ContentObserver;
import android.database.Cursor;
import android.net.Uri;
import android.provider.MediaStore;
import android.provider.MediaStore.Images.Media;
import android.text.TextUtils;

import androidx.annotation.VisibleForTesting;
import androidx.core.content.ContextCompat;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.ui.base.MimeTypeUtils;
import org.chromium.ui.display.DisplayAndroid;

/**
 * This class detects screenshots by monitoring the screenshots directory on internal and external
 * storages, and notifies the ScreenshotMonitorDelegate. The caller should use
 * @{link ScreenshotMonitor#create(ScreenshotMonitorDelegate)} to create an instance.
 */
public class ScreenshotMonitorImpl extends ScreenshotMonitor {
    private static final String TAG = "ScreenshotMonitor";
    private final ScreenshotMonitorContentObserver mContentObserver;

    private ContentResolver mContentResolverForTesting;
    private DisplayAndroid mDisplayAndroidForTesting;

    /** Observe content changes in the Media database looking for screenshots. */
    private class ScreenshotMonitorContentObserver extends ContentObserver {
        private final ScreenshotMonitor mScreenshotMonitor;

        ScreenshotMonitorContentObserver(ScreenshotMonitor screenshotMonitor) {
            // null means use the default thread instead of a new thread for the observer.
            super(/* Handler */ null);
            mScreenshotMonitor = screenshotMonitor;
        }

        // ContentObsever implementation.
        @Override
        public void onChange(boolean selfChange, Uri uri) {
            checkAndNotify(uri);
        }

        private void checkAndNotify(Uri uri) {
            if (uri == null) return;

            Log.d(TAG, "Detected change to the media database " + uri);
            // Validate the uri before processing it.
            if (uri == null || !uri.toString().startsWith(Media.EXTERNAL_CONTENT_URI.toString())) {
                Log.w(TAG, "uri: %s is not valid. Returning without processing screenshot", uri);
                return;
            }

            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        // Unit tests do not have a media database to query, so skip if necessary.
                        if (!doesChangeLookLikeScreenshot(uri)) return;
                        mScreenshotMonitor.notifyDelegate();
                    });
        }

        // Returns true if the uri appears to correspond to a screenshot.  This will look at the
        // location of the file in storage by looking for the word "Screenshot", and the width and
        // height of the image.  We do this to differentiate between screenshots and downloaded
        // images.
        private boolean doesChangeLookLikeScreenshot(Uri storeUri) {
            ThreadUtils.assertOnUiThread();

            Cursor cursor = null;
            String foundPath = "";
            String imageWidthString = "";
            String imageHeightString = "";

            String[] mediaProjection =
                    new String[] {
                        MediaStore.Images.ImageColumns.DATE_TAKEN,
                        MediaStore.MediaColumns.DATA,
                        MediaStore.MediaColumns.HEIGHT,
                        MediaStore.MediaColumns.WIDTH,
                        MediaStore.MediaColumns._ID
                    };

            // Check if the appropriate disk access permission is enabled.
            String requiredPermission =
                    MimeTypeUtils.getPermissionNameForMimeType(MimeTypeUtils.Type.IMAGE);
            if (requiredPermission != null
                    && ContextCompat.checkSelfPermission(
                                    ContextUtils.getApplicationContext(), requiredPermission)
                            != PackageManager.PERMISSION_GRANTED) {
                RecordUserAction.record("Tab.Screenshot.WithoutStoragePermission");
                return false;
            }

            try {
                ContentResolver contentResolver = mContentResolverForTesting;
                if (contentResolver == null) {
                    contentResolver = ContextUtils.getApplicationContext().getContentResolver();
                }

                cursor = contentResolver.query(storeUri, mediaProjection, null, null, null);
            } catch (SecurityException se) {
                // This happens on some exotic devices.
                Log.e(TAG, "Cannot query media store.", se);
            }

            if (cursor == null) {
                return false;
            }

            try {
                while (cursor.moveToNext()) {
                    foundPath =
                            cursor.getString(
                                    cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.DATA));
                    imageHeightString =
                            cursor.getString(
                                    cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.HEIGHT));
                    imageWidthString =
                            cursor.getString(
                                    cursor.getColumnIndexOrThrow(MediaStore.MediaColumns.WIDTH));
                    break;
                }
            } finally {
                cursor.close();
            }

            if (TextUtils.isEmpty(imageHeightString) || TextUtils.isEmpty(imageWidthString)) {
                return false;
            }

            // Verify that it is in a screenshot directory.  We don't check the file extension
            // because we already know that we have an image from the image database. This directory
            // name does not get localized.
            int index = foundPath.indexOf("Screenshot");
            if (index == -1) {
                return false;
            }

            // Check width and height.
            DisplayAndroid display = mDisplayAndroidForTesting;
            if (display == null) {
                display = DisplayAndroid.getNonMultiDisplay(ContextUtils.getApplicationContext());
            }

            int screenHeight = display.getDisplayHeight();
            int screenWidth = display.getDisplayWidth();
            int imageHeight = Integer.parseInt(imageHeightString);
            int imageWidth = Integer.parseInt(imageWidthString);

            // Note that the height of the system bar and the status bar are not counted in the
            // values returned by displayMetrics.  If the device is in portrait, the height reported
            // by this API will be smaller than the actual screen size.  In landscape mode, the
            // reported width will be smaller. This means that either the height or width will match
            // (the other will be a bit short).  So, we return that the image looks like a
            // screenshot if either matches.
            if (screenHeight == imageHeight || screenWidth == imageWidth) return true;

            // Just in case the device gets rotated after the snapshot and before the event, check
            // width against height instead of width.
            if (screenHeight == imageWidth || screenWidth == imageHeight) return true;

            // Otherwise assume this is not a screenshot.
            return false;
        }
    }

    public ScreenshotMonitorImpl(ScreenshotMonitorDelegate delegate, Activity activity) {
        super(delegate);
        mContentObserver = new ScreenshotMonitorContentObserver(this);
    }

    @VisibleForTesting
    ScreenshotMonitorImpl(
            ScreenshotMonitorDelegate delegate,
            Activity activity,
            ContentResolver contentResolver,
            DisplayAndroid displayAndroid) {
        this(delegate, activity);
        mContentResolverForTesting = contentResolver;
        mDisplayAndroidForTesting = displayAndroid;
    }

    // ScreenshotMonitor implementation.
    @Override
    protected void setUpMonitoring(boolean monitor) {
        ContentResolver contentResolver = mContentResolverForTesting;
        if (contentResolver == null) {
            contentResolver = ContextUtils.getApplicationContext().getContentResolver();
        }
        if (monitor) {
            // Register the content observer for the Media database to watch the media database.
            contentResolver.registerContentObserver(
                    Media.EXTERNAL_CONTENT_URI, true, mContentObserver);
        } else {
            contentResolver.unregisterContentObserver(mContentObserver);
        }
    }

    @VisibleForTesting
    ContentObserver getContentObserver() {
        return mContentObserver;
    }
}
