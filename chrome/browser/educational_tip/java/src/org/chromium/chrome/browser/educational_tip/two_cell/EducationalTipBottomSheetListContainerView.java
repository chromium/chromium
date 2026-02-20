// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip.two_cell;

import android.content.Context;
import android.util.AttributeSet;
import android.view.LayoutInflater;
import android.view.View;
import android.widget.LinearLayout;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.educational_tip.EducationalTipCardProvider;
import org.chromium.chrome.browser.educational_tip.R;

import java.util.List;

/**
 * The container view holding multiple {@link EducationalTipBottomSheetListItemView} in a bottom
 * sheet.
 */
@NullMarked
public class EducationalTipBottomSheetListContainerView extends LinearLayout {
    // TODO(crbug.com/479597724): Implement container view.

    @Nullable private Runnable mDismissBottomSheetRunnable;

    public EducationalTipBottomSheetListContainerView(
            Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /** Adds list items views to this container view. */
    public void renderSetUpList(List<EducationalTipCardProvider> rankedEducationalTips) {
        // Clears all previous list items.
        destroy();

        for (int i = 0; i < rankedEducationalTips.size(); i++) {
            EducationalTipCardProvider educationalTip = rankedEducationalTips.get(i);
            EducationalTipBottomSheetListItemView listItemView =
                    (EducationalTipBottomSheetListItemView) createListItemView();
            listItemView.setIcon(educationalTip.getCardImage());
            listItemView.setTitle(educationalTip.getCardTitle());
            listItemView.setDescription(educationalTip.getCardDescription());
            listItemView.setOnClickListener(
                    view -> {
                        educationalTip.onCardClicked();
                        if (mDismissBottomSheetRunnable != null) {
                            // Bottom sheet should be dismissed after an item is clicked.
                            mDismissBottomSheetRunnable.run();
                        }
                    });

            addView(listItemView);
        }
    }

    public void setDismissBottomSheet(Runnable dismissBottomSheetRunnable) {
        mDismissBottomSheetRunnable = dismissBottomSheetRunnable;
    }

    View createListItemView() {
        return LayoutInflater.from(getContext())
                .inflate(R.layout.educational_tip_bottom_sheet_list_item_view, this, false);
    }

    /** Clears {@link View.OnClickListener} of each list item inside this container view. */
    public void destroy() {
        // Implement destroy for all of the bottom sheet list items
        for (int i = 0; i < getChildCount(); i++) {
            getChildAt(i).setOnClickListener(null);
        }
        removeAllViews();
    }
}
