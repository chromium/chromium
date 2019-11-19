// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.components.module_installer.builder.ModuleInterface;
import org.chromium.content_public.browser.WebContents;

import java.util.Map;

/**
 * Interface between base module and assistant DFM.
 */
@ModuleInterface(module = "autofill_assistant",
        impl = "org.chromium.chrome.browser.autofill_assistant.AutofillAssistantModuleEntryImpl")
interface AutofillAssistantModuleEntry {
    /**
     * Starts Autofill Assistant on the current tab of the given chrome activity.
     *
     * <p>When started this way, Autofill Assistant appears immediately in the bottom sheet, expects
     * a single autostartable script for the tab's current URL, runs that script until the end and
     * disappears.
     */
    void start(@NonNull Tab tab, @NonNull WebContents webContents, boolean skipOnboarding,
            String initialUrl, Map<String, String> parameters, String experimentIds,
            Bundle intentExtras);
    /**
     * Returns a {@link AutofillAssistantActionHandler} instance tied to the activity owning the
     * given bottom sheet, and scrim view.
     *
     * @param context activity context
     * @param bottomSheetController bottom sheet controller instance of the activity
     * @param scrimView scrim view of the activity
     * @param getCurrentTab a way to get the activity's current tab, if there is any
     */
    AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController, ScrimView scrimView,
            GetCurrentTab getCurrentTab);
}
