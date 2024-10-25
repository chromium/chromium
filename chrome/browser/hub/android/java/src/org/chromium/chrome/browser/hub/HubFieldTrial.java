// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hub;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.components.cached_flags.BooleanCachedFieldTrialParameter;

/** Common hub feature utils for public use. */
public class HubFieldTrial {
    private static final String ALTERNATIVE_FAB_COLOR_PARAM = "hub_alternative_fab_color";
    public static final BooleanCachedFieldTrialParameter ALTERNATIVE_FAB_COLOR =
            ChromeFeatureList.newBooleanCachedFieldTrialParameter(
                    ChromeFeatureList.ANDROID_HUB_FLOATING_ACTION_BUTTON,
                    ALTERNATIVE_FAB_COLOR_PARAM,
                    false);

    /**
     * Returns whether the primary action on a pane should be shown in a floating action button.
     * When false the button will be in part of the toolbar.
     */
    public static boolean usesFloatActionButton() {
        return ChromeFeatureList.sAndroidHubFloatingActionButton.isEnabled();
    }

    /** Returns whether to use an alternative floating action button color. */
    public static boolean useAlternativeFabColor() {
        return ALTERNATIVE_FAB_COLOR.getValue();
    }
}
