// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.recent_tabs;

import org.chromium.chrome.browser.recent_tabs.ForeignSessionHelper.ForeignSession;

import java.util.List;

/** An interface of methods that perform actions related to the restore tabs promo. */
public interface RestoreTabsControllerDelegate {
    /**
     * Action to perform when the restore tabs promo should be shown.
     *
     * @param sessions The list of synced foreign sessions for the current profile.
     */
    public void showPromo(List<ForeignSession> sessions);

    /** Action to perform when the restore tabs promo is done showing. */
    public void onDismissed();

    /** Get the tab switcher's current tab list model size. */
    public int getGTSTabListModelSize();

    /**
     * Action to perform at the end of the workflow after the user has restored their chosen tabs.
     */
    public void scrollGTSToRestoredTabs(int tabListModelSize);
}
