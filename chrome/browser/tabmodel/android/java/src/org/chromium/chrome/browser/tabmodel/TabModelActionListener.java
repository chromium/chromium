// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import androidx.annotation.IntDef;

import org.chromium.components.browser_ui.widget.ActionConfirmationResult;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Listener to get updates for actions that may show speedbump dialogs when performing operations on
 * a {@link TabModel}. See {@link TabRemover} and {@link TabUngrouper}.
 */
public interface TabModelActionListener {
    /** An enum representing the type of dialog that was shown. */
    @IntDef({DialogType.NONE, DialogType.SYNC, DialogType.COLLABORATION})
    @Retention(RetentionPolicy.SOURCE)
    public @interface DialogType {
        /** No dialog was shown. */
        int NONE = 0;

        /** A dialog about synced group destruction was shown. */
        int SYNC = 1;

        /** A dialog about collaboration destruction was shown. */
        int COLLABORATION = 2;
    }

    /**
     * Called with the result of showing the action confirmation dialog for the action. This is
     * guaranteed to be called, and may be called synchronously if no dialog is shown and the action
     * will proceed synchronously. This will be called after the action is triggered.
     *
     * @param dialogType The type of dialog that was shown.
     * @param result The {@link ActionConfirmationResult}.
     */
    default void onConfirmationDialogResult(
            @DialogType int dialogType, @ActionConfirmationResult int result) {}
}
