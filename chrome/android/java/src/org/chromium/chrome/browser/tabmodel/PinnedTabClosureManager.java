// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

import android.content.Context;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.ui.widget.Toast;

import java.util.HashMap;
import java.util.Objects;

/** Manages the confirmation flow for closing pinned tabs to prevent accidental closure. */
@NullMarked
public class PinnedTabClosureManager {
    private static final long CONFIRM_TIMEOUT_MS = 4000;

    // Maps the selector to the ID of the pinned tab currently awaiting confirmation.
    private final HashMap<TabModelSelector, Integer> mPendingPinnedTabs = new HashMap<>();
    private @Nullable Toast mCurrentToast;

    private PinnedTabClosureManager() {}

    private static class PinnedTabClosureManagerHolder {
        // The instance is created when this inner class is loaded by the JVM.
        private static final PinnedTabClosureManager INSTANCE = new PinnedTabClosureManager();
    }

    /**
     * Returns the singleton instance of {@link PinnedTabClosureManager}, creating it if necessary.
     * Must be called on the UI thread.
     */
    static PinnedTabClosureManager getInstance() {
        return PinnedTabClosureManagerHolder.INSTANCE;
    }

    /**
     * Determines whether a tab is ready to be closed.
     *
     * <p>For pinned tabs, this method implements a double-confirmation flow to prevent accidental
     * closure: the first call records the tab as "pending," shows a warning toast, and returns
     * false. If called again for the same tab within the timeout period, it returns true. Unpinned
     * tabs always return true.
     *
     * @param selector The {@link TabModelSelector} managing the tab.
     * @param tab The tab the user is attempting to close.
     * @param isBulkClose Whether is closing multiple tabs.
     * @return True if the tab should be closed immediately; false if the closure is intercepted for
     *     confirmation.
     */
    public boolean shouldCloseTab(TabModelSelector selector, Tab tab, boolean isBulkClose) {
        if (!tab.getIsPinned() || isBulkClose) {
            clearPendingState(selector);
            return true;
        }

        int pendingTabId = mPendingPinnedTabs.getOrDefault(selector, Tab.INVALID_TAB_ID);
        int tabId = tab.getId();

        // Confirm the closure on the second attempt.
        if (pendingTabId == tabId) {
            clearPendingState(selector);
            return true;
        }

        // Pending closure on the first attempt.
        mPendingPinnedTabs.put(selector, tabId);
        showToast(tab.getContext());

        // Clears the pending state after 4 seconds to match desktop behavior.
        PostTask.postDelayedTask(
                TaskTraits.UI_DEFAULT,
                () -> {
                    if (Objects.equals(mPendingPinnedTabs.get(selector), tabId)) {
                        clearPendingState(selector);
                    }
                },
                CONFIRM_TIMEOUT_MS);

        return false;
    }

    @VisibleForTesting
    public void clearPendingState(TabModelSelector selector) {
        mPendingPinnedTabs.remove(selector);
        if (mCurrentToast != null) {
            mCurrentToast.cancel();
            mCurrentToast = null;
        }
    }

    @VisibleForTesting
    public void showToast(Context context) {
        if (mCurrentToast != null) mCurrentToast.cancel();
        mCurrentToast =
                Toast.makeText(
                        context, R.string.keyboard_shortcut_close_pinned_tab, Toast.LENGTH_LONG);
        mCurrentToast.show();
    }

    HashMap<TabModelSelector, Integer> getPendingPinnedTabsForTesting() {
        return mPendingPinnedTabs; // IN-TEST
    }
}
