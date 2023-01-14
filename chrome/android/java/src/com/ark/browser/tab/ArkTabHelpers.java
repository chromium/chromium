// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import com.ark.browser.core.utils.TaskTabHelper;

import org.chromium.chrome.browser.autofill_assistant.AutofillAssistantTabHelper;
import org.chromium.chrome.browser.contextualsearch.ContextualSearchTabHelper;
import org.chromium.chrome.browser.crypto.CipherFactory;
import org.chromium.chrome.browser.dom_distiller.ReaderModeManager;
import org.chromium.chrome.browser.dom_distiller.TabDistillabilityProvider;
import org.chromium.chrome.browser.infobar.InfoBarContainer;
import org.chromium.chrome.browser.media.ui.MediaSessionTabHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabFavicon;
import org.chromium.chrome.browser.tab.TrustedCdn;

/**
 * Helper class that initializes various tab UserData objects.
 */
public final class ArkTabHelpers {
    private ArkTabHelpers() {}

    /**
     * Creates Tab helper objects upon Tab creation.
     * @param tab {@link Tab} to create helpers for.
     * @param parentTab {@link Tab} parent tab
     */
    public static void initTabHelpers(Tab tab, Tab parentTab) {
        TabDistillabilityProvider.createForTab(tab);
        ArkInterceptNavigationDelegateTabHelper.createForTab(tab);
        ContextualSearchTabHelper.createForTab(tab);
        MediaSessionTabHelper.createForTab(tab);
        TaskTabHelper.createForTab(tab, parentTab);
        ArkTabBrowserControlsConstraintsHelper.createForTab(tab);
        if (ReaderModeManager.isEnabled()) ReaderModeManager.createForTab(tab);
        AutofillAssistantTabHelper.createForTab(tab);

        // TODO(jinsukkim): Do this by having something observe new tab creation.
        if (tab.isIncognito()) CipherFactory.getInstance().triggerKeyGeneration();
    }

    /**
     * Initializes {@link ArkTabWebContentsUserData} and WebContents-related objects
     * when a new WebContents is set to the tab.
     * @param tab {@link Tab} to create helpers for.
     */
    public static void initWebContentsHelpers(ArkTabImpl tab) {
        // The InfoBarContainer needs to be created after the ContentView has been natively
        // initialized. In the case where restoring a Tab or showing a prerendered one we already
        // have a valid infobar container, no need to recreate one.
        InfoBarContainer.from(tab);

        ArkTabWebContentsObserver.from(tab);
        ArkSwipeRefreshHandler.from(tab);
        TabFavicon.from(tab);
        TrustedCdn.from(tab);
//        TabAssociatedApp.from(tab);
        ArkTabGestureStateListener.from(tab);
    }
}
