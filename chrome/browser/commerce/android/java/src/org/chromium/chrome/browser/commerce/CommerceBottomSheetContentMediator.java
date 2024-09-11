// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import androidx.annotation.Nullable;

import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

public class CommerceBottomSheetContentMediator {
    private final ModelList mModelList;
    private int mContentReadyCount;
    private final int mExpectedContentCount;

    public CommerceBottomSheetContentMediator(ModelList modelList, int expectedContentCount) {
        mModelList = modelList;
        mExpectedContentCount = expectedContentCount;
    }

    public void onContentReady(@Nullable PropertyModel model) {
        mContentReadyCount++;

        if (model == null) {
            requestToShowBottomSheetIfReady();
            return;
        }

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
        requestToShowBottomSheetIfReady();
    }

    public void timeOut() {
        showBottomSheet();
    }

    private boolean isValidPropertyModel(PropertyModel model) {
        return model.getAllProperties()
                .containsAll(Arrays.asList(CommerceBottomSheetContentProperties.ALL_KEYS));
    }

    private void requestToShowBottomSheetIfReady() {
        if (mContentReadyCount < mExpectedContentCount) return;
        showBottomSheet();
    }

    private void showBottomSheet() {
        // TODO(b/362359865): create BottomSheetContent and request to show with the
        // BottomSheetController.
    }
}
