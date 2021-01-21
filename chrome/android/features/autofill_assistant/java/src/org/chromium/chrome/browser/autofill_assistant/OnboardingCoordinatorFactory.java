// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

import java.util.Map;

/**
 * Onboarding coordinator factory to decide which onboarding subclass to use.
 */
public class OnboardingCoordinatorFactory {
    /** Creates an instance of onboarding coordinator subclass to be used. */
    public static BaseOnboardingCoordinator createOnboardingCoordinator(
            boolean isDialogOnboardingEnabled, String experimentIds, Map<String, String> parameters,
            Context context, BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls,
            CompositorViewHolder compositorViewHolder) {
        return isDialogOnboardingEnabled
                ? new DialogOnboardingCoordinator(experimentIds, parameters, context)
                : new BottomSheetOnboardingCoordinator(experimentIds, parameters, context,
                        bottomSheetController, browserControls, compositorViewHolder,
                        bottomSheetController.getScrimCoordinator());
    }
}