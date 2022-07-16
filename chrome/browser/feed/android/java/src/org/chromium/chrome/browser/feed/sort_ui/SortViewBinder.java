// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.feed.sort_ui;

import org.chromium.ui.modelutil.ListModel;
import org.chromium.ui.modelutil.ListModelChangeProcessor;
import org.chromium.ui.modelutil.PropertyModel;

/**
 * View binder that links between a property model with {@link SortChipProperties} and a SortView.
 */
public class SortViewBinder
        implements ListModelChangeProcessor.ViewBinder<ListModel<PropertyModel>, SortView, Void> {
    @Override
    public void onItemsInserted(
            ListModel<PropertyModel> modelList, SortView view, int index, int count) {
        // Assumes we are never inserting chips between other pre-existing chips.
        // This will always add chips at the end.
        for (int i = index; i < index + count; i++) {
            PropertyModel model = modelList.get(i);
            view.addButton(model.get(SortChipProperties.NAME_KEY),
                    model.get(SortChipProperties.ON_SELECT_CALLBACK_KEY),
                    model.get(SortChipProperties.IS_INITIALLY_SELECTED_KEY));
        }
    }

    @Override
    public void onItemsRemoved(
            ListModel<PropertyModel> model, SortView view, int index, int count) {
        // Does nothing. We don't support removal.
    }

    @Override
    public void onItemsChanged(
            ListModel<PropertyModel> model, SortView view, int index, int count, Void payload) {
        // Does nothing. Nothing can be changed right now.
    }
}
