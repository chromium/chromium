// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.app.Activity;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.autofill_assistant.overlay.AssistantOverlayCoordinator;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.feedback.HelpAndFeedbackLauncherImpl;
import org.chromium.chrome.browser.feedback.ScreenshotMode;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.ui.TabObscuringHandler;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController.SheetState;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

/**
 * The main coordinator for the Autofill Assistant, responsible for instantiating all other
 * sub-components and shutting down the Autofill Assistant.
 */
public class AssistantCoordinator {
    public static final String FEEDBACK_CATEGORY_TAG =
            "com.android.chrome.USER_INITIATED_FEEDBACK_REPORT_AUTOFILL_ASSISTANT";

    private final Activity mActivity;

    private final AssistantModel mModel;
    private AssistantBottomBarCoordinator mBottomBarCoordinator;
    private final AssistantKeyboardCoordinator mKeyboardCoordinator;
    private final AssistantOverlayCoordinator mOverlayCoordinator;
    private final Supplier<Tab> mCurrentTabSupplier;

    AssistantCoordinator(Activity activity, BottomSheetController controller,
            TabObscuringHandler tabObscuringHandler,
            @Nullable AssistantOverlayCoordinator overlayCoordinator,
            AssistantKeyboardCoordinator.Delegate keyboardCoordinatorDelegate,
            @NonNull ActivityKeyboardVisibilityDelegate keyboardDelegate,
            @NonNull CompositorViewHolder compositorViewHolder,
            @NonNull Supplier<Tab> currentTabSupplier,
            @NonNull BrowserControlsManager browserControlsManager,
            @NonNull ApplicationViewportInsetSupplier applicationBottomInsetProvider) {
        mActivity = activity;
        mCurrentTabSupplier = currentTabSupplier;

        if (overlayCoordinator != null) {
            mModel = new AssistantModel(overlayCoordinator.getModel());
            mOverlayCoordinator = overlayCoordinator;
        } else {
            mModel = new AssistantModel();
            mOverlayCoordinator = new AssistantOverlayCoordinator(activity, browserControlsManager,
                    compositorViewHolder, controller.getScrimCoordinator(),
                    mModel.getOverlayModel());
        }

        mBottomBarCoordinator = new AssistantBottomBarCoordinator(activity, mModel,
                mOverlayCoordinator, controller, applicationBottomInsetProvider,
                tabObscuringHandler, browserControlsManager);
        mKeyboardCoordinator = new AssistantKeyboardCoordinator(activity, keyboardDelegate,
                compositorViewHolder, mModel, keyboardCoordinatorDelegate, controller);
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
        Tab currentTab = mCurrentTabSupplier.get();
        if (currentTab == null) return;
        Profile profile = Profile.fromWebContents(currentTab.getWebContents());

        HelpAndFeedbackLauncherImpl.getInstance().showFeedback(mActivity, profile,
                currentTab.getUrl().getSpec(), FEEDBACK_CATEGORY_TAG, screenshotMode, debugContext);
    }

    public void show() {
        // Simulates native's initialization.
        mModel.setVisible(true);
        mBottomBarCoordinator.restoreState(SheetState.HALF);
    }
}
