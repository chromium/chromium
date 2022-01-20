// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant.onboarding;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.autofill_assistant.AssistantInfoPageUtil;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.ui.util.AccessibilityUtil;

import java.util.Arrays;
import java.util.List;
import java.util.Map;

/**
 * Onboarding coordinator factory which facilitates the creation of onboarding coordinators.
 */
public class OnboardingCoordinatorFactory {
    // See details and variant breakdown in b/209561294.
    private static final String SPLIT_ONBOARDING_VARIANT_A_EXPERIMENT_ID = "4702489";
    private static final String SPLIT_ONBOARDING_VARIANT_B_EXPERIMENT_ID = "4702490";

    private final Context mContext;
    private final BottomSheetController mBottomSheetController;
    private final BrowserControlsStateProvider mBrowserControls;
    private final View mRootView;
    private final AccessibilityUtil mAccessibilityUtil;
    private final AssistantInfoPageUtil mInfoPageUtil;

    public OnboardingCoordinatorFactory(Context context,
            BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, View rootView,
            AccessibilityUtil accessibilityUtil, AssistantInfoPageUtil infoPageUtil) {
        mContext = context;
        mBottomSheetController = bottomSheetController;
        mBrowserControls = browserControls;
        mRootView = rootView;
        mAccessibilityUtil = accessibilityUtil;
        mInfoPageUtil = infoPageUtil;
    }

    /**
     * Creates an onboarding coordinator ready to be shown in the bottom sheet.
     */
    public BaseOnboardingCoordinator createBottomSheetOnboardingCoordinator(
            String experimentIds, Map<String, String> parameters) {
        List<String> experimentIdsList = Arrays.asList(experimentIds.split(","));
        if (experimentIdsList.contains(SPLIT_ONBOARDING_VARIANT_A_EXPERIMENT_ID)) {
            return new BottomSheetOnboardingWithPopupCoordinator(mInfoPageUtil, experimentIds,
                    parameters, mContext, mBottomSheetController, mBrowserControls, mRootView,
                    mBottomSheetController.getScrimCoordinator(), mAccessibilityUtil);
        } else if (experimentIdsList.contains(SPLIT_ONBOARDING_VARIANT_B_EXPERIMENT_ID)) {
            return new BottomSheetOnboardingWithPopupAndBubbleCoordinator(mInfoPageUtil,
                    experimentIds, parameters, mContext, mBottomSheetController, mBrowserControls,
                    mRootView, mBottomSheetController.getScrimCoordinator(), mAccessibilityUtil);
        }

        return new BottomSheetOnboardingCoordinator(mInfoPageUtil, experimentIds, parameters,
                mContext, mBottomSheetController, mBrowserControls, mRootView,
                mBottomSheetController.getScrimCoordinator(), mAccessibilityUtil);
    }

    /**
     * Creates an onboarding coordinator that will appear as a standalong popup dialog.
     */
    public BaseOnboardingCoordinator createDialogOnboardingCoordinator(
            String experimentIds, Map<String, String> parameters) {
        return new DialogOnboardingCoordinator(mInfoPageUtil, experimentIds, parameters, mContext);
    }
}
