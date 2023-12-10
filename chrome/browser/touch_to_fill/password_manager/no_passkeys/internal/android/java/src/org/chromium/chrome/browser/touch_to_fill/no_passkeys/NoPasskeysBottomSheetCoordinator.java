// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.touch_to_fill.no_passkeys;

import android.content.Context;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.lang.ref.WeakReference;

/**
 * Coordinator of no passkeys bottom sheet.
 *
 * <p>This component shows a bottom sheet to inform the user that no passkeys are available.
 */
public class NoPasskeysBottomSheetCoordinator {
    interface NativeDelegate {
        /** Notifies the native side when the user selects "Use another device" option. */
        void onClickUseAnotherDevice();

        /** Notifies the native side when the bottom sheet is dismissed. */
        void onDismissed();
    }

    private final NoPasskeysBottomSheetMediator mMediator;
    private final WeakReference<Context> mContext;

    /**
     * Creates the coordinator.
     *
     * @param context The {@link Context} for this component.
     * @param bottomSheetController The {@link BottomSheetController} showing this bottom sheet.
     * @param nativeDelegate A {@link NoPasskeysBottomSheetCoordinator.NativeDelegate} to interact
     *     with the native side.
     */
    public NoPasskeysBottomSheetCoordinator(
            WeakReference<Context> context,
            WeakReference<BottomSheetController> bottomSheetController,
            NoPasskeysBottomSheetCoordinator.NativeDelegate nativeDelegate) {
        mContext = context;
        mMediator = new NoPasskeysBottomSheetMediator(bottomSheetController, nativeDelegate);
    }

    /**
     * Request to show the bottom sheet.
     *
     * <p>Invokes the dismiss callback even if the bottom sheet failed to show up.
     *
     * @param origin The formatted origin to render in the bottom sheet.
     */
    public void show(String origin) {
        if (mContext.get() == null
                || !mMediator.show(
                        new NoPasskeysBottomSheetContent(mContext.get(), origin, mMediator))) {
            destroy();
        }
    }

    /** Destroys this component hiding the bottom sheet if needed. */
    public void destroy() {
        mMediator.destroy();
    }
}
