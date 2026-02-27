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
import org.chromium.chrome.browser.setup_list.SetupListCompletable;

import java.util.List;

/**
 * The container view holding multiple {@link EducationalTipSetupListBottomSheetListItemView} in a
 * bottom sheet.
 */
@NullMarked
public class EducationalTipBottomSheetListContainerView extends LinearLayout {
    // TODO(crbug.com/479597724): Implement container view.

    @Nullable private Runnable mDismissBottomSheetRunnable;

    public EducationalTipBottomSheetListContainerView(
            Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    /**
     * Adds list items views to this container view. Creates each list item view based on their
     * corresponding {@link EducationalTipCardProvider}
     *
     * @param rankedEducationalTips list of {@link EducationalTipCardProvider} that will be
     *     displayed in the container.
     */
    public void renderSetUpList(List<EducationalTipBottomSheetItem> rankedEducationalTips) {
        // Clears all previous list items.
        destroy();

        for (int i = 0; i < rankedEducationalTips.size(); i++) {
            EducationalTipBottomSheetItem item = rankedEducationalTips.get(i);
            EducationalTipCardProvider educationalTip = item.provider;
            EducationalTipSetupListBottomSheetListItemView listItemView =
                    (EducationalTipSetupListBottomSheetListItemView) createListItemView();
            SetupListCompletable.CompletionState itemCompletionState = item.completionState;

            listItemView.setTitle(educationalTip.getCardTitle());
            listItemView.setDescription(educationalTip.getCardDescription());
            if (itemCompletionState != null && itemCompletionState.isCompleted) {
                listItemView.setIcon(itemCompletionState.iconRes);
                listItemView.displayAsCompleted();
            } else {
                listItemView.setIcon(educationalTip.getCardImage());
                listItemView.setOnClickListener(
                        view -> {
                            educationalTip.onCardClicked();
                            if (mDismissBottomSheetRunnable != null) {
                                // Bottom sheet should be dismissed after an item is clicked.
                                mDismissBottomSheetRunnable.run();
                            }
                        });
            }
            // TODO(crbug.com/469425754): Consider passing a Position enum to the list item view
            // to let it handle its own background styling.
            // Set the custom border radius for first and last list items.
            if (i == 0) {
                listItemView.setBackground(
                        getContext()
                                .getDrawable(
                                        R.drawable
                                                .educational_tip_setup_list_bottom_sheet_first_list_item_background));
            } else if (i == rankedEducationalTips.size() - 1) {
                listItemView.setBackground(
                        getContext()
                                .getDrawable(
                                        R.drawable
                                                .educational_tip_setup_list_bottom_sheet_last_list_item_background));
            }

            addView(listItemView);
        }
    }

    public void setDismissBottomSheet(Runnable dismissBottomSheetRunnable) {
        mDismissBottomSheetRunnable = dismissBottomSheetRunnable;
    }

    View createListItemView() {
        return LayoutInflater.from(getContext())
                .inflate(
                        R.layout.educational_tip_setup_list_bottom_sheet_list_item_view,
                        this,
                        false);
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
