// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.ObserverList;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.LazyOneshotSupplier;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManagerFactory;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListEditorCoordinator.TabListEditorController;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Manages message related glue for the {@link TabSwitcher}. */
public class TabSwitcherMessageManager implements PriceWelcomeMessageController {
    /** Used to observe updates to message cards. */
    public interface MessageUpdateObserver {
        /** Invoked when messages are added. */
        default void onAppendedMessage() {}

        /** Invoked when messages are removed. */
        default void onRemoveAllAppendedMessage() {}

        /** Invoked when messages are restored. */
        default void onRestoreAllAppendedMessage() {}

        /** Invoked when price message is shown. */
        default void onShowPriceWelcomeMessage() {}

        /** Invoked when price message is removed. */
        default void onRemovePriceWelcomeMessage() {}

        /** Invoked when price message is restored. */
        default void onRestorePriceWelcomeMessage() {}
    }

    private final MultiWindowModeStateDispatcher.MultiWindowModeObserver mMultiWindowModeObserver =
            isInMultiWindowMode -> {
                if (isInMultiWindowMode) {
                    removeAllAppendedMessage();
                } else {
                    restoreAllAppendedMessage();
                }
            };

    private final TabModelObserver mTabModelObserver =
            new TabModelObserver() {
                @Override
                public void willCloseTab(Tab tab, boolean animate, boolean didCloseAlone) {
                    if (mCurrentTabModelFilterSupplier.get().getTabModel().getCount() == 1) {
                        removeAllAppendedMessage();
                    } else if (mPriceMessageService != null
                            && mPriceMessageService.getBindingTabId() == tab.getId()) {
                        removePriceWelcomeMessage();
                    }
                }

                @Override
                public void tabClosureUndone(Tab tab) {
                    if (mCurrentTabModelFilterSupplier.get().getTabModel().getCount() == 1) {
                        restoreAllAppendedMessage();
                    }
                    if (mPriceMessageService != null
                            && mPriceMessageService.getBindingTabId() == tab.getId()) {
                        restorePriceWelcomeMessage();
                    }
                }

                @Override
                public void tabClosureCommitted(Tab tab) {
                    // TODO(crbug.com/1157578): Auto update the PriceMessageService instead of
                    // updating it based on the client caller.
                    if (mPriceMessageService != null
                            && mPriceMessageService.getBindingTabId() == tab.getId()) {
                        mPriceMessageService.invalidateMessage();
                    }
                }
            };

    private static boolean sAppendedMessagesForTesting;

    private final @NonNull ObserverList<MessageUpdateObserver> mObservers = new ObserverList<>();
    private final @NonNull Context mContext;
    private final @NonNull ActivityLifecycleDispatcher mLifecylceDispatcher;
    private final @NonNull ObservableSupplier<TabModelFilter> mCurrentTabModelFilterSupplier;
    private final @NonNull ViewGroup mContainer;
    private final @NonNull MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull TabListCoordinator mTabListCoordinator;
    private final @NonNull LazyOneshotSupplier<TabListEditorController>
            mTabListEditorControllerSupplier;
    private final @NonNull PriceWelcomeMessageReviewActionProvider
            mPriceWelcomeMessageReviewActionProvider;
    private final @TabListMode int mMode;
    private final @NonNull MessageCardProviderCoordinator mMessageCardProviderCoordinator;
    private final @NonNull ValueChangedCallback<TabModelFilter> mOnTabModelFilterChanged =
            new ValueChangedCallback<>(this::onTabModelFilterChanged);

    private @Nullable Profile mProfile;
    private @Nullable TabGridIphDialogCoordinator mTabGridIphDialogCoordinator;
    private @Nullable IncognitoReauthManager mIncognitoReauthManager;
    private @Nullable TabSuggestionsOrchestrator mTabSuggestionsOrchestrator;
    private @Nullable TabSuggestionMessageService mTabSuggestionMessageService;
    private @Nullable PriceMessageService mPriceMessageService;
    private @Nullable IncognitoReauthPromoMessageService mIncognitoReauthPromoMessageService;

    /**
     * @param context The Android activity context.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the activity.
     * @param currentTabModelFilterSupplier The supplier of the current {@link TabModelFilter}.
     * @param container The {@link ViewGroup} of the container view.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} to observe
     *     for multi-window related changes.
     * @param snackbarManager The {@link SnackbarManager} for the activity.
     * @param modalDialogManager The {@link ModalDialogManager} for the activity.
     * @param tabListCoordinator The {@link TabListCoordinator} to show messages on.
     * @param tabListEditorControllerSupplier The supplier of the {@link TabListEditorController}.
     * @param priceWelcomeMessageReviewActionProvider The review action provider for price welcome.
     * @param mode The {@link TabListMode} the {@link TabListCoordinator} is in.
     */
    public TabSwitcherMessageManager(
            @NonNull Context context,
            @NonNull ActivityLifecycleDispatcher lifecylceDispatcher,
            @NonNull ObservableSupplier<TabModelFilter> currentTabModelFilterSupplier,
            @NonNull ViewGroup container,
            @NonNull MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            @NonNull SnackbarManager snackbarManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull TabListCoordinator tabListCoordinator,
            @NonNull LazyOneshotSupplier<TabListEditorController> tabListEditorControllerSupplier,
            @NonNull
                    PriceWelcomeMessageReviewActionProvider priceWelcomeMessageReviewActionProvider,
            @TabListMode int mode) {
        mContext = context;
        mLifecylceDispatcher = lifecylceDispatcher;
        mCurrentTabModelFilterSupplier = currentTabModelFilterSupplier;
        mContainer = container;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mSnackbarManager = snackbarManager;
        mModalDialogManager = modalDialogManager;
        mTabListCoordinator = tabListCoordinator;
        mTabListEditorControllerSupplier = tabListEditorControllerSupplier;
        mPriceWelcomeMessageReviewActionProvider = priceWelcomeMessageReviewActionProvider;
        mMode = mode;

        mMessageCardProviderCoordinator =
                new MessageCardProviderCoordinator(
                        context,
                        () -> currentTabModelFilterSupplier.get().isIncognito(),
                        this::dismissHandler);

        registerMessages(tabListCoordinator, mode);

        mMultiWindowModeStateDispatcher.addObserver(mMultiWindowModeObserver);
        mOnTabModelFilterChanged.onResult(
                currentTabModelFilterSupplier.addObserver(mOnTabModelFilterChanged));
    }

    /**
     * @param observer The {@link MessageUpdateObserver} to notify.
     */
    public void addObserver(MessageUpdateObserver observer) {
        mObservers.addObserver(observer);
    }

    /**
     * @param observer The {@link MessageUpdateObserver} to remove.
     */
    public void removeObserver(MessageUpdateObserver observer) {
        mObservers.removeObserver(observer);
    }

    /** Post-native initialization. */
    public void initWithNative(@NonNull Profile profile) {
        assert profile != null;
        mProfile = profile;
        if (mMode != TabListCoordinator.TabListMode.GRID) return;

        if (ChromeFeatureList.sArchiveTabService.isEnabled()) {
            mTabSuggestionsOrchestrator =
                    new TabSuggestionsOrchestrator(mContext, mCurrentTabModelFilterSupplier);
            mTabSuggestionMessageService =
                    new TabSuggestionMessageService(
                            mContext,
                            profile,
                            mCurrentTabModelFilterSupplier,
                            mTabListEditorControllerSupplier::get);
            mTabSuggestionsOrchestrator.addObserver(mTabSuggestionMessageService);
            mMessageCardProviderCoordinator.subscribeMessageService(mTabSuggestionMessageService);
        }

        mTabGridIphDialogCoordinator =
                new TabGridIphDialogCoordinator(mContext, mContainer, mModalDialogManager);
        IphMessageService iphMessageService =
                new IphMessageService(profile, mTabGridIphDialogCoordinator);
        mMessageCardProviderCoordinator.subscribeMessageService(iphMessageService);

        if (IncognitoReauthManager.isIncognitoReauthFeatureAvailable()
                && mIncognitoReauthPromoMessageService == null) {
            mIncognitoReauthManager = new IncognitoReauthManager();
            mIncognitoReauthPromoMessageService =
                    new IncognitoReauthPromoMessageService(
                            MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                            profile,
                            mContext,
                            ChromeSharedPreferences.getInstance(),
                            mIncognitoReauthManager,
                            mSnackbarManager,
                            () -> TabUiFeatureUtilities.isTabToGtsAnimationEnabled(mContext),
                            mLifecylceDispatcher);
            mMessageCardProviderCoordinator.subscribeMessageService(
                    mIncognitoReauthPromoMessageService);
        }
        setUpPriceTracking();
    }

    /** Called before resetting the list of tabs. */
    public void beforeReset() {
        // Invalidate price welcome message for every reset so that the stale message won't be
        // restored by mistake (e.g. from tabClosureUndone in TabSwitcherMediator).
        if (mPriceMessageService != null) {
            mPriceMessageService.invalidateMessage();
        }
    }

    /** Called after resetting the list of tabs. */
    public void afterReset(int tabCount) {
        removeAllAppendedMessage();
        if (tabCount > 0) {
            if (mPriceMessageService != null
                    && PriceTrackingUtilities.isPriceAlertsMessageCardEnabled(mProfile)) {
                mPriceMessageService.preparePriceMessage(PriceMessageType.PRICE_ALERTS, null);
            }
            appendMessagesTo(tabCount);
        }
    }

    /** Destroys the module and unregisters observers. */
    public void destroy() {
        mMultiWindowModeStateDispatcher.removeObserver(mMultiWindowModeObserver);
        removeTabModelFilterObservers(mCurrentTabModelFilterSupplier.get());
        mCurrentTabModelFilterSupplier.removeObserver(mOnTabModelFilterChanged);

        mMessageCardProviderCoordinator.destroy();
        if (mTabGridIphDialogCoordinator != null) {
            mTabGridIphDialogCoordinator.destroy();
        }
        if (mIncognitoReauthPromoMessageService != null) {
            mIncognitoReauthPromoMessageService.destroy();
        }
    }

    @Override
    public void showPriceWelcomeMessage(PriceMessageService.PriceTabData priceTabData) {
        if (mPriceMessageService == null
                || !PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled(mProfile)
                || mMessageCardProviderCoordinator.isMessageShown(
                        MessageService.MessageType.PRICE_MESSAGE, PriceMessageType.PRICE_WELCOME)) {
            return;
        }
        if (mPriceMessageService.preparePriceMessage(
                PriceMessageType.PRICE_WELCOME, priceTabData)) {
            appendNextMessage(MessageService.MessageType.PRICE_MESSAGE);
            // To make the message card in view when user enters tab switcher, we should scroll to
            // current tab with 0 offset. See {@link
            // TabSwitcherMediator#setInitialScrollIndexOffset} for more details.
            mPriceWelcomeMessageReviewActionProvider.scrollToTab(
                    mCurrentTabModelFilterSupplier.get().index());
        }
        for (MessageUpdateObserver observer : mObservers) {
            observer.onShowPriceWelcomeMessage();
        }
    }

    @Override
    public void removePriceWelcomeMessage() {
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.LARGE_MESSAGE, MessageService.MessageType.PRICE_MESSAGE);
        for (MessageUpdateObserver observer : mObservers) {
            observer.onRemovePriceWelcomeMessage();
        }
    }

    @Override
    public void restorePriceWelcomeMessage() {
        appendNextMessage(MessageService.MessageType.PRICE_MESSAGE);
        for (MessageUpdateObserver observer : mObservers) {
            observer.onRestorePriceWelcomeMessage();
        }
    }

    private void appendNextMessage(@MessageService.MessageType int messageType) {
        assert mMessageCardProviderCoordinator != null;

        MessageCardProviderMediator.Message nextMessage =
                mMessageCardProviderCoordinator.getNextMessageItemForType(messageType);
        if (nextMessage == null || !shouldAppendMessage(nextMessage)) return;
        if (messageType == MessageService.MessageType.PRICE_MESSAGE) {
            mTabListCoordinator.addSpecialListItem(
                    mTabListCoordinator.getPriceWelcomeMessageInsertionIndex(),
                    TabProperties.UiType.LARGE_MESSAGE,
                    nextMessage.model);
        } else {
            mTabListCoordinator.addSpecialListItemToEnd(
                    TabProperties.UiType.MESSAGE, nextMessage.model);
        }
    }

    private void appendMessagesTo(int index) {
        if (mMultiWindowModeStateDispatcher.isInMultiWindowMode()) return;
        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            if (!shouldAppendMessage(messages.get(i))) continue;
            if (messages.get(i).type == MessageService.MessageType.PRICE_MESSAGE) {
                mTabListCoordinator.addSpecialListItem(
                        index, TabProperties.UiType.LARGE_MESSAGE, messages.get(i).model);
            } else if (messages.get(i).type
                    == MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE) {
                mayAddIncognitoReauthPromoCard(messages.get(i).model);
            } else if (messages.get(i).type == MessageService.MessageType.TAB_SUGGESTION) {
                // TODO(crbug.com/1487664): Update to a mayAdd call checking show criteria
                mTabListCoordinator.addSpecialListItem(
                        mCurrentTabModelFilterSupplier.get().index() + 1,
                        TabProperties.UiType.LARGE_MESSAGE,
                        messages.get(i).model);
            } else {
                mTabListCoordinator.addSpecialListItem(
                        index, TabProperties.UiType.MESSAGE, messages.get(i).model);
            }
            index++;
        }
        if (messages.size() > 0) sAppendedMessagesForTesting = true;
        for (MessageUpdateObserver observer : mObservers) {
            observer.onAppendedMessage();
        }
    }

    private void mayAddIncognitoReauthPromoCard(PropertyModel model) {
        if (mIncognitoReauthPromoMessageService.isIncognitoReauthPromoMessageEnabled(mProfile)) {
            mTabListCoordinator.addSpecialListItemToEnd(TabProperties.UiType.LARGE_MESSAGE, model);
            mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
        }
    }

    private boolean shouldAppendMessage(MessageCardProviderMediator.Message message) {
        if (mTabListCoordinator.specialItemExists(message.type)) return false;
        PropertyModel messageModel = message.model;

        Integer messageCardVisibilityControlValue =
                messageModel.get(
                        MessageCardViewProperties
                                .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE);

        @MessageCardViewProperties.MessageCardScope
        int scope =
                (messageCardVisibilityControlValue != null)
                        ? messageCardVisibilityControlValue
                        : MessageCardViewProperties.MessageCardScope.REGULAR;

        if (scope == MessageCardViewProperties.MessageCardScope.BOTH) return true;
        return mCurrentTabModelFilterSupplier.get().isIncognito()
                ? scope == MessageCardViewProperties.MessageCardScope.INCOGNITO
                : scope == MessageCardViewProperties.MessageCardScope.REGULAR;
    }

    /**
     * Remove all the message items in the model list. Right now this is used when all tabs are
     * closed in the grid tab switcher.
     */
    private void removeAllAppendedMessage() {
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.MESSAGE, MessageService.MessageType.ALL);
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.LARGE_MESSAGE,
                MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.LARGE_MESSAGE, MessageService.MessageType.TAB_SUGGESTION);
        sAppendedMessagesForTesting = false;
        for (MessageUpdateObserver observer : mObservers) {
            observer.onRemoveAllAppendedMessage();
        }
    }

    /**
     * Restore all the message items that should show. Right now this is only used to restore
     * message items when the closure of the last tab in tab switcher is undone.
     */
    private void restoreAllAppendedMessage() {
        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            if (!shouldAppendMessage(messages.get(i))) continue;
            // The restore of PRICE_MESSAGE is handled in the restorePriceWelcomeMessage() below.
            if (messages.get(i).type == MessageService.MessageType.PRICE_MESSAGE) {
                continue;
            } else if (messages.get(i).type
                    == MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE) {
                mTabListCoordinator.addSpecialListItemToEnd(
                        TabProperties.UiType.LARGE_MESSAGE, messages.get(i).model);
            } else if (messages.get(i).type == MessageService.MessageType.TAB_SUGGESTION) {
                mTabListCoordinator.addSpecialListItemToEnd(
                        TabProperties.UiType.LARGE_MESSAGE, messages.get(i).model);
            } else {
                mTabListCoordinator.addSpecialListItemToEnd(
                        TabProperties.UiType.MESSAGE, messages.get(i).model);
            }
        }
        sAppendedMessagesForTesting = messages.size() > 0;
        for (MessageUpdateObserver observer : mObservers) {
            observer.onRestoreAllAppendedMessage();
        }
    }

    private void registerMessages(
            @NonNull TabListCoordinator tabListCoordinator, @TabListMode int mode) {
        if (mode != TabListCoordinator.TabListMode.GRID) return;

        tabListCoordinator.registerItemType(
                TabProperties.UiType.MESSAGE,
                new LayoutViewBuilder(R.layout.tab_grid_message_card_item),
                MessageCardViewBinder::bind);

        tabListCoordinator.registerItemType(
                TabProperties.UiType.LARGE_MESSAGE,
                new LayoutViewBuilder(R.layout.large_message_card_item),
                LargeMessageCardViewBinder::bind);

        if (ChromeFeatureList.sArchiveTabService.isEnabled()) {
            tabListCoordinator.registerItemType(
                    TabProperties.UiType.CUSTOM_MESSAGE,
                    new LayoutViewBuilder(R.layout.custom_message_card_item),
                    (model, view, key) -> {
                        CustomMessageCardViewBinder.bind(
                                model,
                                new CustomMessageCardViewBinder.ViewHolder(
                                        (CustomMessageCardView) view, mTabSuggestionMessageService),
                                key);
                    });
        }
    }

    private void setUpPriceTracking() {
        assert mProfile != null;
        if (PriceTrackingFeatures.isPriceTrackingEnabled(mProfile)) {
            PriceDropNotificationManager notificationManager =
                    PriceDropNotificationManagerFactory.create();
            if (mPriceMessageService == null) {
                mPriceMessageService =
                        new PriceMessageService(
                                mProfile,
                                mTabListCoordinator,
                                mPriceWelcomeMessageReviewActionProvider,
                                notificationManager);
            }
            mMessageCardProviderCoordinator.subscribeMessageService(mPriceMessageService);
        }
    }

    private void dismissHandler(@MessageType int messageType) {
        if (messageType == MessageService.MessageType.PRICE_MESSAGE
                || messageType == MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE
                || messageType == MessageService.MessageType.TAB_SUGGESTION) {
            mTabListCoordinator.removeSpecialListItem(
                    TabProperties.UiType.LARGE_MESSAGE, messageType);
        } else {
            mTabListCoordinator.removeSpecialListItem(TabProperties.UiType.MESSAGE, messageType);
            appendNextMessage(messageType);
        }
    }

    private void onTabModelFilterChanged(
            @Nullable TabModelFilter newFilter, @Nullable TabModelFilter oldFilter) {
        removeTabModelFilterObservers(oldFilter);

        if (newFilter != null) {
            newFilter.addObserver(mTabModelObserver);
        }
    }

    private void removeTabModelFilterObservers(@Nullable TabModelFilter filter) {
        if (filter != null) {
            filter.removeObserver(mTabModelObserver);
        }
    }

    /** Returns whether this manager has appended any messages. */
    public static boolean hasAppendedMessagesForTesting() {
        return sAppendedMessagesForTesting;
    }

    /** Resets whether this manager has appended any messages. */
    public static void resetHasAppendedMessagesForTesting() {
        sAppendedMessagesForTesting = false;
    }

    void setPriceMessageServiceForTesting(@NonNull PriceMessageService priceMessageService) {
        assert mPriceMessageService == null
                : "setPriceMessageServiceForTesting() must be before initWithNative().";
        mPriceMessageService = priceMessageService;
    }
}
