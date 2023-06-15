// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.flags.BooleanCachedFieldTrialParameter;
import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;

import java.util.List;

/**
 * An interface of methods that perform actions related to the restore tabs promo.
 */
public interface RestoreTabsControllerDelegate {
    /**
     * Action to perform when the restore tabs promo should be shown.
     *
     * @param foreignSessionHelper The active ForeignSessionHelper instance.
     * @param sessions The list of synced foreign sessions for the current profile.
     */
    public void showPromo(ForeignSessionHelper foreignSessionHelper, List<ForeignSession> sessions);

    /**
     * Action to perform when the restore tabs promo is done showing.
     *
     * @param wasPromoShown The boolean to notify the callback if the promo was shown.
     */
    public void onDismissed(boolean wasPromoShown);

    /**
     * Helper method to retrieve the param value stored for skipping the feature engagement check.
     */
    public BooleanCachedFieldTrialParameter getSkipFeatureEngagementParam();
}
