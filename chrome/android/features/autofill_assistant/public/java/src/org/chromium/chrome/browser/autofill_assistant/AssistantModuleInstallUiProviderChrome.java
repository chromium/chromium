
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.base.Consumer;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;

/**
 * Implementation of {@link AssistantModuleInstallUi.Provider} for Chrome.
 */
public class AssistantModuleInstallUiProviderChrome implements AssistantModuleInstallUi.Provider {
    private final Tab mTab;

    public AssistantModuleInstallUiProviderChrome(Tab tab) {
        mTab = tab;
    }

    @Override
    public AssistantModuleInstallUi create(Consumer<Boolean> onFailure) {
        ModuleInstallUi ui = new ModuleInstallUi(mTab, R.string.autofill_assistant_module_title,
                new ModuleInstallUi.FailureUiListener() {
                    @Override
                    public void onFailureUiResponse(boolean retry) {
                        onFailure.accept(retry);
                    }
                });

        return new AssistantModuleInstallUi() {
            @Override
            public void showInstallStartUi() {
                ui.showInstallStartUi();
            }

            @Override
            public void showInstallFailureUi() {
                ui.showInstallFailureUi();
            }
        };
    }
}
