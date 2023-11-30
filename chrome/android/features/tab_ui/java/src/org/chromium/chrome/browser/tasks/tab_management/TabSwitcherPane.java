// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.View.OnClickListener;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.hub.DrawableButtonData;
import org.chromium.chrome.browser.hub.LoadHint;
import org.chromium.chrome.browser.hub.Pane;
import org.chromium.chrome.browser.hub.PaneId;

/**
 * A {@link Pane} representing the tab switcher. This is effectively an adapter layer between the
 * {@link Pane} and {@link TabSwitcher} APIs.
 */
public class TabSwitcherPane extends TabSwitcherPaneBase {
    private final TabSwitcherPaneDrawableCoordinator mTabSwitcherPaneDrawableCoordinator;

    /**
     * @param tabSwitcher The {@link TabSwitcher} hosted by the Pane.
     * @param newTabButtonClickListener The {@link OnClickListener} for the new tab button.
     * @param tabSwitcherPaneDrawableCoordinator The drawable to represent the pane.
     */
    TabSwitcherPane(
            @NonNull TabSwitcher tabSwitcher,
            @NonNull OnClickListener newTabButtonClickListener,
            @NonNull TabSwitcherPaneDrawableCoordinator tabSwitcherDrawableCoordinator) {
        super(
                tabSwitcher,
                newTabButtonClickListener,
                org.chromium.chrome.browser.toolbar.R.string.button_new_tab);
        mTabSwitcherPaneDrawableCoordinator = tabSwitcherDrawableCoordinator;

        // TODO(crbug/1505772): Update this string to not be an a11y string and it should probably
        // just say "Tabs".
        mReferenceButtonDataSupplier.set(
                new DrawableButtonData(
                        org.chromium.chrome.tab_ui.R.string.accessibility_tab_switcher,
                        org.chromium.chrome.tab_ui.R.string.accessibility_tab_switcher,
                        tabSwitcherDrawableCoordinator.getTabSwitcherDrawable()));
    }

    @Override
    public @PaneId int getPaneId() {
        return PaneId.TAB_SWITCHER;
    }

    @Override
    public void notifyLoadHint(@LoadHint int loadHint) {
        // TODO(crbug/1505772): Implement.
    }
}
