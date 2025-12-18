// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import android.content.Context;
import android.content.Intent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
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
    @Nullable Intent createScreenCaptureIntent(
            Context context, MediaCapturePickerManager.Params params);

    /**
     * Returns the WebContents that was picked by the user, if any. This should be called after the
     * picker has finished with a successful result.
     *
     * @return The picked WebContents, or null if a window/screen was picked or no selection.
     */
    @Nullable WebContents getPickedWebContents();

    /**
     * Returns whether the user requested audio sharing in the picker. This is only valid if {@link
     * #getPickedWebContents()} returns non-null.
     *
     * @return True if audio sharing was requested.
     */
    boolean shouldShareAudio();
}
