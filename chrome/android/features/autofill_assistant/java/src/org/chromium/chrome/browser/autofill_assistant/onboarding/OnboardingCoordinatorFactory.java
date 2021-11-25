// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.onboarding;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.util.AccessibilityUtil;

import java.util.Map;

/**
 * Onboarding coordinator factory which facilitates the creation of onboarding coordinators.
 */
public class OnboardingCoordinatorFactory {
    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsStateProvider mBrowserControls;
    private final View mRootView;
    private final AccessibilityUtil mAccessibilityUtil;

    public OnboardingCoordinatorFactory(Context context,
            BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, View rootView,
            AccessibilityUtil accessibilityUtil) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mBrowserControls = browserControls;
        mRootView = rootView;
        mAccessibilityUtil = accessibilityUtil;
    }

    /**
     * Creates an onboarding coordinator ready to be shown in the bottom sheet.
     */
    public BaseOnboardingCoordinator createBottomSheetOnboardingCoordinator(
            String experimentIds, Map<String, String> parameters) {
        return new BottomSheetOnboardingCoordinator(experimentIds, parameters, mContext,
                mBottomSheetController, mBrowserControls, mRootView,
                mBottomSheetController.getScrimCoordinator(), mAccessibilityUtil);
    }

    /**
     * Creates an onboarding coordinator that will appear as a standalong popup dialog.
     */
    public BaseOnboardingCoordinator createDialogOnboardingCoordinator(
            String experimentIds, Map<String, String> parameters) {
        return new DialogOnboardingCoordinator(experimentIds, parameters, mContext);
    }
}
