// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceTabData;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;

/** Controller for the price welcome message in the grid tab switcher. */
@NullMarked
public class PriceWelcomeMessageController {
    /** Used to observe updates to price message cards. */
    public interface PriceMessageUpdateObserver {
        /** Invoked when price message is shown. */
        default void onShowPriceWelcomeMessage() {}

        /** Invoked when price message is removed. */
        default void onRemovePriceWelcomeMessage() {}

        /** Invoked when price message is restored. */
        default void onRestorePriceWelcomeMessage() {}
    }

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void willCloseTab(Tab tab, boolean didCloseAlone) {
                    TabGroupModelFilter tabGroupModelFilter =
                            mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(tabGroupModelFilter);
                    assert mPriceMessageService != null;
                    if (mPriceMessageService.getBindingTabId() == tab.getId()) {
                        removePriceWelcomeMessage();
                    }
                }

                @Override
                public void tabClosureUndone(Tab tab) {
                    TabGroupModelFilter tabGroupModelFilter =
                            mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(tabGroupModelFilter);
                    assert mPriceMessageService != null;
                    if (mPriceMessageService.getBindingTabId() == tab.getId()) {
                        restorePriceWelcomeMessage();
                    }
                }

                @Override
                public void tabClosureCommitted(Tab tab) {
                    // TODO(crbug.com/40160889): Auto update the PriceMessageService instead of
                    // updating it based on the client caller.
                    assert mPriceMessageService != null;
                    if (mPriceMessageService.getBindingTabId() == tab.getId()) {
                        mPriceMessageService.invalidateMessage();
                    }
                }
            };

    private final ObserverList<PriceMessageUpdateObserver> mObservers = new ObserverList<>();
    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);
    private final TabSwitcherMessageManager mTabSwitcherMessageManager;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final MessageCardProviderCoordinator mMessageCardProviderCoordinator;
    private final ObservableSupplierImpl<@Nullable PriceWelcomeMessageReviewActionProvider>
            mPriceWelcomeMessageReviewActionProviderSupplier;
    private final Profile mProfile;
    private final ObservableSupplierImpl<@Nullable TabListCoordinator> mTabListCoordinatorSupplier;
    private final @Nullable PriceMessageService mPriceMessageService;

    @VisibleForTesting
    PriceWelcomeMessageController(
            TabSwitcherMessageManager tabSwitcherMessageManager,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            MessageCardProviderCoordinator<@MessageType Integer, @UiType Integer>
                    messageCardProviderCoordinator,
            ObservableSupplierImpl<@Nullable PriceWelcomeMessageReviewActionProvider>
                    priceWelcomeMessageReviewActionProviderSupplier,
            Profile profile,
            ObservableSupplierImpl<@Nullable TabListCoordinator> tabListCoordinatorSupplier,
            @Nullable PriceMessageService priceMessageService) {
        mTabSwitcherMessageManager = tabSwitcherMessageManager;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mMessageCardProviderCoordinator = messageCardProviderCoordinator;
        mPriceWelcomeMessageReviewActionProviderSupplier =
                priceWelcomeMessageReviewActionProviderSupplier;
        mProfile = profile;
        mTabListCoordinatorSupplier = tabListCoordinatorSupplier;
        mPriceMessageService = priceMessageService;

        if (mPriceMessageService != null) {
            currentTabGroupModelFilterSupplier.addSyncObserverAndCallIfNonNull(
                    mOnTabGroupModelFilterChanged);
            messageCardProviderCoordinator.subscribeMessageService(mPriceMessageService);
        }
    }

    /**
     * @param tabSwitcherMessageManager Manages messages for the tab switcher.
     * @param currentTabGroupModelFilterSupplier Supplies the current {@link TabGroupModelFilter}.
     * @param messageCardProviderCoordinator To build message cards.
     * @param priceWelcomeMessageReviewActionProviderSupplier Supplier for the review action
     *     provider.
     * @param profile The current {@link Profile}.
     * @param tabListCoordinatorSupplier Supplies the {@link TabListCoordinator}.
     */
    public static PriceWelcomeMessageController build(
            TabSwitcherMessageManager tabSwitcherMessageManager,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            MessageCardProviderCoordinator<@MessageType Integer, @UiType Integer>
                    messageCardProviderCoordinator,
            ObservableSupplierImpl<@Nullable PriceWelcomeMessageReviewActionProvider>
                    priceWelcomeMessageReviewActionProviderSupplier,
            Profile profile,
            ObservableSupplierImpl<@Nullable TabListCoordinator> tabListCoordinatorSupplier) {
        PriceMessageService priceMessageService =
                PriceTrackingFeatures.isPriceAnnotationsEnabled(profile)
                        ? new PriceMessageService(
                                profile,
                                tabListCoordinatorSupplier::get,
                                priceWelcomeMessageReviewActionProviderSupplier)
                        : null;

        return new PriceWelcomeMessageController(
                tabSwitcherMessageManager,
                currentTabGroupModelFilterSupplier,
                messageCardProviderCoordinator,
                priceWelcomeMessageReviewActionProviderSupplier,
                profile,
                tabListCoordinatorSupplier,
                priceMessageService);
    }

    /** Destroys the controller. */
    public void destroy() {
        mCurrentTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);
    }

    /**
     * Adds an observer to be notified of price message updates.
     *
     * @param observer The observer to add.
     */
    public void addObserver(PriceMessageUpdateObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * Removes an observer.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(PriceMessageUpdateObserver observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Shows the price welcome message card, if possible.
     *
     * @param priceTabData The data for the tab associated with the price change.
     */
    public void showPriceWelcomeMessage(PriceTabData priceTabData) {
        @Nullable PriceWelcomeMessageReviewActionProvider actionProvider =
                mPriceWelcomeMessageReviewActionProviderSupplier.get();
        if (actionProvider == null) return;

        if (mPriceMessageService == null
                || !PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile)
                || mMessageCardProviderCoordinator.isMessageShown(
                        MessageType.PRICE_MESSAGE,
                        PriceMessageService.PriceMessageType.PRICE_WELCOME)) {
            return;
        }

        if (mPriceMessageService.preparePriceMessage(
                PriceMessageService.PriceMessageType.PRICE_WELCOME, priceTabData)) {
            mTabSwitcherMessageManager.appendNextMessage(MessageType.PRICE_MESSAGE);
            // To make the message card in view when user enters tab switcher, we should scroll to
            // current tab with 0 offset. See {@link
            // TabSwitcherMediator#setInitialScrollIndexOffset} for more details.
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            assumeNonNull(filter);
            actionProvider.scrollToTab(filter.getCurrentRepresentativeTabIndex());
        }
        for (PriceMessageUpdateObserver observer : mObservers) {
            observer.onShowPriceWelcomeMessage();
        }
    }

    /** Removes the price welcome message from the tab grid. */
    public void removePriceWelcomeMessage() {
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator != null) {
            tabListCoordinator.removeSpecialListItem(
                    UiType.PRICE_MESSAGE, MessageType.PRICE_MESSAGE);
        }

        for (PriceMessageUpdateObserver observer : mObservers) {
            observer.onRemovePriceWelcomeMessage();
        }
    }

    /** Restores the price welcome message to the tab grid. */
    public void restorePriceWelcomeMessage() {
        mTabSwitcherMessageManager.appendNextMessage(MessageType.PRICE_MESSAGE);
        for (PriceMessageUpdateObserver observer : mObservers) {
            observer.onRestorePriceWelcomeMessage();
        }
    }

    /** Invalidates the price welcome message. */
    public void invalidate() {
        if (mPriceMessageService != null) {
            // Invalidate price welcome message for every reset so that the stale message won't be
            // restored by mistake (e.g. from tabClosureUndone in TabSwitcherMediator).
            mPriceMessageService.invalidateMessage();
        }
    }

    private void onTabGroupModelFilterChanged(
            @Nullable TabGroupModelFilter newFilter, @Nullable TabGroupModelFilter oldFilter) {
        removeObserver(oldFilter);

        if (newFilter != null) {
            newFilter.addObserver(mTabModelObserver);
        }
    }

    private void removeObserver(@Nullable TabGroupModelFilter oldFilter) {
        if (oldFilter != null) {
            oldFilter.removeObserver(mTabModelObserver);
        }
    }
}
