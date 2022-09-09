// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;
import android.view.View;

import org.chromium.base.Callback;
import org.chromium.base.supplier.Supplier;
import org.chromium.components.autofill_assistant.AssistantBrowserControlsFactory;
import org.chromium.components.autofill_assistant.AssistantDependencies;
import org.chromium.components.autofill_assistant.AssistantModuleInstallUi;
import org.chromium.components.autofill_assistant.AssistantOnboardingHelper;
import org.chromium.components.autofill_assistant.AssistantStaticDependencies;
import org.chromium.components.autofill_assistant.AutofillAssistantActionHandler;
import org.chromium.components.autofill_assistant.AutofillAssistantActionHandlerImpl;
import org.chromium.components.autofill_assistant.AutofillAssistantDirectAction;
import org.chromium.components.autofill_assistant.AutofillAssistantDirectActionImpl;
import org.chromium.components.autofill_assistant.AutofillAssistantModuleEntry;
import org.chromium.components.autofill_assistant.AutofillAssistantModuleEntryProvider;
import org.chromium.components.autofill_assistant.onboarding.OnboardingCoordinatorFactory;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.content_public.browser.WebContents;

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
                AssistantBrowserControlsFactory browserControlsFactory, View rootView,
                Supplier<WebContents> webContentsSupplier,
                AssistantStaticDependencies staticDependencies) {
            super(new OnboardingCoordinatorFactory(context, bottomSheetController,
                          staticDependencies.getBrowserContext(), browserControlsFactory, rootView,
                          staticDependencies.getAccessibilityUtil(),
                          staticDependencies.createInfoPageUtil()),
                    webContentsSupplier, staticDependencies);
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
        public AssistantOnboardingHelper createOnboardingHelper(
                WebContents webContents, AssistantDependencies dependencies) {
            return null;
        }

        @Override
        public AutofillAssistantActionHandler createActionHandler(Context context,
                BottomSheetController bottomSheetController,
                AssistantBrowserControlsFactory browserControlsFactory, View rootView,
                Supplier<WebContents> webContentsSupplier,
                AssistantStaticDependencies staticDependencies) {
            return new MockAutofillAssistantActionHandler(context, bottomSheetController,
                    browserControlsFactory, rootView, webContentsSupplier, staticDependencies);
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
    public void getModuleEntry(Callback<AutofillAssistantModuleEntry> callback,
            AssistantModuleInstallUi.Provider moduleInstallUiProvider, boolean showUi) {
        if (mCannotInstall) {
            callback.onResult(null);
            return;
        }
        mNotInstalled = false;
        super.getModuleEntry(callback, moduleInstallUiProvider, showUi);
    }
}
