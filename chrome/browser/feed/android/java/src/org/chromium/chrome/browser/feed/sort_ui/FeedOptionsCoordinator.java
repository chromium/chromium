// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.feed.sort_ui;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;

import androidx.annotation.StringRes;
import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.feed.FeedServiceBridge;
import org.chromium.chrome.browser.feed.R;
import org.chromium.chrome.browser.feed.v2.ContentOrder;
import org.chromium.components.browser_ui.widget.chips.ChipProperties;
import org.chromium.components.browser_ui.widget.chips.ChipView;
import org.chromium.components.browser_ui.widget.chips.ChipViewBinder;
import org.chromium.ui.modelutil.PropertyKey;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.ArrayList;
import java.util.List;

/**
 * A coordinator for the feed options panel.
 */
public class FeedOptionsCoordinator {
    /** Method for translating between model changes and corresponding view updates. */
    static void bind(PropertyModel model, FeedOptionsView view, PropertyKey key) {
        if (key == FeedOptionsProperties.VISIBILITY_KEY) {
            view.setVisibility(model.get(FeedOptionsProperties.VISIBILITY_KEY));
        }
    }

    private final FeedOptionsView mView;
    private final Context mContext;
    private List<PropertyModel> mChipModels;
    private PropertyModel mModel;

    public FeedOptionsCoordinator(Context context) {
        // We don't use ChipsCoordinator here because RecyclerView does not play
        // nicely with the animations used, causing all chips to render with 0 height.
        this(context,
                (FeedOptionsView) LayoutInflater.from(context).inflate(
                        R.layout.feed_options_panel, null, false));
    }

    @VisibleForTesting
    FeedOptionsCoordinator(Context context, FeedOptionsView view) {
        mContext = context;
        mView = view;

        mChipModels = createAndBindChips();
        mModel = new PropertyModel.Builder(FeedOptionsProperties.getAllKeys())
                         .with(FeedOptionsProperties.VISIBILITY_KEY, false)
                         .build();
        PropertyModelChangeProcessor.create(mModel, mView, FeedOptionsCoordinator::bind);
    }

    /** Returns the view that this coordinator manages. */
    public View getView() {
        return mView;
    }

    /** Toggles visibility of the options panel. */
    public void toggleVisibility() {
        boolean isVisible = mModel.get(FeedOptionsProperties.VISIBILITY_KEY);
        // If not currently visible, we're toggling to visible.
        if (!isVisible) {
            updateSelectedChip();
        }
        mModel.set(FeedOptionsProperties.VISIBILITY_KEY, !isVisible);
    }

    /** Ensures that the options panel is completely collapsed. */
    public void ensureGone() {
        mModel.set(FeedOptionsProperties.VISIBILITY_KEY, false);
    }

    @VisibleForTesting
    void onOptionSelected(PropertyModel selectedOption) {
        for (PropertyModel model : mChipModels) {
            if (model.get(ChipProperties.SELECTED)) {
                model.set(ChipProperties.SELECTED, false);
            }
        }

        selectedOption.set(ChipProperties.SELECTED, true);
        FeedServiceBridge.setContentOrderForWebFeed(selectedOption.get(ChipProperties.ID));
    }

    private PropertyModel createChipModel(
            @ContentOrder int id, @StringRes int textId, boolean isSelected) {
        return new PropertyModel.Builder(ChipProperties.ALL_KEYS)
                .with(ChipProperties.ID, id)
                .with(ChipProperties.TEXT, mContext.getResources().getString(textId))
                .with(ChipProperties.SELECTED, isSelected)
                .with(ChipProperties.CLICK_HANDLER, this::onOptionSelected)
                .with(ChipProperties.CONTENT_DESCRIPTION, mContext.getResources().getString(textId))
                .build();
    }

    private List<PropertyModel> createAndBindChips() {
        @ContentOrder
        int currentSort = FeedServiceBridge.getContentOrderForWebFeed();
        List<PropertyModel> chipModels = new ArrayList<>();
        chipModels.add(createChipModel(ContentOrder.GROUPED, R.string.feed_sort_publisher,
                currentSort == ContentOrder.GROUPED));
        chipModels.add(createChipModel(ContentOrder.REVERSE_CHRON, R.string.latest,
                currentSort == ContentOrder.REVERSE_CHRON));

        for (PropertyModel model : chipModels) {
            ChipView chip = mView.createNewChip();
            PropertyModelChangeProcessor.create(model, chip, ChipViewBinder::bind);
        }
        return chipModels;
    }

    // Re-fetches the content order to ensure that the selected chip reflects
    // the current content order.
    private void updateSelectedChip() {
        @ContentOrder
        int currentSort = FeedServiceBridge.getContentOrderForWebFeed();
        for (PropertyModel chip : mChipModels) {
            chip.set(ChipProperties.SELECTED, chip.get(ChipProperties.ID) == currentSort);
        }
    }

    PropertyModel getModelForTest() {
        return mModel;
    }

    List<PropertyModel> getChipModelsForTest() {
        return mChipModels;
    }
}
