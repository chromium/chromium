// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.help.HelpAndFeedback;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;

/**
 * The main coordinator for the Autofill Assistant, responsible for instantiating all other
 * sub-components and shutting down the Autofill Assistant.
 */
class AssistantCoordinator {
    private static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";

    private final ChromeActivity mActivity;

    private final AssistantModel mModel;
    private AssistantBottomBarCoordinator mBottomBarCoordinator;
    private final AssistantKeyboardCoordinator mKeyboardCoordinator;
    private final AssistantOverlayCoordinator mOverlayCoordinator;

    AssistantCoordinator(ChromeActivity activity, BottomSheetController controller,
            @Nullable AssistantOverlayCoordinator overlayCoordinator) {
        mActivity = activity;

        if (overlayCoordinator != null) {
            mModel = new AssistantModel(overlayCoordinator.getModel());
            mOverlayCoordinator = overlayCoordinator;
        } else {
            mModel = new AssistantModel();
            mOverlayCoordinator =
                    new AssistantOverlayCoordinator(activity, mModel.getOverlayModel());
        }

        mBottomBarCoordinator = new AssistantBottomBarCoordinator(activity, mModel, controller);
        mKeyboardCoordinator = new AssistantKeyboardCoordinator(activity, mModel);

        activity.getCompositorViewHolder().addCompositorViewResizer(mBottomBarCoordinator);
        mModel.setVisible(true);
    }

    /** Detaches and destroys the view. */
    public void destroy() {
        if (mActivity.getCompositorViewHolder() != null) {
            mActivity.getCompositorViewHolder().removeCompositorViewResizer(mBottomBarCoordinator);
        }

        mModel.setVisible(false);
        mOverlayCoordinator.destroy();
        mBottomBarCoordinator.destroy();
        mBottomBarCoordinator = null;
    }

    /**
     * Get the model representing the current state of the UI.
     */

    public AssistantModel getModel() {
        return mModel;
    }

    // Getters to retrieve the sub coordinators.

    public AssistantBottomBarCoordinator getBottomBarCoordinator() {
        return mBottomBarCoordinator;
    }

    AssistantKeyboardCoordinator getKeyboardCoordinator() {
        return mKeyboardCoordinator;
    }

    /**
     * Show the Chrome feedback form.
     */
    public void showFeedback(String debugContext) {
        HelpAndFeedback.getInstance().showFeedback(mActivity, Profile.getLastUsedProfile(),
                mActivity.getActivityTab().getUrl(), FEEDBACK_CATEGORY_TAG,
                FeedbackContext.buildContextString(mActivity, debugContext, 4));
    }
}
