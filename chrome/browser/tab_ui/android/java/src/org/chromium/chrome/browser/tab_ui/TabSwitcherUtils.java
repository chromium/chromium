// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import org.chromium.chrome.browser.layouts.LayoutManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;

/** Utility methods for TabSwitcher related actions. */
public class TabSwitcherUtils {
    /**
     * A method to navigate to tab switcher.
     *
     * @param layoutManager A {@link LayoutManagerChrome} used to watch for scene changes.
     * @param animate Whether the transition should be animated if the layout supports it.
     * @param onNavigationFinished Runnable to run after navigation to TabSwitcher is finished.
     */
    public static void navigateToTabSwitcher(
            LayoutManager layoutManager, boolean animate, Runnable onNavigationFinished) {
        if (layoutManager.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            onNavigationFinished.run();
            return;
        }

        layoutManager.addObserver(
                new LayoutStateObserver() {
                    @Override
                    public void onFinishedShowing(int layoutType) {
                        if (layoutType == LayoutType.TAB_SWITCHER) {
                            layoutManager.removeObserver(this);
                            onNavigationFinished.run();
                        }
                    }
                });

        layoutManager.showLayout(LayoutType.TAB_SWITCHER, animate);
    }
}
