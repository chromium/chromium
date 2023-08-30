// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.vcn;

import android.content.Context;
import android.view.View;

import org.chromium.ui.base.WindowAndroid;

/** Coordinator controller for the virtual card enrollment bottom sheet. */
/*package*/ class AutofillVcnEnrollBottomSheetCoordinator {
    private final AutofillVcnEnrollBottomSheetMediator mMediator;

    /**
     * Constructs a coordinator controller for the virtual card enrollment bottom sheet.
     *
     * @param context The activity context.
     * @param onDismiss The callback to invoke when the user dismisses the bottom sheet.
     */
    /*package*/ AutofillVcnEnrollBottomSheetCoordinator(Context context, Runnable onDismiss) {
        mMediator = new AutofillVcnEnrollBottomSheetMediator(new View(context), onDismiss);
    }

    /**
     * Requests to show the bottom sheet.
     *
     * @param window The window where the bottom sheet should be shown.
     *
     * @return True if shown.
     */
    /*package*/ boolean requestShowContent(WindowAndroid window) {
        return mMediator.requestShowContent(window);
    }

    /** Hides the virtual card enrollment bottom sheet, if present. */
    /*package*/ void hide() {
        mMediator.hide();
    }
}
