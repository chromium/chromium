
// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import android.content.Context;

import org.chromium.base.Consumer;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.modules.ModuleInstallUi;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.autofill_assistant.AssistantModuleInstallUi;
import org.chromium.ui.base.WindowAndroid;

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
        ModuleInstallUi.Delegate moduleInstallUiDelegate = new ModuleInstallUi.Delegate() {
            @Override
            public WindowAndroid getWindowAndroid() {
                return mTab.getWindowAndroid();
            }

            @Override
            public Context getContext() {
                return mTab.getWindowAndroid() != null ? mTab.getWindowAndroid().getActivity().get()
                                                       : null;
            }
        };
        ModuleInstallUi ui = new ModuleInstallUi(moduleInstallUiDelegate,
                R.string.autofill_assistant_module_title, new ModuleInstallUi.FailureUiListener() {
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
