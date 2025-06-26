// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Constructs and shows a dialog suggesting the user update their version of Chrome. Positive action
 * will take the user ot the Play Store.
 */
@NullMarked
public class VersioningModalDialog {
    /**
     * Shows a dialog prompting the user to update Chrome both negative and positive buttons.
     *
     * @param context Used to load resources and launch intents.
     * @param modalDialogManager Used to show as a dialog.
     */
    public static void show(Context context, ModalDialogManager modalDialogManager) {
        // TODO(https://crbug.com/422514006): Implement.
    }
}
