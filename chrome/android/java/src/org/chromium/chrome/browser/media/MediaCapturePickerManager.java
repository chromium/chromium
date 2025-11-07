// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * Manager for the media capture picker. This class is the entry point for showing the picker UI. It
 * will decide whether to show the old dialog or a new UI based on a feature flag.
 */
@NullMarked
public class MediaCapturePickerManager {
    /** A delegate for handling returning the picker result. */
    public interface Delegate {
        /**
         * Called when the user has selected a tab to share.
         *
         * @param webContents The contents to share.
         * @param audioShare True if tab audio should be shared.
         */
        void onPickTab(WebContents webContents, boolean audioShare);

        /** Called when the user has selected a window to share. */
        void onPickWindow();

        /** Called when the user has selected a screen to share. */
        void onPickScreen();

        /** Called when the user has elected to not share anything. */
        void onCancel();
    }

    private static @Nullable Context maybeGetContext(WebContents webContents) {
        final WindowAndroid window = webContents.getTopLevelNativeWindow();
        if (window == null) return null;
        return window.getContext().get();
    }

    /**
     * Shows the media capture picker dialog.
     *
     * @param webContents The {@link WebContents} to show the dialog on behalf of.
     * @param appName Name of the app that wants to share content.
     * @param requestAudio True if audio sharing is also requested.
     * @param delegate Invoked with a WebContents if a tab is selected, or {@code null} if the
     *     dialog is dismissed.
     */
    public static void showDialog(
            WebContents webContents, String appName, boolean requestAudio, Delegate delegate) {
        final Context context = maybeGetContext(webContents);
        if (context == null) {
            delegate.onCancel();
            return;
        }

        if (ChromeFeatureList.sAndroidNewMediaPicker.isEnabled()) {
            MediaCapturePickerInvoker.show(context, webContents, appName, requestAudio, delegate);
        } else {
            new MediaCapturePickerDialog(context, webContents, appName, requestAudio, delegate)
                    .show();
        }
    }
}
