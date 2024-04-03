// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.page_info_sheet.feedback;

import android.view.View;
import android.widget.RadioGroup;

import org.chromium.chrome.browser.share.page_info_sheet.feedback.FeedbackSheetCoordinator.Delegate;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.StateChangeReason;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
import org.chromium.components.browser_ui.widget.RadioButtonLayout;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.Optional;

class FeedbackSheetMediator extends EmptyBottomSheetObserver {

    private final Delegate mFeedbackDelegate;
    private final BottomSheetController mBottomSheetController;
    private final FeedbackSheetContent mFeedbackSheetContent;

    private final PropertyModel mModel;
    private Optional<String> mCurrentlySelectedOption = Optional.empty();

    public PropertyModel getModel() {
        return mModel;
    }

    public FeedbackSheetMediator(
            FeedbackSheetCoordinator.Delegate feedbackDelegate,
            FeedbackSheetContent feedbackSheetContent,
            BottomSheetController bottomSheetController) {
        mFeedbackDelegate = feedbackDelegate;
        mBottomSheetController = bottomSheetController;
        mFeedbackSheetContent = feedbackSheetContent;
        mModel =
                FeedbackSheetProperties.defaultModelBuilder()
                        .with(FeedbackSheetProperties.ON_ACCEPT_CLICKED, this::onAcceptClicked)
                        .with(FeedbackSheetProperties.ON_CANCEL_CLICKED, this::onCancelClicked)
                        .with(FeedbackSheetProperties.IS_ACCEPT_BUTTON_ENABLED, false)
                        .with(
                                FeedbackSheetProperties.AVAILABLE_OPTIONS,
                                feedbackDelegate.getAvailableOptions())
                        .with(
                                FeedbackSheetProperties.OPTION_SELECTED_CALLBACK,
                                this::onOptionSelectedChanged)
                        .build();

        mBottomSheetController.addObserver(this);
    }

    boolean requestShowContent() {
        return mBottomSheetController.requestShowContent(mFeedbackSheetContent, true);
    }

    @Override
    public void onSheetClosed(@StateChangeReason int reason) {
        switch (reason) {
            case StateChangeReason.BACK_PRESS,
                    StateChangeReason.SWIPE,
                    StateChangeReason.TAP_SCRIM -> {
                mFeedbackDelegate.onCanceled();
                destroySheet();
            }
        }
    }

    private void onOptionSelectedChanged(RadioGroup radioGroup, int id) {
        if (id == RadioButtonLayout.INVALID_INDEX) {
            mCurrentlySelectedOption = Optional.empty();
        } else {
            String selectedOptionKey = (String) radioGroup.findViewById(id).getTag();
            mCurrentlySelectedOption = Optional.of(selectedOptionKey);
        }
        mModel.set(
                FeedbackSheetProperties.IS_ACCEPT_BUTTON_ENABLED,
                mCurrentlySelectedOption.isPresent());
    }

    private void onCancelClicked(View view) {
        mFeedbackDelegate.onCanceled();
    }

    private void onAcceptClicked(View view) {
        assert mCurrentlySelectedOption.isPresent()
                : "Accept button should be disabled if no option is selected";

        mFeedbackDelegate.onAccepted(mCurrentlySelectedOption.get());
    }

    void destroySheet() {
        mBottomSheetController.hideContent(mFeedbackSheetContent, true);
        mBottomSheetController.removeObserver(this);
    }
}
