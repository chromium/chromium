// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.IntDef;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * One of the concrete {@link MessageService} that only serves {@link MessageType#PRICE_MESSAGE}.
 */
public class PriceMessageService extends MessageService {
    private static final String WELCOME_MESSAGE_METRICS_IDENTIFIER = "PriceWelcomeMessageCard";
    private static final String ALERTS_MESSAGE_METRICS_IDENTIFIER = "PriceAlertsMessageCard";

    // PRICE_WELCOME and PRICE_ALERTS are added to {@link TabListModel} at a different time and the
    // insertion positions are different as well. Right now PRICE_WELCOME is added via {@link
    // TabSwitcherCoordinator#appendNextMessage}, while PRICE_ALERTS is added via {@link
    // TabSwitcherCoordinator#appendMessagesTo}.
    @IntDef({PriceMessageType.PRICE_WELCOME, PriceMessageType.PRICE_ALERTS})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PriceMessageType {
        int PRICE_WELCOME = 0;
        int PRICE_ALERTS = 1;
    }

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
         * This method gets the tab index from tab ID.
         *
         * @param tabId The tab ID to search for.
         * @return the index within the {@link TabListModel}.
         */
        int getTabIndexFromTabId(int tabId);

        /**
         * This method updates {@link TabProperties#SHOULD_SHOW_PRICE_DROP_TOOLTIP} of the binding
         * tab.
         *
         * @param index The binding tab index in {@link TabListModel}.
         */
        void showPriceDropTooltip(int index);
    }

    /**
     * An interface to handle the review action of PriceWelcomeMessage.
     */
    public interface PriceWelcomeMessageReviewActionProvider {
        /**
         * This method scrolls to the tab at given index.
         *
         * @param tabIndex The index of the {@link Tab} which we will scroll to.
         */
        void scrollToTab(int tabIndex);
    }

    /**
     * This is the data type that this MessageService is serving to its Observer.
     */
    class PriceMessageData implements MessageData {
        private final int mType;
        private final ShoppingPersistedTabData.PriceDrop mPriceDrop;
        private final MessageCardView.ReviewActionProvider mReviewActionProvider;
        private final MessageCardView.DismissActionProvider mDismissActionProvider;

        PriceMessageData(@PriceMessageType int type, @Nullable PriceTabData priceTabData,
                MessageCardView.ReviewActionProvider reviewActionProvider,
                MessageCardView.DismissActionProvider dismissActionProvider) {
            mType = type;
            mPriceDrop = priceTabData == null ? null : priceTabData.priceDrop;
            mReviewActionProvider = reviewActionProvider;
            mDismissActionProvider = dismissActionProvider;
        }

        /**
         * @return The price message type.
         */
        @PriceMessageType
        int getType() {
            return mType;
        }

        /**
         * @return The {@link MessageCardViewProperties#PRICE_DROP} for the associated
         *         PRICE_MESSAGE.
         */
        ShoppingPersistedTabData.PriceDrop getPriceDrop() {
            return mPriceDrop;
        }

        /**
         * @return The {@link MessageCardView.ReviewActionProvider} for the associated
         *         PRICE_MESSAGE.
         */
        MessageCardView.ReviewActionProvider getReviewActionProvider() {
            return mReviewActionProvider;
        }

        /**
         * @return The {@link MessageCardView.DismissActionProvider} for the associated
         *         PRICE_MESSAGE.
         */
        MessageCardView.DismissActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }
    }

    private static final int MAX_PRICE_MESSAGE_SHOW_COUNT = 10;
    // TODO(crbug.com/1148020): Currently every time entering the tab switcher, {@link
    // ResetHandler.resetWithTabs} will be called twice if {@link
    // TabUiFeatureUtilities#isTabToGtsAnimationEnabled} returns true, see {@link
    // TabSwitcherMediator#prepareOverview}.
    private static final int PREPARE_MESSAGE_TIMES_ENTERING_TAB_SWITCHER =
            TabUiFeatureUtilities.isTabToGtsAnimationEnabled(ContextUtils.getApplicationContext())
            ? 2
            : 1;

    private final PriceWelcomeMessageProvider mPriceWelcomeMessageProvider;
    private final PriceWelcomeMessageReviewActionProvider mPriceWelcomeMessageReviewActionProvider;
    private final PriceDropNotificationManager mNotificationManager;

    private PriceTabData mPriceTabData;

    PriceMessageService(PriceWelcomeMessageProvider priceWelcomeMessageProvider,
            PriceWelcomeMessageReviewActionProvider priceWelcomeMessageReviewActionProvider,
            PriceDropNotificationManager notificationManager) {
        super(MessageType.PRICE_MESSAGE);
        mPriceTabData = null;
        mPriceWelcomeMessageProvider = priceWelcomeMessageProvider;
        mPriceWelcomeMessageReviewActionProvider = priceWelcomeMessageReviewActionProvider;
        mNotificationManager = notificationManager;
    }

    /**
     * @return Whether the message is successfully prepared.
     */
    boolean preparePriceMessage(@PriceMessageType int type, @Nullable PriceTabData priceTabData) {
        assert (type == PriceMessageType.PRICE_WELCOME
                && PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled())
                || (type == PriceMessageType.PRICE_ALERTS
                        && PriceTrackingUtilities.isPriceAlertsMessageCardEnabled());
        if (type == PriceMessageType.PRICE_WELCOME) {
            PriceTrackingUtilities.increasePriceWelcomeMessageCardShowCount();
            if (PriceTrackingUtilities.getPriceWelcomeMessageCardShowCount()
                    > MAX_PRICE_MESSAGE_SHOW_COUNT * PREPARE_MESSAGE_TIMES_ENTERING_TAB_SWITCHER) {
                logMessageDisableMetrics(
                        WELCOME_MESSAGE_METRICS_IDENTIFIER, MessageDisableReason.MESSAGE_IGNORED);
                PriceTrackingUtilities.disablePriceWelcomeMessageCard();
                return false;
            }
            // When PriceWelcomeMessageCard is available, it takes priority over
            // PriceAlertsMessageCard which will be removed first. This should be called only if
            // PriceAlertsMessageCard is currently enabled.
            if (PriceTrackingUtilities.isPriceAlertsMessageCardEnabled()) {
                PriceTrackingUtilities.decreasePriceAlertsMessageCardShowCount();
            }
        } else if (type == PriceMessageType.PRICE_ALERTS) {
            PriceTrackingUtilities.increasePriceAlertsMessageCardShowCount();
            if (PriceTrackingUtilities.getPriceAlertsMessageCardShowCount()
                    > MAX_PRICE_MESSAGE_SHOW_COUNT * PREPARE_MESSAGE_TIMES_ENTERING_TAB_SWITCHER) {
                logMessageDisableMetrics(
                        ALERTS_MESSAGE_METRICS_IDENTIFIER, MessageDisableReason.MESSAGE_IGNORED);
                PriceTrackingUtilities.disablePriceAlertsMessageCard();
                return false;
            }
        }
        // To avoid the confusion of different-type stale messages, invalidateMessage every time
        // before preparing new messages.
        invalidateMessage();
        mPriceTabData = priceTabData;
        sendAvailabilityNotification(new PriceMessageData(
                type, mPriceTabData, () -> review(type), (int messageType) -> dismiss(type)));
        return true;
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
    public void review(@PriceMessageType int type) {
        if (type == PriceMessageType.PRICE_WELCOME) {
            assert mPriceTabData != null;
            int bindingTabIndex =
                    mPriceWelcomeMessageProvider.getTabIndexFromTabId(mPriceTabData.bindingTabId);
            mPriceWelcomeMessageReviewActionProvider.scrollToTab(bindingTabIndex);
            mPriceWelcomeMessageProvider.showPriceDropTooltip(bindingTabIndex);
            logMessageDisableMetrics(
                    WELCOME_MESSAGE_METRICS_IDENTIFIER, MessageDisableReason.MESSAGE_ACCEPTED);
            PriceTrackingUtilities.disablePriceWelcomeMessageCard();
            mPriceTabData = null;
            RecordUserAction.record("Commerce.PriceWelcomeMessageCard.Reviewed");
        } else if (type == PriceMessageType.PRICE_ALERTS) {
            if (mNotificationManager.areAppNotificationsEnabled()) {
                mNotificationManager.createNotificationChannel();
            } else {
                mNotificationManager.launchNotificationSettings();
            }
            logMessageDisableMetrics(
                    ALERTS_MESSAGE_METRICS_IDENTIFIER, MessageDisableReason.MESSAGE_ACCEPTED);
            PriceTrackingUtilities.disablePriceAlertsMessageCard();
            RecordUserAction.record("Commerce.PriceAlertsMessageCard.Reviewed");
        }
    }

    @VisibleForTesting
    public void dismiss(@PriceMessageType int type) {
        if (type == PriceMessageType.PRICE_WELCOME) {
            logMessageDisableMetrics(
                    WELCOME_MESSAGE_METRICS_IDENTIFIER, MessageDisableReason.MESSAGE_DISMISSED);
            PriceTrackingUtilities.disablePriceWelcomeMessageCard();
            mPriceTabData = null;
            RecordUserAction.record("Commerce.PriceWelcomeMessageCard.Dismissed");
        } else if (type == PriceMessageType.PRICE_ALERTS) {
            logMessageDisableMetrics(
                    ALERTS_MESSAGE_METRICS_IDENTIFIER, MessageDisableReason.MESSAGE_DISMISSED);
            PriceTrackingUtilities.disablePriceAlertsMessageCard();
            RecordUserAction.record("Commerce.PriceAlertsMessageCard.Dismissed");
        }
    }

    @VisibleForTesting
    PriceTabData getPriceTabDataForTesting() {
        return mPriceTabData;
    }
}
