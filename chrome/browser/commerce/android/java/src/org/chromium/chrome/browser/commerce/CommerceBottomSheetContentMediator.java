// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

public class CommerceBottomSheetContentMediator implements CommerceBottomSheetContentListener {
    private final ModelList mModelList;

    public CommerceBottomSheetContentMediator(ModelList modelList) {
        mModelList = modelList;
    }

    @Override
    public void onContentReady(PropertyModel model) {
        assert isValidPropertyModel(model)
                : "Miss required property in PropertyModel from"
                        + " CommerceBottomSheetContentProperties.";
        int index = 0;
        int currentType = model.get(CommerceBottomSheetContentProperties.TYPE);
        for (; index < mModelList.size(); index++) {
            int type = mModelList.get(index).model.get(CommerceBottomSheetContentProperties.TYPE);

            assert currentType != type : "There can only be one view per commerce content type";

            if (currentType < type) {
                break;
            }
        }

        mModelList.add(index, new ListItem(0, model));
    }

    private boolean isValidPropertyModel(PropertyModel model) {
        return model.getAllProperties()
                .containsAll(Arrays.asList(CommerceBottomSheetContentProperties.ALL_KEYS));
    }
}
