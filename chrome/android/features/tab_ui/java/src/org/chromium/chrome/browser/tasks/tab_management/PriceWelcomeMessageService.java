// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;

/**
 * One of the concrete {@link MessageService} that only serves {@link MessageType#PRICE_WELCOME}.
 */
public class PriceWelcomeMessageService extends MessageService {
    /**
     * Provides the binding tab ID and the price drop of the binding tab.
     */
    static class PriceTabData {
        public final int bindingTabId;
        public final ShoppingPersistedTabData.PriceDrop priceDrop;

        PriceTabData(int bindingTabId, ShoppingPersistedTabData.PriceDrop priceDrop) {
            this.bindingTabId = bindingTabId;
            this.priceDrop = priceDrop;
        }

        @Override
        public boolean equals(Object object) {
            if (!(object instanceof PriceTabData)) return false;
            PriceTabData priceTabData = (PriceTabData) object;
            return this.bindingTabId == priceTabData.bindingTabId
                    && this.priceDrop.equals(priceTabData.priceDrop);
        }

        @Override
        public int hashCode() {
            int result = 17;
            result = 31 * result + bindingTabId;
            result = 31 * result + (priceDrop == null ? 0 : priceDrop.hashCode());
            return result;
        }
    }

    /**
     * An interface to help build the PriceWelcomeMessage.
     */
    public interface PriceWelcomeMessageProvider {
        /**
         * This method gets the information of the first tab showing price card.
         *
         * @return The PriceTabData including the tab ID and the price drop.
         */
        PriceTabData getFirstTabShowingPriceCard();

        /**
         * This method gets the tab index from tab ID.
         *
         * @param tabId The tab ID to search for.
         * @return the index within the {@link TabListModel}.
         */
        int getTabIndexFromTabId(int tabId);
    }

    /**
     * An interface to handle the review action of PriceWelcomeMessage.
     */
    public interface PriceWelcomeMessageReviewActionProvider {
        /**
         * This method scrolls to the binding tab of the PriceWelcomeMessage.
         *
         * @param tabIndex The index of the {@link Tab} that is binding to PriceWelcomeMessage.
         */
        void scrollToBindingTab(int tabIndex);
    }

    /**
     * This is the data type that this MessageService is serving to its Observer.
     */
    class PriceWelcomeMessageData implements MessageData {
        private final ShoppingPersistedTabData.PriceDrop mPriceDrop;
        private final MessageCardView.ReviewActionProvider mReviewActionProvider;
        private final MessageCardView.DismissActionProvider mDismissActionProvider;

        PriceWelcomeMessageData(ShoppingPersistedTabData.PriceDrop priceDrop,
                MessageCardView.ReviewActionProvider reviewActionProvider,
                MessageCardView.DismissActionProvider dismissActionProvider) {
            mPriceDrop = priceDrop;
            mReviewActionProvider = reviewActionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        /**
         * @return The {@link MessageCardViewProperties#PRICE_DROP} for the associated
         *         PRICE_WELCOME.
         */
        ShoppingPersistedTabData.PriceDrop getPriceDrop() {
            return mPriceDrop;
        }

        /**
         * @return The {@link MessageCardView.ReviewActionProvider} for the associated
         *         PRICE_WELCOME.
         */
        MessageCardView.ReviewActionProvider getReviewActionProvider() {
            return mReviewActionProvider;
        }

        /**
         * @return The {@link MessageCardView.DismissActionProvider} for the associated
         *         PRICE_WELCOME.
         */
        MessageCardView.DismissActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }
    }

    private static final int MAX_PRICE_WELCOME_MESSAGE_SHOW_COUNT = 10;
    // TODO(crbug.com/1148020): Currently every time entering the tab switcher, {@link
    // ResetHandler.resetWithTabs} will be called twice if {@link
    // TabUiFeatureUtilities#isTabToGtsAnimationEnabled} returns true, see {@link
    // TabSwitcherMediator#prepareOverview}.
    private static final int PREPARE_MESSAGE_TIMES_ENTERING_TAB_SWITCHER =
            TabUiFeatureUtilities.isTabToGtsAnimationEnabled() ? 2 : 1;

    private final PriceWelcomeMessageProvider mPriceWelcomeMessageProvider;
    private final PriceWelcomeMessageReviewActionProvider mPriceWelcomeMessageReviewActionProvider;

    private PriceTabData mPriceTabData;

    PriceWelcomeMessageService(PriceWelcomeMessageProvider priceWelcomeMessageProvider,
            PriceWelcomeMessageReviewActionProvider priceWelcomeMessageReviewActionProvider) {
        super(MessageType.PRICE_WELCOME);
        mPriceTabData = null;
        mPriceWelcomeMessageProvider = priceWelcomeMessageProvider;
        mPriceWelcomeMessageReviewActionProvider = priceWelcomeMessageReviewActionProvider;
    }

    void preparePriceMessage() {
        if (PriceTrackingUtilities.isPriceWelcomeMessageCardDisabled()) return;
        PriceTabData priceTabData = mPriceWelcomeMessageProvider.getFirstTabShowingPriceCard();
        if (priceTabData == null) {
            invalidateMessage();
            return;
        }
        PriceTrackingUtilities.increasePriceWelcomeMessageCardShowCount();
        if (PriceTrackingUtilities.getPriceWelcomeMessageCardShowCount()
                > MAX_PRICE_WELCOME_MESSAGE_SHOW_COUNT
                        * PREPARE_MESSAGE_TIMES_ENTERING_TAB_SWITCHER) {
            invalidateMessage();
            PriceTrackingUtilities.disablePriceWelcomeMessageCard();
        } else if (!priceTabData.equals(mPriceTabData)) {
            mPriceTabData = priceTabData;
            sendInvalidNotification();
            sendAvailabilityNotification(new PriceWelcomeMessageData(
                    mPriceTabData.priceDrop, this::review, (int messageType) -> dismiss()));
        }
    }

    int getBindingTabId() {
        if (mPriceTabData == null) return Tab.INVALID_TAB_ID;
        return mPriceTabData.bindingTabId;
    }

    void invalidateMessage() {
        mPriceTabData = null;
        sendInvalidNotification();
    }

    @VisibleForTesting
    public void review() {
        assert mPriceTabData != null;
        mPriceWelcomeMessageReviewActionProvider.scrollToBindingTab(
                mPriceWelcomeMessageProvider.getTabIndexFromTabId(mPriceTabData.bindingTabId));
        PriceTrackingUtilities.disablePriceWelcomeMessageCard();
    }

    @VisibleForTesting
    public void dismiss() {
        PriceTrackingUtilities.disablePriceWelcomeMessageCard();
    }

    @VisibleForTesting
    PriceTabData getPriceTabDataForTesting() {
        return mPriceTabData;
    }
}
