// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.mandatory_reauth;

import android.content.Context;
import android.content.res.Resources;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.Button;

import androidx.annotation.StringRes;

import org.chromium.base.Callback;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.autofill.PaymentsUiClosedReason;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/**
 * This class is responsible for rendering the Mandatory Reauth opt-in prompt in a bottomsheet. It
 * asks users if they want to be challenged with their device login credentials for Autofilling
 * credit cards which do not have any form of user authentication.
 */
@NullMarked
class MandatoryReauthOptInBottomSheet implements BottomSheetContent {
    private final View mView;

    MandatoryReauthOptInBottomSheet(Context context, Callback<Integer> interactionHandler) {
        mView =
                LayoutInflater.from(context)
                        .inflate(R.layout.mandatory_reauth_opt_in_bottom_sheet, null);

        Button acceptButton = mView.findViewById(R.id.mandatory_reauth_opt_in_accept_button);
        acceptButton.setOnClickListener(
                unused -> interactionHandler.onResult(PaymentsUiClosedReason.ACCEPTED));
        Button cancelButton = mView.findViewById(R.id.mandatory_reauth_opt_in_cancel_button);
        cancelButton.setOnClickListener(
                unused -> interactionHandler.onResult(PaymentsUiClosedReason.CANCELLED));
    }

    /* BottomSheetContent implementation. */
    @Override
    public View getContentView() {
        return mView;
    }

    @Override
    public @Nullable View getToolbarView() {
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
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public String getSheetContentDescription(Context context) {
        return context.getString(R.string.autofill_mandatory_reauth_opt_in_content_description);
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return Resources.ID_NULL;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        return R.string.autofill_mandatory_reauth_opt_in_opened_full;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        return R.string.autofill_mandatory_reauth_opt_in_closed;
    }
}
