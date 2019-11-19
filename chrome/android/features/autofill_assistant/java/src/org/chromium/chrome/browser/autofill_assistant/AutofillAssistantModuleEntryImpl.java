// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.os.Bundle;

import androidx.annotation.NonNull;

import org.chromium.base.annotations.UsedByReflection;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.autofill_assistant.metrics.OnBoarding;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.ScrimView;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

import java.util.Map;

/**
 * Implementation of {@link AutofillAssistantModuleEntry}. This is the entry point into the
 * assistant DFM.
 */
@UsedByReflection("AutofillAssistantModuleEntryProvider.java")
public class AutofillAssistantModuleEntryImpl implements AutofillAssistantModuleEntry {
    @Override
    public void start(@NonNull Tab tab, @NonNull WebContents webContents, boolean skipOnboarding,
            String initialUrl, Map<String, String> parameters, String experimentIds,
            Bundle intentExtras) {
        if (skipOnboarding) {
            AutofillAssistantMetrics.recordOnBoarding(OnBoarding.OB_NOT_SHOWN);
            AutofillAssistantClient.fromWebContents(tab.getWebContents())
                    .start(initialUrl, parameters, experimentIds, intentExtras, null);
            return;
        }

        ChromeActivity activity = tab.getActivity();
        AssistantOnboardingCoordinator onboardingCoordinator = new AssistantOnboardingCoordinator(
                experimentIds, activity, activity.getBottomSheetController(), tab);
        onboardingCoordinator.show(accepted -> {
            if (!accepted) return;

            AutofillAssistantClient.fromWebContents(tab.getWebContents())
                    .start(initialUrl, parameters, experimentIds, intentExtras,
                            onboardingCoordinator);
        });
    }

    @Override
    public AutofillAssistantActionHandler createActionHandler(Context context,
            BottomSheetController bottomSheetController, ScrimView scrimView,
            GetCurrentTab getCurrentTab) {
        return new AutofillAssistantActionHandlerImpl(
                context, bottomSheetController, scrimView, getCurrentTab);
    }
}
