// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history_clusters;

import android.content.Context;
import android.util.AttributeSet;
import android.widget.FrameLayout;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.Callback;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.browser_ui.widget.chips.ChipsCoordinator;
import org.chromium.ui.base.ViewUtils;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;

import java.util.List;

class HistoryClustersRelatedSearchesChipLayout extends FrameLayout {
    private DividerView mDividerView;
    private ChipsCoordinator mChipsCoordinator;

    private final ModelList mChipList = new ModelList();
    private Callback<String> mOnClickHandler;

    public HistoryClustersRelatedSearchesChipLayout(
            @NonNull Context context, @Nullable AttributeSet attrs) {
        super(context, attrs);
    }

    @Override
    public void onFinishInflate() {
        super.onFinishInflate();

        mChipsCoordinator = new ChipsCoordinator(getContext(), mChipList);
        mChipsCoordinator.setSpaceItemDecoration(
                getResources().getDimensionPixelSize(R.dimen.related_search_chip_list_chip_spacing),
                getResources()
                        .getDimensionPixelSize(R.dimen.related_search_chip_list_side_padding));
        addView(mChipsCoordinator.getView());
        mDividerView = new DividerView(getContext(), null, 0, R.style.HorizontalDivider);
        mDividerView.addToParent(this, generateDefaultLayoutParams());
    }

    void setOnChipClickHandler(Callback<String> onClickHandler) {
        mOnClickHandler = onClickHandler;
    }

    void setRelatedSearches(List<String> relatedSearches) {
        for (int i = 0; i < relatedSearches.size(); i++) {
            String search = relatedSearches.get(i);
            ListItem listItem =
                    ChipsCoordinator.buildChipListItem(
                            i,
                            search,
                            (unused) -> mOnClickHandler.onResult(search),
                            R.drawable.ic_search);
            listItem.model.set(ChipProperties.ENABLED, true);
            mChipList.add(listItem);
        }
    }

    void setDividerVisibility(boolean visible) {
        mDividerView.setVisibility(visible ? VISIBLE : GONE);
    }

    void setHasThickDivider(boolean hasThickDivider) {
        mDividerView.setIsThickDivider(hasThickDivider);
        LayoutParams layoutParams = (LayoutParams) mChipsCoordinator.getView().getLayoutParams();
        if (hasThickDivider) {
            layoutParams.bottomMargin =
                    getResources().getDimensionPixelSize(R.dimen.divider_margin);
        } else {
            layoutParams.bottomMargin = 0;
        }

        ViewUtils.requestLayout(
                this, "HistoryClustersRelatedSearchesChipLayout.setHasThickDivider");
    }
}
