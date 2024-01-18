// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_resumption;

import android.view.ViewGroup;

import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.url.GURL;

/** Utilities for the tab resumption module. */
public class TabResumptionModuleUtils {
    /** Callback to handle click on suggestion tiles. */
    public interface SuggestionClickCallback {
        void onSuggestionClick(GURL gurl);
    }

    /** Returns whether to show the tab resumption module. */
    static boolean shouldShowTabResumptionModule(Profile profile) {
        // TODO(crbug.com/1515325): Check user is signed in with sync enabled.
        return ChromeFeatureList.sTabResumptionModuleAndroid.isEnabled();
    }

    /**
     * Creates a {@link TabResumptionModuleCoordinator} if allowed to.
     *
     * @param parent The parent layout which the tab resumption module lives.
     * @param suggestionClickCallback The function to call when a suggestion tile is clicked.
     * @param profile The profile of the user.
     * @param moduleContainerStubId The id of the tab resumption module on its parent view.
     */
    public static TabResumptionModuleCoordinator mayCreateTabResumptionModuleCoordinator(
            ViewGroup parent,
            SuggestionClickCallback suggestionClickCallback,
            Profile profile,
            int moduleContainerStubId) {
        if (!shouldShowTabResumptionModule(profile)) return null;

        return new TabResumptionModuleCoordinator(parent, moduleContainerStubId);
    }
}
