// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.autofill_ai;

import static org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiAttributionVerificationSheetProperties.DISMISS_HANDLER;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.autofill.autofill_ai.AutofillAiAttributionVerificationSheetCoordinator.Delegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.modelutil.PropertyModel;

/** Mediator handling presentation and lifecycle for the attribution verification bottom sheet. */
@NullMarked
/*package*/ class AutofillAiAttributionVerificationSheetMediator {
    private final BottomSheetController mBottomSheetController;
    private final BottomSheetContent mBottomSheetContent;
    private final PropertyModel mModel;
    private final Delegate mDelegate;
    private boolean mIsShowing;

    AutofillAiAttributionVerificationSheetMediator(
            BottomSheetController bottomSheetController,
            BottomSheetContent bottomSheetContent,
            Delegate delegate) {
        mBottomSheetController = bottomSheetController;
        mBottomSheetContent = bottomSheetContent;
        mDelegate = delegate;
        mModel =
                new PropertyModel.Builder(AutofillAiAttributionVerificationSheetProperties.ALL_KEYS)
                        .with(DISMISS_HANDLER, this::onDismissed)
                        .build();
    }

    PropertyModel getModel() {
        return mModel;
    }

    void requestShowContent() {
        if (mIsShowing) {
            return;
        }
        mIsShowing = true;
        if (!mBottomSheetController.requestShowContent(mBottomSheetContent, /* animate= */ true)) {
            mBottomSheetController.hideContent(mBottomSheetContent, /* animate= */ false);
            onDismissed();
            return;
        }
        if (!mIsShowing) {
            return;
        }
        mBottomSheetController.expandSheet();
    }

    private void onDismissed() {
        if (!mIsShowing) {
            return;
        }
        mIsShowing = false;
        mDelegate.onDismissed();
    }
}
