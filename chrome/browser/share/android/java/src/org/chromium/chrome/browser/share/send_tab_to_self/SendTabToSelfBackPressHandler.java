// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.send_tab_to_self;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.NonNullObservableSupplier;
import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.base.supplier.ObservableSuppliers;
import org.chromium.base.supplier.SettableNonNullObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tab.TabSelectionType;
import org.chromium.chrome.browser.tab.TabSupplierObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelSelectorTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;

import java.util.List;
import java.util.function.Supplier;

/**
 * Back press handler that intercepts the back gesture when on a received Send Tab To Self tab,
 * switching back to the parent tab without closing the STTS tab.
 *
 * <p>The handler is enabled by {@link #enable(Tab)} when the user opens a received tab, and
 * consumes exactly one back press. It is reset as soon as it consumes a back press, or as soon as
 * either tab it depends on stops being usable.
 *
 * <p>While enabled, {@link #getHandleBackPressChangedSupplier()} must yield {@code true} only while
 * the handler can actually consume the back press. That condition spans two tabs, so enabling
 * installs two observers:
 *
 * <ul>
 *   <li>a {@link TabSupplierObserver} on the activity tab, to reset when the user navigates away
 *       from the received tab, and
 *   <li>a {@link TabModelSelectorTabModelObserver}, to reset when either the enabled received tab
 *       or its parent tab is closed or removed. Without this, closing either tab (possible while
 *       browsing, e.g. from the tablet tab strip) would leave the supplier reporting {@code true}
 *       while {@link #handleBackPress()} could only return {@link BackPressResult#FAILURE}.
 * </ul>
 *
 * <p>To optimize for the majority of users who never receive a shared tab, neither observer is
 * maintained across the browser session; both are attached only when enabled and detached
 * immediately on reset or destruction.
 */
@NullMarked
public final class SendTabToSelfBackPressHandler implements BackPressHandler, Destroyable {
    private final SettableNonNullObservableSupplier<Boolean> mBackPressChangedSupplier =
            ObservableSuppliers.createNonNull(false);
    private final NullableObservableSupplier<Tab> mActivityTabSupplier;
    private final Supplier<@Nullable TabModelSelector> mTabModelSelectorSupplier;

    private int mReceivedTabId = Tab.INVALID_TAB_ID;
    private int mParentTabId = Tab.INVALID_TAB_ID;
    private @Nullable TabSupplierObserver mActivityTabObserver;
    private @Nullable TabModelSelectorTabModelObserver mParentTabObserver;

    /**
     * @param activityTabSupplier Supplies the current activity tab.
     * @param tabModelSelectorSupplier Supplies the TabModelSelector to switch tabs.
     */
    public SendTabToSelfBackPressHandler(
            NullableObservableSupplier<Tab> activityTabSupplier,
            Supplier<@Nullable TabModelSelector> tabModelSelectorSupplier) {
        mActivityTabSupplier = activityTabSupplier;
        mTabModelSelectorSupplier = tabModelSelectorSupplier;
    }

    /**
     * Enables the handler for the given tab if it is an eligible received tab and its parent tab is
     * usable. Any previous state is reset.
     *
     * @param tab The received tab.
     */
    public void enable(Tab tab) {
        // Discard any previous state first, so that an ineligible tab cannot leave the handler
        // enabled for a tab the user is no longer on.
        reset();

        if (!isEligibleReceivedTab(tab)) {
            return;
        }

        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (selector == null) return;

        // Only enable if the parent tab is actually available to switch back to, so that the
        // supplier never claims a back press this handler cannot consume.
        Tab parentTab = selector.getTabById(tab.getParentId());
        if (parentTab == null || !isTabUsable(parentTab)) return;

        mReceivedTabId = tab.getId();
        mParentTabId = tab.getParentId();

        mActivityTabObserver =
                new TabSupplierObserver(mActivityTabSupplier, /* shouldTrigger= */ false) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab newTab) {
                        if (newTab == null || newTab.getId() != mReceivedTabId) {
                            reset();
                        }
                    }

                    @Override
                    public void onDestroyed(Tab destroyedTab) {
                        reset();
                    }
                };

        mParentTabObserver =
                new TabModelSelectorTabModelObserver(selector) {
                    @Override
                    public void willCloseTab(Tab closingTab, boolean didCloseAlone) {
                        resetIfParentOrReceived(closingTab);
                    }

                    @Override
                    public void willCloseTabs(
                            List<Tab> closingTabs, boolean isAllTabs, boolean allowUndo) {
                        if (isAllTabs) {
                            reset();
                            return;
                        }
                        for (int i = 0; i < closingTabs.size(); i++) {
                            resetIfParentOrReceived(closingTabs.get(i));
                        }
                    }

                    @Override
                    public void tabRemoved(Tab removedTab) {
                        resetIfParentOrReceived(removedTab);
                    }
                };

        // Enable only once both observers are installed, so the supplier is never true while the
        // handler is not yet watching everything it depends on.
        mBackPressChangedSupplier.set(true);
    }

    /** Resets if {@code tab} is the parent tab or the received tab this handler tracks. */
    private void resetIfParentOrReceived(Tab tab) {
        if (tab.getId() == mParentTabId || tab.getId() == mReceivedTabId) {
            reset();
        }
    }

    private static boolean isEligibleReceivedTab(Tab tab) {
        return tab.getLaunchType() == TabLaunchType.FROM_SYNC_BACKGROUND
                && tab.getParentId() != Tab.INVALID_TAB_ID
                && tab.getParentId() != tab.getId()
                && isTabUsable(tab);
    }

    private static boolean isTabUsable(Tab tab) {
        return !tab.isClosing() && !tab.isDestroyed();
    }

    private void reset() {
        mReceivedTabId = Tab.INVALID_TAB_ID;
        mParentTabId = Tab.INVALID_TAB_ID;
        mBackPressChangedSupplier.set(false);
        if (mActivityTabObserver != null) {
            mActivityTabObserver.destroy();
            mActivityTabObserver = null;
        }
        if (mParentTabObserver != null) {
            mParentTabObserver.destroy();
            mParentTabObserver = null;
        }
    }

    @Override
    public @BackPressResult int handleBackPress() {
        int receivedTabId = mReceivedTabId;
        int parentTabId = mParentTabId;
        reset();

        // Everything below is defensive. enable() only enables when the received tab and its parent
        // are both usable, and the observers installed there reset as soon as that stops holding,
        // so the supplier should never report true unless this handler can consume the back press.
        if (receivedTabId == Tab.INVALID_TAB_ID) {
            return BackPressResult.FAILURE;
        }

        Tab currentTab = mActivityTabSupplier.get();
        TabModelSelector selector = mTabModelSelectorSupplier.get();
        if (currentTab == null
                || currentTab.getId() != receivedTabId
                || !isTabUsable(currentTab)
                || selector == null) {
            return BackPressResult.FAILURE;
        }

        Tab parentTab = selector.getTabById(parentTabId);
        if (parentTab == null || !isTabUsable(parentTab)) {
            return BackPressResult.FAILURE;
        }

        TabModelUtils.selectTabById(selector, parentTabId, TabSelectionType.FROM_USER);
        return BackPressResult.SUCCESS;
    }

    @Override
    public NonNullObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void destroy() {
        reset();
    }

    int getReceivedTabIdForTesting() {
        return mReceivedTabId;
    }
}
