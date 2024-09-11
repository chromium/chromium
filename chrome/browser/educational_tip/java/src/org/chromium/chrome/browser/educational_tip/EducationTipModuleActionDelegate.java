// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.educational_tip;

import android.content.Context;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.hub.PaneId;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;

/** The delegate to provide actions for the educational tips module. */
public interface EducationTipModuleActionDelegate {

    /** Gets the application context. */
    @NonNull
    Context getContext();

    /** Gets the instance of {@link BottomSheetController}. */
    @NonNull
    BottomSheetController getBottomSheetController();

    /**
     * Opens the Hub layout and displays a specific pane.
     *
     * @param paneId The id of the pane to show.
     */
    void openHubPane(@PaneId int paneId);

    /** Opens the grid tab switcher and show the tab grip Iph dialog. */
    void openTabGroupIphDialog();

    /** Opens the app menu and highlights the quick delete menu item. */
    void openAndHighlightQuickDeleteMenuItem();
}
