// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.compositor.bottombar.contextualsearch;

import android.view.View;

import org.chromium.chrome.R;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent;

class ContextualSearchSheetContent implements BottomSheetContent {
    private final View mView;
    private final float mFullHeightFraction;

    /**
     * Construct the coordinator.
     * @param view The view for the bottom sheet.
     * @param fullHeightFraction The fraction for the height the content when fully expanded.
     */
    public ContextualSearchSheetContent(View view, float fullHeightFraction) {
        mView = view;
        mFullHeightFraction = fullHeightFraction;
    }

    // region BottomSheetContent implementation
    // ---------------------------------------------------------------------------------------------

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
        return true;
    }

    @Override
    public float getFullHeightRatio() {
        return mFullHeightFraction;
    }

    // TODO(sinansahin): These are the temporary strings borrowed from the Preview Tab to avoid a
    // Resources$NotFoundException. We can replace them once the real strings are ready.
    @Override
    public int getSheetContentDescriptionStringId() {
        return R.string.ephemeral_tab_sheet_description;
    }

    @Override
    public int getSheetHalfHeightAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_opened_half;
    }

    @Override
    public int getSheetFullHeightAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_opened_full;
    }

    @Override
    public int getSheetClosedAccessibilityStringId() {
        return R.string.ephemeral_tab_sheet_closed;
    }

    // ---------------------------------------------------------------------------------------------
    // endregion
}
