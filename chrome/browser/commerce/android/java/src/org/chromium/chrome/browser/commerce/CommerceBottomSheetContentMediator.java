// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.commerce;

import android.view.View;
import android.view.View.AccessibilityDelegate;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetContent.HeightMode;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.modelutil.MVCListAdapter.ListItem;
import org.chromium.ui.modelutil.MVCListAdapter.ModelList;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Arrays;

@NullMarked
public class CommerceBottomSheetContentMediator {
    private final ModelList mModelList;
    private int mContentReadyCount;
    private final int mExpectedContentCount;
    private final BottomSheetController mBottomSheetController;
    private final View mCommerceBottomSheetContentContainer;
    private @Nullable CommerceBottomSheetContent mContent;

    public CommerceBottomSheetContentMediator(
            ModelList modelList,
            int expectedContentCount,
            BottomSheetController bottomSheetController,
            View commerceBottomSheetContentContainer) {
        mModelList = modelList;
        mExpectedContentCount = expectedContentCount;
        mBottomSheetController = bottomSheetController;
        mCommerceBottomSheetContentContainer = commerceBottomSheetContentContainer;
    }

    void onContentReady(@Nullable PropertyModel model) {
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

    void timeOut() {
        if (mContentReadyCount == 0 || mContent != null) return;
        showBottomSheet();
    }

    void onBottomSheetClosed() {
        if (mContent != null) {
            mBottomSheetController.hideContent(mContent, true);
        }
        mCommerceBottomSheetContentContainer.setAccessibilityDelegate(null);
        mContent = null;
        mModelList.clear();
        mContentReadyCount = 0;
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
        mContent =
                new CommerceBottomSheetContent(
                        mCommerceBottomSheetContentContainer, mBottomSheetController);
        mCommerceBottomSheetContentContainer.setAccessibilityDelegate(
                createExpandOnFocusAccessibilityDelegate());
        mBottomSheetController.requestShowContent(mContent, true);
    }

    boolean isContentWrappingContent() {
        if (mContent == null) return true;
        return mContent.getFullHeightRatio() == HeightMode.WRAP_CONTENT;
    }

    private AccessibilityDelegate createExpandOnFocusAccessibilityDelegate() {
        return new AccessibilityDelegate() {
            @Override
            public boolean onRequestSendAccessibilityEvent(
                    ViewGroup host, View child, AccessibilityEvent event) {
                // If bottom sheet content is focused under talkback, expand the sheet to full.
                if (event.getEventType() == AccessibilityEvent.TYPE_VIEW_ACCESSIBILITY_FOCUSED) {
                    if (mContent != null
                            && mBottomSheetController.getSheetState() != SheetState.FULL) {
                        mContent.setIsHalfHeightDisabled(true);
                        mBottomSheetController.expandSheet();
                    }
                }
                return super.onRequestSendAccessibilityEvent(host, child, event);
            }
        };
    }
}
