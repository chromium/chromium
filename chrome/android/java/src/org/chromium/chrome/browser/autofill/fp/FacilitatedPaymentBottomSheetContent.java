// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.fp;

import android.content.Context;
import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

/** The contents of facilitated payment bottom sheet. */
/*package*/ class FacilitatedPaymentBottomSheetContent implements BottomSheetContent {
    private final View mView;

    FacilitatedPaymentBottomSheetContent(Context context) {
        mView = new View(context);
    }

    @Override
    public View getContentView() {
        return mView;
    }

    @Nullable
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
    public int getSheetContentDescriptionStringId() {
        return R.string.ok;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.ok;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.ok;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.ok;
    }
}
