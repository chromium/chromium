// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_ui;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.tab.Tab;

import java.util.List;

/** Interface for the Tab Switcher. */
public interface TabSwitcher {

    /** Called when native initialization is completed. */
    void initWithNative();

    /** Returns a {@link Supplier} that provides dialog visibility. */
    Supplier<Boolean> getTabGridDialogVisibilitySupplier();

    /**
     * Returns a {@link TabSwitcherCustomViewManager} that allows to pass custom views to {@link
     * TabSwitcherCoordinator}.
     */
    @Nullable
    TabSwitcherCustomViewManager getTabSwitcherCustomViewManager();

    /** Returns the number of elements in the tab switcher's tab list model. */
    int getTabSwitcherTabListModelSize();

    /**
     * Set the tab switcher's current RecyclerViewPosition. This is a no-op for tab switcher without
     * a recyclerView.
     */
    void setTabSwitcherRecyclerViewPosition(RecyclerViewPosition recyclerViewPosition);

    /**
     * Show the Quick Delete animation on the tab list.
     *
     * @param onAnimationEnd Runnable that is invoked when the animation is completed.
     * @param tabs The tabs to fade with the animation. These tabs will get closed after the
     *     animation is complete.
     */
    void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs);

    /**
     * Open the invitation modal on top of the tab switcher view when an invitation intent is
     * intercepted.
     *
     * @param invitationId The id of the invitation.
     */
    void openInvitationModal(String invitationId);

    /**
     * Requests to show a dialog for a tab group.
     *
     * @param tabId The id of any tab in the group.
     * @return Whether the request to show was able to be handled.
     */
    boolean requestOpenTabGroupDialog(int tabId);
}
