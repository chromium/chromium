// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import android.content.Context;

import org.chromium.components.autofill.PaymentsBubbleClosedReason;
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
    private final MandatoryReauthOptInBottomSheetComponent.Delegate mDelegate;
    private final MandatoryReauthOptInBottomSheet mView;
    private final BottomSheetObserver mObserver =
            new EmptyBottomSheetObserver() {
                @Override
                public void onSheetClosed(@StateChangeReason int reason) {
                    switch (reason) {
                        case StateChangeReason.BACK_PRESS: // Intentional fallthrough.
                        case StateChangeReason.SWIPE: // Intentional fallthrough.
                        case StateChangeReason.TAP_SCRIM:
                            mDelegate.onClosed(PaymentsBubbleClosedReason.CLOSED);
                            break;
                        case StateChangeReason.NAVIGATION: // Intentional fallthrough.
                        case StateChangeReason.COMPOSITED_UI: // Intentional fallthrough.
                        case StateChangeReason.VR: // Intentional fallthrough.
                        case StateChangeReason.PROMOTE_TAB: // Intentional fallthrough.
                        case StateChangeReason.OMNIBOX_FOCUS: // Intentional fallthrough.
                        case StateChangeReason.NONE:
                            mDelegate.onClosed(PaymentsBubbleClosedReason.NOT_INTERACTED);
                            break;
                        case StateChangeReason.INTERACTION_COMPLETE:
                            break;
                    }
                    mController.removeObserver(this);
                }
            };

    MandatoryReauthOptInBottomSheetCoordinator(
            Context context,
            BottomSheetController controller,
            MandatoryReauthOptInBottomSheetComponent.Delegate delegate) {
        mController = controller;
        mDelegate = delegate;
        mView = new MandatoryReauthOptInBottomSheet(context, this::close);
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
    public void close(@PaymentsBubbleClosedReason int closedReason) {
        switch (closedReason) {
            case PaymentsBubbleClosedReason.ACCEPTED: // Intentional fallthrough.
            case PaymentsBubbleClosedReason.CANCELLED:
                mController.hideContent(mView, true, StateChangeReason.INTERACTION_COMPLETE);
                mDelegate.onClosed(closedReason);
                break;
            case PaymentsBubbleClosedReason.NOT_INTERACTED:
                mController.hideContent(mView, true, StateChangeReason.NONE);
                break;
            default:
                assert false;
        }
    }
}
