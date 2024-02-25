// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.search_engines.choice_screen;

import androidx.annotation.Nullable;

import org.chromium.base.Log;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.stream.Collectors;
import java.util.stream.IntStream;

/**
 * Contains the controller logic of the search engine choice screen component.
 * It communicates with data providers and native backends to update a model based on {@link
 * ChoiceScreenProperties}.
 */
class ChoiceScreenMediator {
    private static final String TAG = "SearchEngines";

    private final PropertyModel mPropertyModel;
    private final ChoiceScreenDelegate mDelegate;

    /**
     * Index of the currently selected item in the model's list of items, or {@code null} if no item
     * is currently selected.
     */
    private @Nullable Integer mSelectedItemPosition;

    /**
     * Whether the user confirmed their choice. Once it gets set to {@code true}, further changes to
     * the UI will be blocked.
     */
    private boolean mHasConfirmedChoice;

    /**
     * Constructs the choice screen mediator.
     *
     * @param propertyModel model for the choice screen, will be updated as events are received by
     *         this mediator.
     * @param delegate provides backend access to get the data to be displayed and persist the user
     *         choice.
     */
    ChoiceScreenMediator(PropertyModel propertyModel, ChoiceScreenDelegate delegate) {
        mPropertyModel = propertyModel;
        mDelegate = delegate;

        List<MVCListAdapter.ListItem> itemModels =
                IntStream.range(0, mDelegate.getSearchEngines().size())
                        .mapToObj(
                                (index) ->
                                        ChoiceScreenProperties.Item.createPropertyModel(
                                                mDelegate
                                                        .getSearchEngines()
                                                        .get(index)
                                                        .getShortName(),
                                                () -> onItemClicked(index)))
                        .map(
                                (model) ->
                                        new MVCListAdapter.ListItem(
                                                ChoiceScreenProperties.DEFAULT_TYPE, model))
                        .collect(Collectors.toList());
        mPropertyModel.get(ChoiceScreenProperties.ITEM_MODELS).set(itemModels);

        setButtonListenersEnabled(false);
    }

    private void onConfirmClicked() {
        Log.d(TAG, "confirmChoice()");
        if (mHasConfirmedChoice) return;
        if (mSelectedItemPosition == null) return;
        mHasConfirmedChoice = true;

        setButtonListenersEnabled(false);
        mDelegate.onChoiceMade(
                mDelegate.getSearchEngines().get(mSelectedItemPosition).getKeyword());
    }

    private void onItemClicked(int index) {
        // Prevent the selection to change if we started processing it.
        if (mHasConfirmedChoice) return;

        Integer oldIndex = mSelectedItemPosition;

        // If the user re-selects the current item, we clear the selection.
        mSelectedItemPosition =
                mSelectedItemPosition == null || mSelectedItemPosition != index ? index : null;
        Log.d(TAG, "onItemClicked(%d), old=%s, new=%s", index, oldIndex, mSelectedItemPosition);
        setButtonListenersEnabled(mSelectedItemPosition != null);

        MVCListAdapter.ModelList modelList = mPropertyModel.get(ChoiceScreenProperties.ITEM_MODELS);
        if (oldIndex != null) {
            updateItemModel(modelList.get(oldIndex), false);
        }
        if (mSelectedItemPosition != null) {
            updateItemModel(modelList.get(mSelectedItemPosition), true);
        }
    }

    private void setButtonListenersEnabled(boolean enabled) {
        @Nullable Runnable onButtonClicked = enabled ? this::onConfirmClicked : null;
        mPropertyModel.set(ChoiceScreenProperties.ON_PRIMARY_CLICKED, onButtonClicked);
    }

    private void updateItemModel(MVCListAdapter.ListItem item, boolean isSelected) {
        item.model.set(ChoiceScreenProperties.Item.IS_SELECTED, isSelected);
    }
}
