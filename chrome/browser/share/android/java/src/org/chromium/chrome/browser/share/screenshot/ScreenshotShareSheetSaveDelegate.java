// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.screenshot;

import android.content.Context;
import android.graphics.Bitmap;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.share.SaveBitmapDelegate;
import org.chromium.ui.base.WindowAndroid;
import org.chromium.ui.modelutil.PropertyModel;

/** ScreenshotShareSheetSaveDelegate is in charge of download the current bitmap. */
@NullMarked
class ScreenshotShareSheetSaveDelegate {
    private final PropertyModel mModel;
    private final Context mContext;
    private final WindowAndroid mWindowAndroid;
    private final Runnable mCloseDialogRunnable;
    private final Profile mProfile;

    /**
     * The ScreenshotShareSheetSaveDelegate constructor.
     *
     * @param context The context to use.
     * @param propertyModel The property model to use to communicate with views.
     * @param closeDialogRunnable The runnable to close the dialog.
     * @param windowAndroid The {@link WindowAndroid} to request permissions.
     * @param profile The {@link Profile} associated with the download.
     */
    ScreenshotShareSheetSaveDelegate(
            Context context,
            PropertyModel propertyModel,
            Runnable closeDialogRunnable,
            WindowAndroid windowAndroid,
            Profile profile) {
        mContext = context;
        mModel = propertyModel;
        mWindowAndroid = windowAndroid;
        mCloseDialogRunnable = closeDialogRunnable;
        mProfile = profile;
    }

    /** Saves the current image. */
    protected void save() {
        Bitmap bitmap = mModel.get(ScreenshotShareSheetViewProperties.SCREENSHOT_BITMAP);
        if (bitmap == null) {
            return;
        }

        SaveBitmapDelegate saveBitmapDelegate =
                new SaveBitmapDelegate(
                        mContext,
                        bitmap,
                        R.string.screenshot_filename_prefix,
                        mCloseDialogRunnable,
                        mWindowAndroid,
                        mProfile);

        saveBitmapDelegate.save();
    }
}
