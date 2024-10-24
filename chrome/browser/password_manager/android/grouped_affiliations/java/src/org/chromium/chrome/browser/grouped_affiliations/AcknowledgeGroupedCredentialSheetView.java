// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.grouped_affiliations;

import android.view.View;

import androidx.annotation.Nullable;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

class AcknowledgeGroupedCredentialSheetView implements BottomSheetContent {
    private final View mContent;

    public AcknowledgeGroupedCredentialSheetView(View content) {
        mContent = content;
    }

    @Override
    public View getContentView() {
        return mContent;
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
        return BottomSheetContent.ContentPriority.HIGH;
    }

    @Override
    public boolean swipeToDismissEnabled() {
        return false;
    }

    @Override
    public int getSheetContentDescriptionStringId() {
        // TODO(crbug.com/372635361): Append web site to the title.
        return R.string.ack_grouped_cred_sheet_title;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        // Half-height is disabled so no need for an accessibility string.
        assert false : "This method should not be called";
        return 0;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        // TODO(crbug.com/372635361): Append web site to the title.
        return R.string.ack_grouped_cred_sheet_title;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        // TODO(crbug.com/372635361): Return  ack_grouped_cred_sheet_accepted or
        // ack_grouped_cred_sheet_rejected here depending on user choice.
        return R.string.ack_grouped_cred_sheet_accepted;
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
    public int getPeekHeight() {
        return HeightMode.DISABLED;
    }
}
