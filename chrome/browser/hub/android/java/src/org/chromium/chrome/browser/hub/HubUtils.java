// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import android.view.View;

import androidx.annotation.IdRes;

import org.chromium.base.DeviceInfo;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.DeviceFormFactor;

/** Feature related utilities for Hub. */
@NullMarked
public class HubUtils {
    private static @Nullable Boolean sIsTabletForTesting;

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
        if (DeviceInfo.isXr()) return false;

        return true;
    }

    /** Utility to determine which UI variants to show based on device width. */
    public static boolean isScreenWidthTablet(int screenWidthDp) {
        if (sIsTabletForTesting != null) return sIsTabletForTesting;
        return screenWidthDp >= DeviceFormFactor.MINIMUM_TABLET_WIDTH_DP;
    }

    /**
     * Sets the return value for {@link #isScreenWidthTablet(int)} for testing.
     *
     * @param isTablet The value to return. Null to reset.
     */
    public static void setIsTabletForTesting(Boolean isTablet) {
        sIsTabletForTesting = isTablet;
        ResettersForTesting.register(() -> sIsTabletForTesting = null);
    }
}
