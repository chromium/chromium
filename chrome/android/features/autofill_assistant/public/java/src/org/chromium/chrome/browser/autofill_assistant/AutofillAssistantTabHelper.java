// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.autofill_assistant;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabUtils;
import org.chromium.components.autofill_assistant.Starter;

/**
 * Instantiates a tab helper for autofill-assistant.
 */
public class AutofillAssistantTabHelper {
    private static final Class<Starter> USER_DATA_KEY = Starter.class;

    /**
     * Creates an autofill-assistant starter for the given tab. The starter will attach itself to
     * the tab as observer and connect to its native counterpart in order to fulfill startup
     * requests from either side.
     */
    public static void createForTab(Tab tab) {
        Starter starter = new Starter(()
                                              -> TabUtils.getActivity(tab),
                tab.getWebContents(), new AssistantStaticDependenciesChrome(),
                AssistantDependencyUtilsChrome::isGsa,
                new AssistantModuleInstallUiProviderChrome(tab));
        AssistantDependencyUtilsChrome.attachTabObserver(tab, starter);
        tab.getUserDataHost().setUserData(USER_DATA_KEY, starter);
    }

    public static Starter get(Tab tab) {
        return tab.getUserDataHost().getUserData(USER_DATA_KEY);
    }
}
