// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.data_sharing.ui.versioning;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.components.messages.MessageDispatcher;
import org.chromium.ui.modaldialog.ModalDialogManager;

/**
 * Conditionally constructs and shows a message banner warning the user that they cannot use shared
 * tab groups functionality because of their Chrome. The positive action button will trigger a
 * dialog that will redirect the user to update Chrome.
 */
@NullMarked
public class VersioningMessageBanner {
    /**
     * Conditionally tries to synchronously display UI if the backend tells us to.
     *
     * @param context Used to load resources.
     * @param messageDispatcher Used to show a banner message about out of date version.
     * @param modalDialogManager Used to show a modal dialog about updating version.
     * @param profile Used to fetch scoped dependencies.
     */
    public static void maybeShow(
            Context context,
            MessageDispatcher messageDispatcher,
            ModalDialogManager modalDialogManager,
            Profile profile) {
        // TODO(https://crbug.com/422514006): Implement.
    }
}
