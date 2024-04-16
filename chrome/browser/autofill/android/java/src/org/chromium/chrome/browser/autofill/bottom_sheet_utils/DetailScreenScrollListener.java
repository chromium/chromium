// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill.bottom_sheet_utils;

import static org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState.HALF;

import androidx.annotation.NonNull;
import androidx.recyclerview.widget.RecyclerView;

import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Listener for scroll events of the recycler view holding addresses and credit cards.
 * TODO(crbug.com/40260900): Add test coverage for this class.
 */
public class DetailScreenScrollListener extends RecyclerView.OnScrollListener {
    private final BottomSheetController mBottomSheetController;

    private int mY;

    public DetailScreenScrollListener(BottomSheetController bottomSheetController) {
        mBottomSheetController = bottomSheetController;
    }

    @Override
    public void onScrolled(@NonNull RecyclerView recyclerView, int dx, int dy) {
        super.onScrolled(recyclerView, dx, dy);
        mY = recyclerView.computeVerticalScrollOffset();
        if (isScrolledToTop() && mBottomSheetController.getSheetState() == HALF) {
            recyclerView.suppressLayout(/* suppress= */ true);
        }
    }

    public void reset() {
        mY = 0;
    }

    public boolean isScrolledToTop() {
        return mY == 0;
    }
}
