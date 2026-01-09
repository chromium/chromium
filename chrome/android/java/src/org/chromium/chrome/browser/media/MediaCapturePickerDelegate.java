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
    @Nullable Intent createScreenCaptureIntent(
            Context context, MediaCapturePickerManager.Params params);

    /**
     * Returns the WebContents that was picked by the user, if any. This should be called after the
     * picker has finished with a successful result. TODO(crbug.com/461576210): to be deprecated
     * after migrated to {@link #getPickedTab()}.
     *
     * @return The picked WebContents, or null if a window/screen was picked or no selection.
     */
    default @Nullable WebContents getPickedWebContents() {
        return null;
    }

    /**
     * Returns the tab that was picked by the user, if any. This should be called after the picker
     * has finished with a successful result. TODO(crbug.com/461576210): default for compatibility;
     * remove attribute when implementation changed.
     *
     * @return The picked tab, or null if a window/screen was picked or no selection.
     */
    default @Nullable Tab getPickedTab() {
        return null;
    }

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
    default void onFinish() {}
}
