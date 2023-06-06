// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import android.content.Context;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetObserver;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;

/**
 * Creates the model, and the view, and connects them. It also executes the commands from the native
 * controller.
 */
class MandatoryReauthOptInBottomSheetCoordinator
        implements MandatoryReauthOptInBottomSheetComponent {
    private final BottomSheetController mController;
    private final MandatoryReauthOptInBottomSheet mView;
    private final BottomSheetObserver mObserver = new EmptyBottomSheetObserver() {
        @Override
        public void onSheetClosed(@StateChangeReason int reason) {}
    };

    MandatoryReauthOptInBottomSheetCoordinator(Context context, BottomSheetController controller) {
        mController = controller;
        mView = new MandatoryReauthOptInBottomSheet(context);
    }

    @Override
    public boolean show() {
        mController.addObserver(mObserver);
        if (mController.requestShowContent(mView, /* animate= */ true)) {
            return true;
        }
        mController.removeObserver(mObserver);
        return false;
    }

    @Override
    public void close() {
        mController.hideContent(mView, true);
        mController.removeObserver(mObserver);
    }
}
