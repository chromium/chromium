// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.view.View;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/**
 * Implementation of {@link AutofillAssistantModuleEntry}. This is the entry point into the
 * assistant DFM.
 */
@UsedByReflection("AutofillAssistantModuleEntryProvider.java")
public class AutofillAssistantModuleEntryImpl implements AutofillAssistantModuleEntry {
    @Override
    public AssistantDependenciesFactory createDependenciesFactory() {
        return new AssistantDependenciesFactoryChrome();
    }

    @Override
    public AssistantOnboardingHelper createOnboardingHelper(AssistantDependencies dependencies) {
        return new AssistantOnboardingHelperImpl(dependencies);
    }

    @Override
    public AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, View rootView,
            ActivityTabProvider activityTabProvider,
            AssistantDependenciesFactory dependenciesFactory) {
        return new AutofillAssistantActionHandlerImpl(
                new OnboardingCoordinatorFactory(context, bottomSheetController, browserControls,
                        rootView,
                        dependenciesFactory.createStaticDependencies().getAccessibilityUtil()),
                activityTabProvider, dependenciesFactory);
    }
}
