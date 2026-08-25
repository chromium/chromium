// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import org.chromium.base.ThreadUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.tabwindow.TabWindowManagerSingleton;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.TabWindowManager;
import org.chromium.components.webapps.AppBannerManager;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Predicate;

/** Utility methods for interacting with tabs in the context of webapps. */
@NullMarked
public class WebappTabUtils {
    /**
     * Rechecks installability for all active tabs that match the given predicate.
     *
     * @param matcher The predicate to decide which tabs to recheck.
     */
    public static void recheckInstallabilityForMatchingTabs(Predicate<Tab> matcher) {
        ThreadUtils.assertOnUiThread();
        List<TabModelSelector> selectors = new ArrayList<>();
        TabWindowManager tabWindowManager = TabWindowManagerSingleton.getInstance();
        if (tabWindowManager == null) return;
        selectors.addAll(tabWindowManager.getAllTabModelSelectors());
        selectors.addAll(tabWindowManager.getCustomTabsTabModelSelectors());

        for (TabModelSelector selector : selectors) {
            for (TabModel model : selector.getModels()) {
                for (int i = 0; i < model.getCount(); ++i) {
                    Tab tab = model.getTabAt(i);
                    if (tab == null) continue;
                    WebContents webContents = tab.getWebContents();
                    if (webContents == null || !matcher.test(tab)) continue;
                    AppBannerManager manager = AppBannerManager.forWebContents(webContents);
                    if (manager == null) continue;
                    manager.recheckInstallability();
                }
            }
        }
    }
}
