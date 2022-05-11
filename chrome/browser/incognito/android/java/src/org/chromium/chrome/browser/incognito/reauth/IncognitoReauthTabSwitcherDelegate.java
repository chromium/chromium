// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito.reauth;

import android.view.View;

import androidx.annotation.NonNull;

/**
 * An interface to establish communication with {@link TabSwitcherCustomViewManager} to support
 * showing re-auth screen in tab switcher UI.
 *
 * TODO(crbug.com/1324211, crbug.com/1227656) : Remove this interface once tab_ui is modularized.
 */
public interface IncognitoReauthTabSwitcherDelegate {
    /**
     * A method to supply the incognito re-auth view to tab switcher.
     *
     * @param customView A {@link View} that needs to be added to the tab switcher content area.
     * @return True, if the signal was relayed successfully to {@link TabSwitcherCustomViewManager},
     *         false otherwise.
     */
    boolean addReauthScreenInTabSwitcher(@NonNull View customView);

    /**
     * A method to remove the incognito re-auth view from tab switcher.
     *
     * @return True, if the signal was relayed successfully to {@link TabSwitcherCustomViewManager},
     *         false otherwise.
     */
    boolean removeReauthScreenFromTabSwitcher();
}
