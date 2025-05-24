// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.drawable.BitmapDrawable;
import android.graphics.drawable.Drawable;
import android.util.AttributeSet;
import android.view.View;
import android.widget.FrameLayout;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.share.screenshot.ScreenshotShareSheetViewProperties.NoArgOperation;
import org.chromium.ui.widget.ChromeImageView;

/** Manages the Android View representing the Screenshot share panel. */
class ScreenshotShareSheetView extends FrameLayout {
    /** Constructor for use from XML. */
    public ScreenshotShareSheetView(Context context, AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Sets the listeners for all no arg operations.
     *
     * @param noArgOperationCallback Callback to perform.
     */
    public void setNoArgOperationsListeners(Callback<Integer> noArgOperationCallback) {
        setNoArgOperationListener(NoArgOperation.SHARE, R.id.share, noArgOperationCallback);
        setNoArgOperationListener(NoArgOperation.SAVE, R.id.save, noArgOperationCallback);
        setNoArgOperationListener(NoArgOperation.DELETE, R.id.delete, noArgOperationCallback);
        setNoArgOperationListener(NoArgOperation.DELETE, R.id.close_button, noArgOperationCallback);
    }

    /**
     * Sets the listener for an operation with zero arguments.
     *
     * @param operation The type of operation as defined by BottomBarProperties.NoArgOperation
     * @param viewId The id to listen for a tap on
     * @param noArgOperationCallback The callback to perform on tap
     */
    private void setNoArgOperationListener(
            Integer operation, int viewId, Callback<Integer> noArgOperationCallback) {
        View button = findViewById(viewId);
        button.setOnClickListener(
                v -> {
                    noArgOperationCallback.onResult(operation);
                });
    }

    /**
     * Updates Screenshot image on panel.
     *
     * @param bitmap The {@link Bitmap} to display.
     */
    public void updateScreenshotBitmap(Bitmap bitmap) {
        ChromeImageView screenshotImageView = findViewById(R.id.screenshot);
        Drawable drawable = new BitmapDrawable(bitmap);
        screenshotImageView.setImageDrawable(drawable);
    }
}
