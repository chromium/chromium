// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import android.content.Context;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** Implements the content for the no passkeys bottom sheet. */
class NoPasskeysBottomSheetContent {
    private final Delegate mDelegate;
    private final Context mContext;
    private final String mOrigin;

    /** User actions delegated by this bottom sheet. */
    interface Delegate {
        /** Called when the user acknowledged that there are no passkeys. */
        void onClickOk();

        /** Called when the user decides to starts the flow to check other devices for passkeys. */
        void onClickUseAnotherDevice();

        /**
         * Called when the sheet is hidden (but not due to temporary suppression).
         * @see BottomSheetContent#destroy()
         */
        void onDestroy();
    }

    /**
     * Creates the BottomSheetContent and inflates the view given a delegate responding to actions.
     *
     * @param context The activity context of the window.
     * @param origin A formatted {@link String} of the origin to display.
     * @param delegate A {@link Delegate} handling interaction with the sheet.
     */
    NoPasskeysBottomSheetContent(Context context, String origin, Delegate delegate) {
        mDelegate = delegate;
        mContext = context;
        mOrigin = origin;
    }
}
