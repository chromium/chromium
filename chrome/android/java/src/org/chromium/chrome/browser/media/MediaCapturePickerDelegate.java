// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.WebContents;

/** Interface to delegate media capture picker creation to clank. */
@NullMarked
public interface MediaCapturePickerDelegate {
    /**
     * Creates an intent to launch the screen capture picker with app content sharing.
     *
     * @param context The context.
     * @param params The picker parameters.
     * @return The intent to launch, or null if not supported.
     */
    @Deprecated
    default @Nullable Intent createScreenCaptureIntent(
            Context context, MediaCapturePickerManager.Params params) {
        return null;
    }

    /**
     * Creates an intent to launch the screen capture picker with app content sharing.
     *
     * @param context The context.
     * @param params The picker parameters.
     * @param delegate The delegate to filter tabs to show on the picker.
     * @return The intent to launch, or null if not supported.
     */
    default @Nullable Intent createScreenCaptureIntent(
            Context context,
            MediaCapturePickerManager.Params params,
            MediaCapturePickerManager.Delegate delegate) {
        return createScreenCaptureIntent(context, params);
    }

    /**
     * Returns the tab that was picked by the user, if any. This should be called after the picker
     * has finished with a successful result.
     *
     * @return The picked tab, or null if a window/screen was picked or no selection.
     */
    @Nullable Tab getPickedTab();

    /**
     * Returns whether the user requested audio sharing in the picker. This is only valid if {@link
     * #getPickedTab()} returns non-null.
     *
     * @return True if audio sharing was requested.
     */
    boolean shouldShareAudio();

    /**
     * Called when the screen capture picker is done i.e. after user has picked a tab/ window/
     * screen, or cancel the dialog.
     */
    @Deprecated
    default void onFinish() {}

    /**
     * Called when the screen capture picker is done i.e. after user has picked a tab/ window/
     * screen, or cancel the dialog.
     */
    default void onFinish(WebContents webContents) {
        onFinish();
    }
}
