// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.profiles.ProfileManager;
import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * Destroys incognito {@link Profile}s when the last incognito tab is destroyed.
 *
 * Reacts to the presence or absence of incognito tabs.
 */
public class IncognitoProfileDestroyer implements IncognitoTabModelObserver {
    private final TabModelSelector mTabModelSelector;

    /**
     * Creates an {@link IncognitoProfileDestroyer} that reacts to incognito tabs in a
     * given |tabModelSelector|.
     * @param tabModelSelector The {@link TabModelSelector} to observe
     */
    public static void observeTabModelSelector(TabModelSelector tabModelSelector) {
        tabModelSelector.addIncognitoTabModelObserver(
                new IncognitoProfileDestroyer(tabModelSelector));
    }

    IncognitoProfileDestroyer(TabModelSelector tabModelSelector) {
        mTabModelSelector = tabModelSelector;
    }

    @Override
    public void didBecomeEmpty() {
        if (!IncognitoTabHostUtils.doIncognitoTabsExist()
                && !IncognitoTabHostUtils.isIncognitoTabModelActive()) {
            // Only delete the incognito profile if there are no incognito tabs open in any tab
            // model selector as the profile is shared between them.
            Profile profile = mTabModelSelector.getModel(true).getProfile();
            if (profile != null) {
                ProfileManager.destroyWhenAppropriate(profile);
            }
        }
    }
}
