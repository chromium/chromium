// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.wallet_reminder_notice;

import android.view.View;

import androidx.annotation.StringRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.autofill.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** BottomSheetContent implementation for the Wallet Reminder Notice bottom sheet. */
@NullMarked
/*package*/ class AutofillWalletReminderNoticeBottomSheetContent implements BottomSheetContent {
    private final View mContentView;

    AutofillWalletReminderNoticeBottomSheetContent(View contentView) {
        mContentView = contentView;
    }

    @Override
    public View getContentView() {
        return mContentView;
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
    public boolean hasCustomLifecycle() {
        // Keeps the bottom sheet open during checkout page navigation.
        return true;
    }

    @Override
    public int getPriority() {
        return ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return true;
    }

    @Override
    public float getFullHeightRatio() {
        return HeightMode.WRAP_CONTENT;
    }

    @Override
    public float getHalfHeightRatio() {
        return HeightMode.DISABLED;
    }

    @Override
    public @StringRes int getSheetHalfHeightAccessibilityStringId() {
        // TODO(crbug.com/541299511): Add dedicated Wallet Reminder Notice accessibility strings.
        return R.string.ok;
    }

    @Override
    public @StringRes int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/541299511): Add dedicated Wallet Reminder Notice accessibility strings.
        return R.string.ok;
    }

    @Override
    public @StringRes int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/541299511): Add dedicated Wallet Reminder Notice accessibility strings.
        return R.string.ok;
    }
}
