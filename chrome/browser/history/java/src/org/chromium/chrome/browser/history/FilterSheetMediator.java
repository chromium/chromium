// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.history;

import static org.chromium.build.NullUtil.assertNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.CloseCallback;
import org.chromium.chrome.browser.history.FilterSheetCoordinator.FilterItem;
import org.chromium.ui.modelutil.MVCListAdapter;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Mediator class for history filter sheet. */
@NullMarked
class FilterSheetMediator {
    private final ModelList mModelList;
    private final CloseCallback mCloseCallback;

    private @Nullable PropertyModel mSelectedModel;

    FilterSheetMediator(
            ModelList modelList, List<FilterItem> itemInfoList, CloseCallback closeCallback) {
        mModelList = modelList;
        mCloseCallback = closeCallback;
        updateItems(itemInfoList);
    }

    void updateItems(List<FilterItem> itemInfoList) {
        mModelList.clear();
        mSelectedModel = null;
        for (FilterItem info : itemInfoList) {
            PropertyModel item = generateListItem(info);
            mModelList.add(new MVCListAdapter.ListItem(0, item));
        }
    }

    private PropertyModel generateListItem(FilterItem info) {
        PropertyModel model =
                new PropertyModel.Builder(FilterSheetProperties.LIST_ITEM_KEYS)
                        .with(FilterSheetProperties.ID, info.id)
                        .with(FilterSheetProperties.ICON, info.icon)
                        .with(FilterSheetProperties.LABEL, info.label)
                        .with(FilterSheetProperties.SELECTED, false)
                        .build();
        model.set(FilterSheetProperties.CLICK_LISTENER, v -> handleClick(model));
        return model;
    }

    void resetState(@Nullable FilterItem currentItem) {
        if (mSelectedModel != null) {
            mSelectedModel.set(FilterSheetProperties.SELECTED, false);
            mSelectedModel = null;
        }
        if (currentItem != null) {
            assert currentItem.id != null : "Filter item id should be non-null.";
            mSelectedModel = getModelForId(currentItem.id);
            if (mSelectedModel == null) return;
            mSelectedModel.set(FilterSheetProperties.SELECTED, true);
        }
    }

    @VisibleForTesting
    void handleClick(PropertyModel model) {
        PropertyModel prevModel = mSelectedModel;

        String id = model.get(FilterSheetProperties.ID);
        boolean toFullHistory = prevModel != null && prevModel == model;

        if (prevModel != null) prevModel.set(FilterSheetProperties.SELECTED, false);
        if (toFullHistory) {
            mSelectedModel = null;
            mCloseCallback.onFilterItemUpdated(null);
        } else {
            mSelectedModel = model;
            mSelectedModel.set(FilterSheetProperties.SELECTED, true);
            FilterItem item =
                    new FilterItem(
                            id,
                            model.get(FilterSheetProperties.ICON),
                            model.get(FilterSheetProperties.LABEL));
            mCloseCallback.onFilterItemUpdated(item);
        }
    }

    private @Nullable PropertyModel getModelForId(String id) {
        for (MVCListAdapter.ListItem item : mModelList) {
            if (id.equals(item.model.get(FilterSheetProperties.ID))) {
                return item.model;
            }
        }
        return null;
    }

    void clickItemForTesting(String id) {
        handleClick(assertNonNull(getModelForId(id)));
    }

    @Nullable String getCurrentItemIdForTesting() {
        return mSelectedModel != null ? mSelectedModel.get(FilterSheetProperties.ID) : null;
    }
}
