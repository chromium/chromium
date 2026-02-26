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
import org.chromium.chrome.browser.setup_list.SetupListModuleUtils;

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
    public void renderSetUpList(List<EducationalTipCardProvider> rankedEducationalTips) {
        // Clears all previous list items.
        destroy();

        List<Integer> rankedModuleTypes = SetupListModuleUtils.getRankedModuleTypes();
        for (int i = 0; i < rankedEducationalTips.size(); i++) {
            EducationalTipCardProvider educationalTip = rankedEducationalTips.get(i);
            EducationalTipSetupListBottomSheetListItemView listItemView =
                    (EducationalTipSetupListBottomSheetListItemView) createListItemView();
            // TODO(crbug.com/479597724): Create cached completion state to module type map.
            SetupListCompletable.CompletionState itemCompletionState = null;
            if (i < rankedModuleTypes.size()) {
                itemCompletionState =
                        SetupListCompletable.getCompletionState(
                                educationalTip, rankedModuleTypes.get(i));
            }

            listItemView.setOnClickListener(
                    view -> {
                        educationalTip.onCardClicked();
                        if (mDismissBottomSheetRunnable != null) {
                            // Bottom sheet should be dismissed after an item is clicked.
                            mDismissBottomSheetRunnable.run();
                        }
                    });
            if (itemCompletionState != null && itemCompletionState.isCompleted) {
                listItemView.setIcon(itemCompletionState.iconRes);
                listItemView.displayAsCompleted();
            } else {
                listItemView.setIcon(educationalTip.getCardImage());
            }
            listItemView.setTitle(educationalTip.getCardTitle());
            listItemView.setDescription(educationalTip.getCardDescription());
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
