// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.download;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.drawable.Drawable;
import android.view.ContextThemeWrapper;

import androidx.annotation.DrawableRes;
import androidx.annotation.Px;

import org.jni_zero.CalledByNative;
import org.jni_zero.JniType;

import org.chromium.base.ContextUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.components.browser_ui.util.DownloadUtils;
import org.chromium.components.offline_items_collection.OfflineItem;
import org.chromium.components.offline_items_collection.OfflineItemFilter;
import org.chromium.ui.UiUtils;

/** Icons for download items, shared by the Downloads page and the download toolbar tray. */
@NullMarked
public final class DownloadIconUtils {
    private DownloadIconUtils() {}

    /** Returns the drawable resource for the icon representing {@code item}. */
    public static @DrawableRes int getIconForItem(OfflineItem item) {
        if (DownloadUtils.shouldDisplayDownloadAsDangerous(item.dangerType, item.state)
                || DownloadUtils.isBlockedSensitiveDownload(item)) {
            return R.drawable.dangerous_filled_24dp;
        }
        return getIconForFilter(item.filter);
    }

    /** Returns the drawable resource for the icon representing content of type {@code filter}. */
    private static @DrawableRes int getIconForFilter(@OfflineItemFilter int filter) {
        switch (filter) {
            case OfflineItemFilter.PAGE:
                return R.drawable.ic_globe_24dp;
            case OfflineItemFilter.VIDEO:
                return R.drawable.ic_videocam_fill_24dp;
            case OfflineItemFilter.AUDIO:
                return R.drawable.ic_music_note_24dp;
            case OfflineItemFilter.IMAGE:
                return R.drawable.ic_drive_image_24dp;
            default:
                // Documents share the generic file icon.
                return R.drawable.ic_drive_file_24dp;
        }
    }

    /**
     * Renders the icon for files of type {@code filter} into a square bitmap. Used by native code
     * that has no view to show the drawable in, such as the chrome.downloads.getFileIcon()
     * extension API.
     *
     * @param filter The type of the file.
     * @param sizePx The width and height of the bitmap in pixels.
     */
    @CalledByNative
    static @JniType("SkBitmap") Bitmap getFileTypeIcon(
            @OfflineItemFilter @JniType("offline_items_collection::OfflineItemFilter") int filter,
            @Px int sizePx) {
        // Some of the drawables refer to theme attributes, which the application context's theme
        // does not define.
        Context context =
                new ContextThemeWrapper(
                        ContextUtils.getApplicationContext(), R.style.Theme_BrowserUI_DayNight);
        // The bitmap is shown on a background this code does not know, e.g. an extension's popup
        // in light or dark mode, so use a neutral color that has contrast on both.
        Drawable drawable =
                UiUtils.getTintedDrawable(
                        context, getIconForFilter(filter), R.color.baseline_neutral_50);
        Bitmap bitmap = Bitmap.createBitmap(sizePx, sizePx, Bitmap.Config.ARGB_8888);
        drawable.setBounds(0, 0, sizePx, sizePx);
        drawable.draw(new Canvas(bitmap));
        return bitmap;
    }
}
