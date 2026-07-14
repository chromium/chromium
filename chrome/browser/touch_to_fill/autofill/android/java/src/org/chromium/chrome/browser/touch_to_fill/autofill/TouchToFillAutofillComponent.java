// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.autofill;

import android.content.Context;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.touch_to_fill.common.BottomSheetFocusHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * This component allows prompting the user with the Ambient Autofill notice bottom sheet (Personal
 * Context Notice) and managing its lifecycle.
 */
@NullMarked
public interface TouchToFillAutofillComponent {
    /** This delegate is called when the TouchToFillAutofill component is interacted with. */
    interface Delegate {
        /** Called when the user clicks OK to acknowledge the notice. */
        void onNoticeAcknowledged();

        /** Called when the user clicks the Manage settings link. */
        void onSettingsLinkClicked();

        /** Called whenever the sheet is dismissed (by user or native). */
        void onDismissed();
    }

    /**
     * Initializes the component.
     *
     * @param context A {@link Context} to create views and retrieve resources.
     * @param sheetController A {@link BottomSheetController} used to show/hide the sheet.
     * @param delegate A {@link Delegate} that handles interaction events.
     * @param bottomSheetFocusHelper A {@link BottomSheetFocusHelper} to restore focus upon close.
     */
    void initialize(
            Context context,
            BottomSheetController sheetController,
            Delegate delegate,
            BottomSheetFocusHelper bottomSheetFocusHelper);

    /** Displays the Personal Context Notice bottom sheet. */
    void show();

    /** Hides the bottom sheet if shown. */
    void hide();

    /** Destroys the component and cleans up observers and change processors. */
    void destroy();
}
