// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.state.ShoppingPersistedTabData.PriceDrop;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardView.ServiceDismissActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Objects;
import java.util.function.Supplier;

/**
 * One of the concrete {@link MessageService} that only serves {@link MessageType#PRICE_MESSAGE}.
 */
@NullMarked
public class PriceMessageService extends MessageService<@MessageType Integer, @UiType Integer> {
    private static final String WELCOME_MESSAGE_METRICS_IDENTIFIER = "PriceWelcomeMessageCard";

    // PRICE_WELCOME and PRICE_ALERTS are added to {@link TabListModel} at a different time and the
    // insertion positions are different as well. Right now PRICE_WELCOME is added via {@link
    // TabSwitcherCoordinator#appendNextMessage}, while PRICE_ALERTS is added via {@link
    // TabSwitcherCoordinator#appendMessagesTo}.
    @IntDef({PriceMessageType.PRICE_WELCOME})
    @Retention(RetentionPolicy.SOURCE)
    public @interface PriceMessageType {
        int PRICE_WELCOME = 0;
    }

    /**
     * The reason why we disable the message in grid tab switcher and no longer show it.
     *
     * <p>Needs to stay in sync with GridTabSwitcherMessageDisableReason in enums.xml. These values
     * are persisted to logs. Entries should not be renumbered and numeric values should never be
     * reused.
     */
    @IntDef({
        MessageDisableReason.UNKNOWN,
        MessageDisableReason.MESSAGE_ACCEPTED,
        MessageDisableReason.MESSAGE_DISMISSED,
        MessageDisableReason.MESSAGE_IGNORED
    })
    @Retention(RetentionPolicy.SOURCE)
    public @interface MessageDisableReason {
        int UNKNOWN = 0;
        int MESSAGE_ACCEPTED = 1;
        int MESSAGE_DISMISSED = 2;
        int MESSAGE_IGNORED = 3;
        int MAX_VALUE = 3;
    }

    /** Provides the binding tab ID and the price drop of the binding tab. */
    static class PriceTabData {
        public final int bindingTabId;
        public final PriceDrop priceDrop;

        PriceTabData(int bindingTabId, PriceDrop priceDrop) {
            this.bindingTabId = bindingTabId;
            this.priceDrop = priceDrop;
        }

        @Override
        public boolean equals(Object obj) {
            return (obj instanceof PriceTabData other)
                    && bindingTabId == other.bindingTabId
                    && Objects.equals(priceDrop, other.priceDrop);
        }

        @Override
        public int hashCode() {
            return Objects.hash(bindingTabId, priceDrop);
        }
    }

    /** An interface to help build the PriceWelcomeMessage. */
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

    /** An interface to handle the review action of PriceWelcomeMessage. */
    public interface PriceWelcomeMessageReviewActionProvider {
        /**
         * This method scrolls to the tab at given index.
         *
         * @param tabIndex The index of the {@link Tab} which we will scroll to.
         */
        void scrollToTab(int tabIndex);
    }

    /** This is the data type that this MessageService is serving to its Observer. */
    static class PriceMessageData {
        private final int mType;
        private final @Nullable PriceDrop mPriceDrop;
        private final MessageCardView.ActionProvider mAcceptActionProvider;
        private final MessageCardView.ActionProvider mDismissActionProvider;

        PriceMessageData(
                @PriceMessageType int type,
                @Nullable PriceTabData priceTabData,
                MessageCardView.ActionProvider acceptActionProvider,
                MessageCardView.ActionProvider dismissActionProvider) {
            mType = type;
            mPriceDrop = priceTabData == null ? null : priceTabData.priceDrop;
            mAcceptActionProvider = acceptActionProvider;
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
         *     PRICE_MESSAGE.
         */
        @Nullable PriceDrop getPriceDrop() {
            return mPriceDrop;
        }

        /**
         * @return The {@link MessageCardView.ActionProvider} for the associated PRICE_MESSAGE.
         */
        MessageCardView.ActionProvider getAcceptActionProvider() {
            return mAcceptActionProvider;
        }

        /**
         * @return The {@link ServiceDismissActionProvider} for the associated PRICE_MESSAGE.
         */
        MessageCardView.ActionProvider getDismissActionProvider() {
            return mDismissActionProvider;
        }
    }

    private static final int MAX_PRICE_MESSAGE_SHOW_COUNT = 10;

    private final Profile mProfile;
    private final Supplier<@Nullable PriceWelcomeMessageProvider>
            mPriceWelcomeMessageProviderSupplier;
    private final Supplier<@Nullable PriceWelcomeMessageReviewActionProvider>
            mPriceWelcomeMessageReviewActionProviderSupplier;

    private @Nullable PriceTabData mPriceTabData;

    PriceMessageService(
            Profile profile,
            Supplier<@Nullable PriceWelcomeMessageProvider> priceWelcomeMessageProviderSupplier,
            Supplier<@Nullable PriceWelcomeMessageReviewActionProvider>
                    priceWelcomeMessageReviewActionProviderSupplier) {
        super(
                MessageType.PRICE_MESSAGE,
                UiType.PRICE_MESSAGE,
                R.layout.large_message_card_item,
                LargeMessageCardViewBinder::bind);
        mProfile = profile;
        mPriceTabData = null;
        mPriceWelcomeMessageProviderSupplier = priceWelcomeMessageProviderSupplier;
        mPriceWelcomeMessageReviewActionProviderSupplier =
                priceWelcomeMessageReviewActionProviderSupplier;
    }

    /**
     * @return Whether the message is successfully prepared.
     */
    boolean preparePriceMessage(@PriceMessageType int type, @Nullable PriceTabData priceTabData) {
        assert (type == PriceMessageType.PRICE_WELCOME
                && PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile));
        PriceTrackingUtilities.increasePriceWelcomeMessageCardShowCount();
        if (PriceTrackingUtilities.getPriceWelcomeMessageCardShowCount()
                > MAX_PRICE_MESSAGE_SHOW_COUNT) {
            logMessageDisableMetrics(MessageDisableReason.MESSAGE_IGNORED);
            PriceTrackingUtilities.disablePriceWelcomeMessageCard();
            return false;
        }
        // To avoid the confusion of different-type stale messages, invalidateMessage every time
        // before preparing new messages.
        invalidateMessage();
        mPriceTabData = priceTabData;
        sendAvailabilityNotification((a, b) -> buildModel(type, a, b));
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

    private PropertyModel buildModel(
            @PriceMessageType int type,
            Context context,
            ServiceDismissActionProvider<@MessageType Integer> serviceActionProvider) {
        return PriceMessageCardViewModel.create(
                context,
                serviceActionProvider,
                new PriceMessageData(type, mPriceTabData, () -> review(type), this::dismiss),
                PriceDropNotificationManagerFactory.create(mProfile));
    }

    @VisibleForTesting
    public void review(@PriceMessageType int type) {
        if (type == PriceMessageType.PRICE_WELCOME) {
            assert mPriceTabData != null;
            PriceWelcomeMessageProvider priceWelcomeMessageProvider =
                    mPriceWelcomeMessageProviderSupplier.get();
            assert priceWelcomeMessageProvider != null;
            int bindingTabIndex =
                    priceWelcomeMessageProvider.getTabIndexFromTabId(mPriceTabData.bindingTabId);

            PriceWelcomeMessageReviewActionProvider priceWelcomeMessageReviewActionProvider =
                    mPriceWelcomeMessageReviewActionProviderSupplier.get();
            assert priceWelcomeMessageReviewActionProvider != null;
            priceWelcomeMessageReviewActionProvider.scrollToTab(bindingTabIndex);
            priceWelcomeMessageProvider.showPriceDropTooltip(bindingTabIndex);
            logMessageDisableMetrics(MessageDisableReason.MESSAGE_ACCEPTED);
            PriceTrackingUtilities.disablePriceWelcomeMessageCard();
            mPriceTabData = null;
            RecordUserAction.record("Commerce.PriceWelcomeMessageCard.Reviewed");
        }
    }

    @VisibleForTesting
    public void dismiss() {
        logMessageDisableMetrics(MessageDisableReason.MESSAGE_DISMISSED);
        PriceTrackingUtilities.disablePriceWelcomeMessageCard();
        mPriceTabData = null;
        RecordUserAction.record("Commerce.PriceWelcomeMessageCard.Dismissed");
    }

    private void logMessageDisableMetrics(@MessageDisableReason int reason) {
        RecordHistogram.recordEnumeratedHistogram(
                String.format(
                        "GridTabSwitcher.%s.DisableReason",
                        PriceMessageService.WELCOME_MESSAGE_METRICS_IDENTIFIER),
                reason,
                MessageDisableReason.MAX_VALUE);
    }

    @Nullable PriceTabData getPriceTabDataForTesting() {
        return mPriceTabData;
    }
}
