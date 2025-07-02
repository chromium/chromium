// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.feature_engagement.TrackerFactory;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.incognito.reauth.IncognitoReauthManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.preferences.ChromeSharedPreferences;
import org.chromium.chrome.browser.price_tracking.PriceTrackingFeatures;
import org.chromium.chrome.browser.price_tracking.PriceTrackingUtilities;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabId;
import org.chromium.chrome.browser.tab_group_sync.TabGroupSyncServiceFactory;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tab_ui.TabGridIphDialogCoordinator;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelObserver;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageProvider;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;

/** Manages message related glue for the {@link TabSwitcher}. */
@NullMarked
public class TabSwitcherMessageManager implements PriceWelcomeMessageController {
    /** Used to observe updates to message cards. */
    public interface MessageUpdateObserver {
        /** Invoked when messages are added. */
        default void onAppendedMessage() {}

        /** Invoked when a message is removed. */
        default void onRemovedMessage() {}

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
                public void willCloseTab(Tab tab, boolean didCloseAlone) {
                    TabGroupModelFilter tabGroupModelFilter =
                            mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(tabGroupModelFilter);
                    if (tabGroupModelFilter.getTabModel().getCount() == 1) {
                        removeAllAppendedMessage();
                    } else if (mPriceMessageService != null
                            && mPriceMessageService.getBindingTabId() == tab.getId()) {
                        removePriceWelcomeMessage();
                    }
                }

                @Override
                public void tabClosureUndone(Tab tab) {
                    TabGroupModelFilter tabGroupModelFilter =
                            mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(tabGroupModelFilter);
                    if (tabGroupModelFilter.getTabModel().getCount() == 1) {
                        restoreAllAppendedMessage();
                    }
                    if (mPriceMessageService != null
                            && mPriceMessageService.getBindingTabId() == tab.getId()) {
                        restorePriceWelcomeMessage();
                    }
                }

                @Override
                public void tabClosureCommitted(Tab tab) {
                    // TODO(crbug.com/40160889): Auto update the PriceMessageService instead of
                    // updating it based on the client caller.
                    if (mPriceMessageService != null
                            && mPriceMessageService.getBindingTabId() == tab.getId()) {
                        mPriceMessageService.invalidateMessage();
                    }
                }
            };

    private static boolean sAppendedMessagesForTesting;

    private final ObserverList<MessageUpdateObserver> mObservers = new ObserverList<>();
    private final Activity mActivity;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final TabGridIphDialogCoordinator mTabGridIphDialogCoordinator;
    private final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;
    private final SnackbarManager mSnackbarManager;
    private final ModalDialogManager mModalDialogManager;
    private final MessageCardProviderCoordinator mMessageCardProviderCoordinator;
    private final Callback<@Nullable TabGroupModelFilter> mOnTabGroupModelFilterChanged =
            new ValueChangedCallback<>(this::onTabGroupModelFilterChanged);
    private final ObservableSupplierImpl<@Nullable PriceWelcomeMessageReviewActionProvider>
            mPriceWelcomeMessageReviewActionProviderSupplier = new ObservableSupplierImpl<>();
    private final ObservableSupplierImpl<@Nullable TabListCoordinator> mTabListCoordinatorSupplier =
            new ObservableSupplierImpl<>();
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabContentManager mTabContentManager;
    private final @TabListMode int mTabListMode;
    private final ViewGroup mRootView;
    private final TabCreator mRegularTabCreator;
    private final BackPressManager mBackPressManager;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final Supplier<PaneManager> mPaneManagerSupplier;
    private final Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final Supplier<LayoutStateProvider> mLayoutStateProviderSupplier;

    private Profile mProfile;
    private @Nullable PriceMessageService mPriceMessageService;
    private @Nullable IncognitoReauthPromoMessageService mIncognitoReauthPromoMessageService;
    private @Nullable TabGroupSuggestionMessageService mTabGroupSuggestionMessageService;
    private ArchivedTabsMessageService mArchivedTabsMessageService;

    /**
     * @param activity The Android activity.
     * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for the activity.
     * @param currentTabGroupModelFilterSupplier The supplier of the current {@link
     *     TabGroupModelFilter}.
     * @param multiWindowModeStateDispatcher The {@link MultiWindowModeStateDispatcher} to observe
     *     for multi-window related changes.
     * @param snackbarManager The {@link SnackbarManager} for the activity.
     * @param modalDialogManager The {@link ModalDialogManager} for the activity.
     * @param browserControlStateProvider Provides the state of browser controls.
     * @param tabContentManager Serves tab content to UI components.
     * @param tabListMode The {@link TabListMode} determining how the TabList will be displayed.
     * @param rootView The root {@link ViewGroup} to attach dialogs to.
     * @param regularTabCreator Manages the creation of regular tabs.
     * @param backPressManager Manages the different back press handlers in the app.
     * @param desktopWindowStateManager Manager to get desktop window and app header state.
     * @param edgeToEdgeSupplier Supplier to the {@link EdgeToEdgeController} instance.
     * @param paneManagerSupplier Used to switch and communicate with other panes.
     * @param tabGroupUiActionHandlerSupplier Used to open hidden tab groups.
     * @param layoutStateProviderSupplier Supplies the LayoutStateProvider, which is used to observe
     *     when the TabSwitcher is hidden.
     */
    public TabSwitcherMessageManager(
            Activity activity,
            ActivityLifecycleDispatcher lifecycleDispatcher,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            SnackbarManager snackbarManager,
            ModalDialogManager modalDialogManager,
            BrowserControlsStateProvider browserControlStateProvider,
            TabContentManager tabContentManager,
            @TabListMode int tabListMode,
            ViewGroup rootView,
            TabCreator regularTabCreator,
            BackPressManager backPressManager,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            Supplier<PaneManager> paneManagerSupplier,
            Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            Supplier<LayoutStateProvider> layoutStateProviderSupplier) {
        mActivity = activity;
        mLifecycleDispatcher = lifecycleDispatcher;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;
        mSnackbarManager = snackbarManager;
        mModalDialogManager = modalDialogManager;
        mBrowserControlsStateProvider = browserControlStateProvider;
        mTabContentManager = tabContentManager;
        mTabListMode = tabListMode;
        mRootView = rootView;
        mRegularTabCreator = regularTabCreator;
        mBackPressManager = backPressManager;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;

        Supplier<Profile> profileSupplier =
                () -> {
                    TabGroupModelFilter tabGroupModelFilter =
                            mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(tabGroupModelFilter);
                    return assumeNonNull(tabGroupModelFilter.getTabModel().getProfile());
                };
        mMessageCardProviderCoordinator =
                new MessageCardProviderCoordinator(activity, profileSupplier, this::dismissHandler);

        mTabGridIphDialogCoordinator =
                new TabGridIphDialogCoordinator(activity, mModalDialogManager);

        mMultiWindowModeStateDispatcher.addObserver(mMultiWindowModeObserver);
        mOnTabGroupModelFilterChanged.onResult(
                currentTabGroupModelFilterSupplier.addObserver(mOnTabGroupModelFilterChanged));
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mPaneManagerSupplier = paneManagerSupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
    }

    /**
     * Bind the message manager to emit messages on a specific {@link TabListCoordinator}. If
     * already bound to a coordinator messages are removed from the previous coordinator.
     *
     * @param tabListCoordinator The {@link TabListCoordinator} to show messages on.
     * @param container The {@link ViewGroup} of the container view.
     * @param priceWelcomeMessageReviewActionProvider The review action provider for price welcome.
     * @param onTabSelectingListener The {@link OnTabSelectingListener} for the parent tab switcher.
     */
    public void bind(
            TabListCoordinator tabListCoordinator,
            ViewGroup container,
            PriceWelcomeMessageReviewActionProvider priceWelcomeMessageReviewActionProvider,
            OnTabSelectingListener onTabSelectingListener) {
        TabListCoordinator oldTabListCoordinator = mTabListCoordinatorSupplier.get();
        if (oldTabListCoordinator != null) {
            if (oldTabListCoordinator != tabListCoordinator) {
                unbind(oldTabListCoordinator);
            }
        }
        mTabListCoordinatorSupplier.set(tabListCoordinator);
        mPriceWelcomeMessageReviewActionProviderSupplier.set(
                priceWelcomeMessageReviewActionProvider);

        mTabGridIphDialogCoordinator.setParentView(container);
        if (mArchivedTabsMessageService != null) {
            mArchivedTabsMessageService.setOnTabSelectingListener(onTabSelectingListener);
        }

        // Don't add any messages. Wait for the next time tabs are loaded into the coordinator.
    }

    /**
     * Unbinds a {@link TabListCoordinator} and related objects from receiving updates.
     *
     * @param tabListCoordinator The {@link TabListCoordinator} to remove messages from.
     */
    public void unbind(TabListCoordinator tabListCoordinator) {
        TabListCoordinator currentTabListCoordinator = mTabListCoordinatorSupplier.get();
        if (currentTabListCoordinator != tabListCoordinator) return;

        removeAllAppendedMessage();

        mTabListCoordinatorSupplier.set(null);
        mPriceWelcomeMessageReviewActionProviderSupplier.set(null);

        mTabGridIphDialogCoordinator.setParentView(null);
    }

    /**
     * Register messages for a particular {@link TabListCoordinator}. Should only be called once per
     * coordinator.
     */
    public void registerMessages(TabListCoordinator tabListCoordinator) {
        tabListCoordinator.registerItemType(
                TabProperties.UiType.MESSAGE,
                new LayoutViewBuilder(R.layout.tab_grid_message_card_item),
                MessageCardViewBinder::bind);

        tabListCoordinator.registerItemType(
                TabProperties.UiType.LARGE_MESSAGE,
                new LayoutViewBuilder(R.layout.large_message_card_item),
                LargeMessageCardViewBinder::bind);

        tabListCoordinator.registerItemType(
                TabProperties.UiType.CUSTOM_MESSAGE,
                new LayoutViewBuilder(R.layout.custom_message_card_item),
                CustomMessageCardViewBinder::bind);
    }

    /** Post-native initialization. */
    @Initializer
    public void initWithNative(Profile profile, @TabListMode int mode) {
        assert profile != null;
        mProfile = profile;

        mArchivedTabsMessageService =
                new ArchivedTabsMessageService(
                        mActivity,
                        ArchivedTabModelOrchestrator.getForProfile(mProfile),
                        mBrowserControlsStateProvider,
                        mTabContentManager,
                        mTabListMode,
                        mRootView,
                        mSnackbarManager,
                        mRegularTabCreator,
                        mBackPressManager,
                        mModalDialogManager,
                        TrackerFactory.getTrackerForProfile(profile),
                        () -> appendNextMessage(MessageService.MessageType.ARCHIVED_TABS_MESSAGE),
                        mTabListCoordinatorSupplier,
                        mDesktopWindowStateManager,
                        mEdgeToEdgeSupplier,
                        TabGroupSyncServiceFactory.getForProfile(mProfile),
                        mPaneManagerSupplier,
                        mTabGroupUiActionHandlerSupplier,
                        mCurrentTabGroupModelFilterSupplier,
                        mLayoutStateProviderSupplier);
        addObserver(mArchivedTabsMessageService);
        mMessageCardProviderCoordinator.subscribeMessageService(mArchivedTabsMessageService);

        IphMessageService iphMessageService =
                new IphMessageService(profile, mTabGridIphDialogCoordinator);
        mMessageCardProviderCoordinator.subscribeMessageService(iphMessageService);

        if (IncognitoReauthManager.isIncognitoReauthFeatureAvailable()
                && mIncognitoReauthPromoMessageService == null) {
            IncognitoReauthManager incognitoReauthManager =
                    new IncognitoReauthManager(mActivity, profile);
            mIncognitoReauthPromoMessageService =
                    new IncognitoReauthPromoMessageService(
                            MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                            profile,
                            mActivity,
                            ChromeSharedPreferences.getInstance(),
                            incognitoReauthManager,
                            mSnackbarManager,
                            mLifecycleDispatcher);
            mMessageCardProviderCoordinator.subscribeMessageService(
                    mIncognitoReauthPromoMessageService);

            if (ChromeFeatureList.sTabSwitcherGroupSuggestionsAndroid.isEnabled()) {
                mTabGroupSuggestionMessageService =
                        new TabGroupSuggestionMessageService(
                                mActivity,
                                mCurrentTabGroupModelFilterSupplier,
                                this::addTabGroupSuggestionMessage);
                mMessageCardProviderCoordinator.subscribeMessageService(
                        mTabGroupSuggestionMessageService);
            }
        }
        setUpPriceTracking();
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

    /** Called before resetting the list of tabs. */
    public void beforeReset() {
        // Unregister and re-register the TabModelObserver along with
        // TabListCoordinator#resetWithListOfTabs to ensure that the TabList gets observer
        // calls before the TabSwitcherMessageManager.
        removeTabGroupModelFilterObservers(mCurrentTabGroupModelFilterSupplier.get());
        // Invalidate price welcome message for every reset so that the stale message won't be
        // restored by mistake (e.g. from tabClosureUndone in TabSwitcherMediator).
        if (mPriceMessageService != null) {
            mPriceMessageService.invalidateMessage();
        }
    }

    /** Called after resetting the list of tabs. */
    public void afterReset(int tabCount) {
        onTabGroupModelFilterChanged(mCurrentTabGroupModelFilterSupplier.get(), null);
        removeAllAppendedMessage();
        if (tabCount > 0) {
            appendMessagesTo(tabCount);
        }
    }

    /** Destroys the module and unregisters observers. */
    @SuppressWarnings("NullAway")
    public void destroy() {
        mMultiWindowModeStateDispatcher.removeObserver(mMultiWindowModeObserver);
        removeTabGroupModelFilterObservers(mCurrentTabGroupModelFilterSupplier.get());
        mCurrentTabGroupModelFilterSupplier.removeObserver(mOnTabGroupModelFilterChanged);

        mMessageCardProviderCoordinator.destroy();
        mTabGridIphDialogCoordinator.destroy();
        if (mIncognitoReauthPromoMessageService != null) {
            mIncognitoReauthPromoMessageService.destroy();
        }
        if (mTabGroupSuggestionMessageService != null) {
            mTabGroupSuggestionMessageService.destroy();
        }
        if (mArchivedTabsMessageService != null) {
            mArchivedTabsMessageService.destroy();
            mArchivedTabsMessageService = null;
        }
    }

    @Override
    public void showPriceWelcomeMessage(PriceMessageService.PriceTabData priceTabData) {
        @Nullable
        PriceWelcomeMessageReviewActionProvider actionProvider =
                mPriceWelcomeMessageReviewActionProviderSupplier.get();
        if (actionProvider == null) return;

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
            TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
            assumeNonNull(filter);
            actionProvider.scrollToTab(filter.getCurrentRepresentativeTabIndex());
        }
        for (MessageUpdateObserver observer : mObservers) {
            observer.onShowPriceWelcomeMessage();
        }
    }

    @Override
    public void removePriceWelcomeMessage() {
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator != null) {
            tabListCoordinator.removeSpecialListItem(
                    TabProperties.UiType.LARGE_MESSAGE, MessageService.MessageType.PRICE_MESSAGE);
        }

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

    public @Nullable TabGroupSuggestionMessageService getTabGroupSuggestionMessageService() {
        return mTabGroupSuggestionMessageService;
    }

    private void appendNextMessage(@MessageService.MessageType int messageType) {
        assert mMessageCardProviderCoordinator != null;
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator == null) return;

        MessageCardProviderMediator.Message nextMessage =
                mMessageCardProviderCoordinator.getNextMessageItemForType(messageType);
        if (nextMessage == null || !shouldAppendMessage(nextMessage)) return;
        if (messageType == MessageService.MessageType.PRICE_MESSAGE) {
            tabListCoordinator.addSpecialListItem(
                    tabListCoordinator.getPriceWelcomeMessageInsertionIndex(),
                    TabProperties.UiType.LARGE_MESSAGE,
                    nextMessage.model);
        } else if (messageType == MessageService.MessageType.ARCHIVED_TABS_MESSAGE) {
            tabListCoordinator.addSpecialListItem(
                    0, TabProperties.UiType.CUSTOM_MESSAGE, nextMessage.model);
        } else {
            tabListCoordinator.addSpecialListItem(
                    tabListCoordinator.getTabListModelSize(),
                    TabProperties.UiType.MESSAGE,
                    nextMessage.model);
        }
        for (MessageUpdateObserver observer : mObservers) {
            observer.onAppendedMessage();
        }
    }

    private void appendMessagesTo(int index) {
        if (!shouldShowMessages()) return;

        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        assert tabListCoordinator != null;

        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            if (!shouldAppendMessage(messages.get(i))) continue;
            if (messages.get(i).type == MessageService.MessageType.PRICE_MESSAGE) {
                tabListCoordinator.addSpecialListItem(
                        index, TabProperties.UiType.LARGE_MESSAGE, messages.get(i).model);
            } else if (messages.get(i).type
                    == MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE) {
                if (!mayAddIncognitoReauthPromoCard(messages.get(i).model)) {
                    // Skip incrementing index if the message was not added.
                    continue;
                }
            } else if (messages.get(i).type == MessageService.MessageType.ARCHIVED_TABS_MESSAGE) {
                // Always add the archived tabs message to the start.
                tabListCoordinator.addSpecialListItem(
                        0, TabProperties.UiType.CUSTOM_MESSAGE, messages.get(i).model);
            } else {
                tabListCoordinator.addSpecialListItem(
                        index, TabProperties.UiType.MESSAGE, messages.get(i).model);
            }
            index++;
        }
        if (messages.size() > 0) sAppendedMessagesForTesting = true;
        for (MessageUpdateObserver observer : mObservers) {
            observer.onAppendedMessage();
        }
    }

    private boolean mayAddIncognitoReauthPromoCard(PropertyModel model) {
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        assert tabListCoordinator != null;
        assumeNonNull(mIncognitoReauthPromoMessageService);
        if (mIncognitoReauthPromoMessageService.isIncognitoReauthPromoMessageEnabled(mProfile)) {
            tabListCoordinator.addSpecialListItem(
                    tabListCoordinator.getTabListModelSize(),
                    TabProperties.UiType.LARGE_MESSAGE,
                    model);
            mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
            return true;
        }
        return false;
    }

    private boolean shouldAppendMessage(MessageCardProviderMediator.Message message) {
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        assert tabListCoordinator != null;
        if (tabListCoordinator.specialItemExists(message.type)) return false;
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

        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        return filter.getTabModel().isIncognito()
                ? scope == MessageCardViewProperties.MessageCardScope.INCOGNITO
                : scope == MessageCardViewProperties.MessageCardScope.REGULAR;
    }

    /**
     * Remove all the message items in the model list. Right now this is used when all tabs are
     * closed in the grid tab switcher.
     */
    private void removeAllAppendedMessage() {
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator == null) return;

        tabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.MESSAGE, MessageService.MessageType.ALL);
        tabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.LARGE_MESSAGE,
                MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        tabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.CUSTOM_MESSAGE,
                MessageService.MessageType.ARCHIVED_TABS_MESSAGE);

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
        if (!shouldShowMessages()) return;
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        assert tabListCoordinator != null;

        assert assumeNonNull(mCurrentTabGroupModelFilterSupplier.get()).getTabModel().getProfile()
                != null;

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
                tabListCoordinator.addSpecialListItem(
                        tabListCoordinator.getTabListModelSize(),
                        TabProperties.UiType.LARGE_MESSAGE,
                        messages.get(i).model);
            } else if (messages.get(i).type == MessageService.MessageType.ARCHIVED_TABS_MESSAGE) {
                tabListCoordinator.addSpecialListItem(
                        0, TabProperties.UiType.CUSTOM_MESSAGE, messages.get(i).model);
            } else {
                tabListCoordinator.addSpecialListItem(
                        tabListCoordinator.getTabListModelSize(),
                        TabProperties.UiType.MESSAGE,
                        messages.get(i).model);
            }
        }
        sAppendedMessagesForTesting = messages.size() > 0;
        for (MessageUpdateObserver observer : mObservers) {
            observer.onRestoreAllAppendedMessage();
        }
    }

    private void setUpPriceTracking() {
        assert mProfile != null;
        if (PriceTrackingFeatures.isPriceAnnotationsEnabled(mProfile)) {
            if (mPriceMessageService == null) {
                Supplier<PriceWelcomeMessageProvider> priceWelcomeMessageProviderSupplier =
                        (Supplier<PriceWelcomeMessageProvider>)
                                ((Supplier<? extends PriceWelcomeMessageProvider>)
                                        mTabListCoordinatorSupplier);
                mPriceMessageService =
                        new PriceMessageService(
                                mProfile,
                                priceWelcomeMessageProviderSupplier,
                                mPriceWelcomeMessageReviewActionProviderSupplier);
            }
            mMessageCardProviderCoordinator.subscribeMessageService(mPriceMessageService);
        }
    }

    @VisibleForTesting
    void dismissHandler(@MessageType int messageType) {
        // `bind` and `unbind` to attach to a `TabListCoordinator` are independent of whether the
        // `MessageCardProviderCoordinator`, has access to the `dismissHandler`. That means this
        // may be called while unbound by one of the message services.
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator == null) return;

        if (messageType == MessageService.MessageType.PRICE_MESSAGE
                || messageType == MessageService.MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE) {
            tabListCoordinator.removeSpecialListItem(
                    TabProperties.UiType.LARGE_MESSAGE, messageType);
        } else if (messageType == MessageService.MessageType.ARCHIVED_TABS_MESSAGE) {
            tabListCoordinator.removeSpecialListItem(
                    TabProperties.UiType.CUSTOM_MESSAGE, messageType);
        } else {
            tabListCoordinator.removeSpecialListItem(TabProperties.UiType.MESSAGE, messageType);
            appendNextMessage(messageType);
        }
        for (MessageUpdateObserver observer : mObservers) {
            observer.onRemovedMessage();
        }
    }

    private void onTabGroupModelFilterChanged(
            @Nullable TabGroupModelFilter newFilter, @Nullable TabGroupModelFilter oldFilter) {
        removeTabGroupModelFilterObservers(oldFilter);

        if (newFilter != null) {
            newFilter.addObserver(mTabModelObserver);
        }
    }

    private void removeTabGroupModelFilterObservers(@Nullable TabGroupModelFilter filter) {
        if (filter != null) {
            filter.removeObserver(mTabModelObserver);
        }
    }

    private boolean shouldShowMessages() {
        return !mMultiWindowModeStateDispatcher.isInMultiWindowMode()
                && mTabListCoordinatorSupplier.get() != null;
    }

    /** Returns whether this manager has appended any messages. */
    public static boolean hasAppendedMessagesForTesting() {
        return sAppendedMessagesForTesting;
    }

    /** Resets whether this manager has appended any messages. */
    public static void resetHasAppendedMessagesForTesting() {
        sAppendedMessagesForTesting = false;
    }

    boolean isNativeInitializedForTesting() {
        return mProfile != null;
    }

    void setPriceMessageServiceForTesting(PriceMessageService priceMessageService) {
        assert mPriceMessageService == null
                : "setPriceMessageServiceForTesting() must be before initWithNative().";
        mPriceMessageService = priceMessageService;
    }

    private void addTabGroupSuggestionMessage(@TabId int tabId) {
        @MessageType int messageType = MessageType.TAB_GROUP_SUGGESTION_MESSAGE;

        assert mMessageCardProviderCoordinator != null;
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator == null) return;

        MessageCardProviderMediator.Message nextMessage =
                mMessageCardProviderCoordinator.getNextMessageItemForType(messageType);
        if (nextMessage == null || !shouldAppendMessage(nextMessage)) return;

        int index = tabListCoordinator.getIndexFromTabId(tabId);
        if (index == TabModel.INVALID_TAB_INDEX) return;

        tabListCoordinator.addSpecialListItem(
                index + 1, TabProperties.UiType.MESSAGE, nextMessage.model);
        for (MessageUpdateObserver observer : mObservers) {
            observer.onAppendedMessage();
        }
    }
}
