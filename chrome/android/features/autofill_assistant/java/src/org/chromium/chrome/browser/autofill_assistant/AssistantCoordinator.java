// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import androidx.annotation.Nullable;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.feedback.ScreenshotMode;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;

/**
 * The main coordinator for the Autofill Assistant, responsible for instantiating all other
 * sub-components and shutting down the Autofill Assistant.
 */
public class AssistantCoordinator {
    public static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";

    private final ChromeActivity mActivity;

    private final AssistantModel mModel;
    private AssistantBottomBarCoordinator mBottomBarCoordinator;
    private final AssistantKeyboardCoordinator mKeyboardCoordinator;
    private final AssistantOverlayCoordinator mOverlayCoordinator;

    AssistantCoordinator(ChromeActivity activity, BottomSheetController controller,
            TabObscuringHandler tabObscuringHandler,
            @Nullable AssistantOverlayCoordinator overlayCoordinator,
            AssistantKeyboardCoordinator.Delegate keyboardCoordinatorDelegate) {
        mActivity = activity;

        if (overlayCoordinator != null) {
            mModel = new AssistantModel(overlayCoordinator.getModel());
            mOverlayCoordinator = overlayCoordinator;
        } else {
            mModel = new AssistantModel();
            mOverlayCoordinator = new AssistantOverlayCoordinator(activity,
                    activity.getBrowserControlsManager(), activity.getCompositorViewHolder(),
                    controller.getScrimCoordinator(), mModel.getOverlayModel());
        }

        mBottomBarCoordinator =
                new AssistantBottomBarCoordinator(activity, mModel, mOverlayCoordinator, controller,
                        activity.getWindowAndroid().getApplicationBottomInsetProvider(),
                        tabObscuringHandler);
        mKeyboardCoordinator = new AssistantKeyboardCoordinator(activity,
                activity.getWindowAndroid().getKeyboardDelegate(),
                activity.getCompositorViewHolder(), mModel, keyboardCoordinatorDelegate,
                controller);
    }

    /** Detaches and destroys the view. */
    public void destroy() {
        mModel.setVisible(false);
        mBottomBarCoordinator.destroy();
        mBottomBarCoordinator = null;
        mOverlayCoordinator.destroy();
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
    public void showFeedback(String debugContext, @ScreenshotMode int screenshotMode) {
        Profile profile =
                Profile.fromWebContents(mActivity.getActivityTabProvider().get().getWebContents());

        HelpAndFeedbackLauncherImpl.getInstance().showFeedback(mActivity, profile,
                mActivity.getActivityTab().getUrlString(), FEEDBACK_CATEGORY_TAG, screenshotMode,
                debugContext);
    }

    public void show() {
        // Simulates native's initialization.
        mModel.setVisible(true);
        mBottomBarCoordinator.restoreState(SheetState.HALF);
    }
}
