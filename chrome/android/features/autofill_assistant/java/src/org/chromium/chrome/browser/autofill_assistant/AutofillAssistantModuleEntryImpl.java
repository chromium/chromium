// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.view.View;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

/**
 * Implementation of {@link AutofillAssistantModuleEntry}. This is the entry point into the
 * assistant DFM.
 */
@UsedByReflection("AutofillAssistantModuleEntryProvider.java")
public class AutofillAssistantModuleEntryImpl implements AutofillAssistantModuleEntry {
    @Override
    public AssistantDependencies createDependencies(BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, View rootView, Context context,
            @NonNull WebContents webContents,
            ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
            ApplicationViewportInsetSupplier bottomInsetProvider,
            ActivityTabProvider activityTabProvider) {
        return new AssistantDependenciesImpl(bottomSheetController, browserControls, rootView,
                context, webContents, keyboardVisibilityDelegate, bottomInsetProvider,
                activityTabProvider);
    }

    @Override
    public AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, View rootView,
            ActivityTabProvider activityTabProvider) {
        return new AutofillAssistantActionHandlerImpl(
                new OnboardingCoordinatorFactory(
                        context, bottomSheetController, browserControls, rootView),
                activityTabProvider);
    }
}
