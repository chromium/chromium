// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.view.ViewGroup;

/** Holds dependencies for constructing a {@link TabSwitcherPane}. */
public class TabSwitcherPaneCoordinatorFactory {
    TabSwitcherPaneCoordinatorFactory() {}

    /**
     * Creates a {@link TabSwitcherPaneCoordinator} using a combination of the held dependencies and
     * those supplied through this method.
     *
     * @param parentView The view to use as a parent.
     * @return a {@link TabSwitcherPaneCoordinator} to use.
     */
    TabSwitcherPaneCoordinator create(ViewGroup parentView) {
        return new TabSwitcherPaneCoordinator(parentView);
    }
}
