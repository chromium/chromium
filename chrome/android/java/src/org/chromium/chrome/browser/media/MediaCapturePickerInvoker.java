// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.content_public.browser.WebContents;

/** Stub for the new media capture picker UI. */
@NullMarked
public class MediaCapturePickerInvoker {
    /**
     * Shows the new media capture picker UI.
     *
     * @param context The {@link Context} to use.
     * @param webContents The {@link WebContents} to show the dialog on behalf of.
     * @param appName Name of the app that wants to share content.
     * @param requestAudio True if audio sharing is also requested.
     * @param delegate The delegate to notify of the user's choice.
     */
    public static void show(
            Context context,
            WebContents webContents,
            String appName,
            boolean requestAudio,
            MediaCapturePickerManager.Delegate delegate) {
        // TODO(crbug.com/352187279): Stub implementation. For now, just cancel.
        delegate.onCancel();
    }
}
