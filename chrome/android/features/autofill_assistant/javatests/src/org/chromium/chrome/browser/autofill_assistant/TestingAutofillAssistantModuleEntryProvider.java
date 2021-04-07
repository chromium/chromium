// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.ActivityKeyboardVisibilityDelegate;
import org.chromium.ui.base.ApplicationViewportInsetSupplier;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;

/**
 * Implementation of {@link AutofillAssistantModuleEntryProvider} that can be manipulated to
 * simulate missing or uninstallable module.
 */
class TestingAutofillAssistantModuleEntryProvider extends AutofillAssistantModuleEntryProvider {
    private boolean mNotInstalled;
    private boolean mCannotInstall;

    /*
     * Mock action handler. We only override returning dynamic actions.
     *
     * TODO(crbug/806868): Inject a service also for the DirectAction path and get rid of this
     * mock.
     */
    static class MockAutofillAssistantActionHandler extends AutofillAssistantActionHandlerImpl {
        public MockAutofillAssistantActionHandler(Context context,
                BottomSheetController bottomSheetController,
                BrowserControlsStateProvider browserControls,
                CompositorViewHolder compositorViewHolder,
                ActivityTabProvider activityTabProvider) {
            super(new OnboardingCoordinatorFactory(
                          context, bottomSheetController, browserControls, compositorViewHolder),
                    activityTabProvider);
        }

        @Override
        public List<AutofillAssistantDirectAction> getActions() {
            String[] search = new String[] {"search"};
            String[] required = new String[] {"SEARCH_QUERY"};
            String[] optional = new String[] {"arg2"};
            String[] action2 = new String[] {"action2", "action2_alias"};
            AutofillAssistantDirectAction[] actions = new AutofillAssistantDirectActionImpl[] {
                    new AutofillAssistantDirectActionImpl(search, required, optional),
                    new AutofillAssistantDirectActionImpl(action2, required, optional)};
            return new ArrayList<>(Arrays.asList(actions));
        }
    }

    /** Mock module entry. */
    static class MockAutofillAssistantModuleEntry implements AutofillAssistantModuleEntry {
        @Override
        public AssistantDependencies createDependencies(BottomSheetController bottomSheetController,
                BrowserControlsStateProvider browserControls,
                CompositorViewHolder compositorViewHolder, Context context,
                @NonNull WebContents webContents,
                ActivityKeyboardVisibilityDelegate keyboardVisibilityDelegate,
                ApplicationViewportInsetSupplier bottomInsetProvider,
                ActivityTabProvider activityTabProvider) {
            return null;
        }

        @Override
        public void start(
                AssistantDependencies assistantDependencies, TriggerContext triggerContext) {}

        @Override
        public AutofillAssistantActionHandler createActionHandler(Context context,
                BottomSheetController bottomSheetController,
                BrowserControlsStateProvider browserControls,
                CompositorViewHolder compositorViewHolder,
                ActivityTabProvider activityTabProvider) {
            return new MockAutofillAssistantActionHandler(context, bottomSheetController,
                    browserControls, compositorViewHolder, activityTabProvider);
        }
    }

    /** The module is already installed. This is the default state. */
    public void setInstalled() {
        mNotInstalled = false;
        mCannotInstall = false;
    }

    /** The module is not installed, but can be installed. */
    public void setNotInstalled() {
        mNotInstalled = true;
        mCannotInstall = false;
    }

    /** The module is not installed, and cannot be installed. */
    public void setCannotInstall() {
        mNotInstalled = true;
        mCannotInstall = true;
    }

    @Override
    public AutofillAssistantModuleEntry getModuleEntryIfInstalled() {
        if (mNotInstalled) return null;
        return new MockAutofillAssistantModuleEntry();
    }

    @Override
    public void getModuleEntry(
            Tab tab, Callback<AutofillAssistantModuleEntry> callback, boolean showUi) {
        if (mCannotInstall) {
            callback.onResult(null);
            return;
        }
        mNotInstalled = false;
        super.getModuleEntry(tab, callback, showUi);
    }
}
