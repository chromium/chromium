// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import org.chromium.base.Callback;
import org.chromium.components.autofill.PaymentsBubbleClosedReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * This class is responsible for rendering the Mandatory Reauth opt-in prompt in a bottomsheet. It
 * asks users if they want to be challenged with their device login credentials for Autofilling
 * credit cards which do not have any form of user authentication.
 */
class MandatoryReauthOptInBottomSheet implements BottomSheetContent {
    private final View mView;

    MandatoryReauthOptInBottomSheet(Context context, Callback<Integer> interactionHandler) {
        mView =
                LayoutInflater.from(context)
                        .inflate(R.layout.mandatory_reauth_opt_in_bottom_sheet, null);

        Button acceptButton = mView.findViewById(R.id.mandatory_reauth_opt_in_accept_button);
        acceptButton.setOnClickListener(
                unused -> interactionHandler.onResult(PaymentsBubbleClosedReason.ACCEPTED));
        Button cancelButton = mView.findViewById(R.id.mandatory_reauth_opt_in_cancel_button);
        cancelButton.setOnClickListener(
                unused -> interactionHandler.onResult(PaymentsBubbleClosedReason.CANCELLED));
    }

    /* BottomSheetContent implementation. */
    @Override
    public View getContentView() {
        return mView;
    }

    @Override
    public View getToolbarView() {
        return null;
    }

    @Override
    public int getVerticalScrollOffset() {
        return 0;
    }

    @Override
    public void destroy() {}

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public boolean hasCustomLifecycle() {
        // Declare a custom lifecycle to prevent the bottom sheet from being dismissed by page
        // navigation.
        return true;
    }

    @Override
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.autofill_mandatory_reauth_opt_in_content_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_mandatory_reauth_opt_in_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_mandatory_reauth_opt_in_closed;
    }
}
