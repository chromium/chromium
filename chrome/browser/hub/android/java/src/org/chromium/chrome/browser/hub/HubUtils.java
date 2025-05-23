// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import androidx.annotation.IdRes;

import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.theme.ThemeModuleUtils;
import org.chromium.ui.util.XrUtils;

/** Feature related utilities for Hub. */
@NullMarked
public class HubUtils {
    /**
     * Returns the height of the hub search box's calculated container.
     *
     * @param containerView The {@link HubContainerView} that hosts the search box.
     * @param hubToolbar The resource id for the hub toolbar.
     * @param toolbarActionContainer The resource id for the toolbar action container.
     */
    public static int getSearchBoxHeight(
            HubContainerView containerView,
            @IdRes int hubToolbar,
            @IdRes int toolbarActionContainer) {
        View hubToolbarView = containerView.findViewById(hubToolbar);
        View searchBoxContainerView = containerView.findViewById(toolbarActionContainer);
        if (hubToolbarView == null || searchBoxContainerView == null) return 0;
        if (!hubToolbarView.isLaidOut() || !searchBoxContainerView.isLaidOut()) return 0;

        int hubToolbarBottom = hubToolbarView.getBottom();
        int searchBoxContainerBottom = searchBoxContainerView.getBottom();
        return hubToolbarBottom - searchBoxContainerBottom;
    }

    /** Whether enable the grid tab switcher UI update. */
    public static boolean isGtsUpdateEnabled() {
        // TODO(crbug.com/419822825): Remove explicit check once XR toolbar crash is resolved.
        if (XrUtils.isXrDevice()) return false;

        return ChromeFeatureList.sGridTabSwitcherUpdate.isEnabled()
                || ThemeModuleUtils.isForceEnableDependencies();
    }
}
