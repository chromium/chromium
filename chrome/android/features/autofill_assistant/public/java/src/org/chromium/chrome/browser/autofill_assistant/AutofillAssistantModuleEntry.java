// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.view.View;

import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.content_public.browser.WebContents;

/**
 * Interface between base module and assistant DFM.
 */
@ModuleInterface(module = "autofill_assistant",
        impl = "org.chromium.chrome.browser.autofill_assistant.AutofillAssistantModuleEntryImpl")
public interface AutofillAssistantModuleEntry {
    /**
     * Creates a concrete {@code AssistantDependenciesFactory} object. Its contents are opaque to
     * the outside of the module.
     */
    AssistantDependenciesFactory createDependenciesFactory();

    /**
     * Creates a concrete {@code AssistantOnboardingHelper} object. Its contents are opaque to
     * the outside of the module.
     */
    AssistantOnboardingHelper createOnboardingHelper(
            WebContents webContents, AssistantDependencies dependencies);

    /**
     * Returns a {@link AutofillAssistantActionHandler} instance tied to the activity owning the
     * given bottom sheet, and scrim view.
     *
     * @param context activity context
     * @param bottomSheetController bottom sheet controller instance of the activity
     * @param browserControls provider of browser controls state
     * @param rootView root view of the activity
     * @param activityTabProvider activity tab provider
     * @param dependenciesFactory creates platform-specific dependencies
     */
    AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController,
            BrowserControlsStateProvider browserControls, View rootView,
            ActivityTabProvider activityTabProvider,
            AssistantDependenciesFactory dependenciesFactory);
}
