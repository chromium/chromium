// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.UiTypeHelper.messageTypeToUiType;

import android.app.Activity;
import android.view.ViewGroup;

import androidx.annotation.IntDef;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.ObserverList;
import org.chromium.base.ValueChangedCallback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
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
import org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MessageCardScope;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.Message;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceWelcomeMessageReviewActionProvider;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.List;
import java.util.function.Supplier;

/** Manages message related glue for the {@link TabSwitcher}. */
@NullMarked
public class TabSwitcherMessageManager {
    /** Represents the types of messages that can be shown in the tab switcher. */
    @IntDef({
        MessageType.IPH,
        MessageType.PRICE_MESSAGE,
        MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE,
        MessageType.ARCHIVED_TABS_MESSAGE,
        MessageType.ARCHIVED_TABS_IPH_MESSAGE,
        MessageType.COLLABORATION_ACTIVITY,
        MessageType.TAB_GROUP_SUGGESTION_MESSAGE,
        MessageType.ALL
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.TYPE_USE})
    public @interface MessageType {
        int FOR_TESTING = 0;
        int IPH = 1;
        int PRICE_MESSAGE = 2;
        int INCOGNITO_REAUTH_PROMO_MESSAGE = 3;
        int ARCHIVED_TABS_MESSAGE = 4;
        int ARCHIVED_TABS_IPH_MESSAGE = 5;
        int COLLABORATION_ACTIVITY = 6;
        int TAB_GROUP_SUGGESTION_MESSAGE = 7;
        int ALL = 8;
    }

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
                    removeMessagesIfTabModelEmpty(/* numTabsToRemove= */ 1);
                }

                @Override
                public void willCloseMultipleTabs(boolean allowUndo, List<Tab> tabs) {
                    // Handles case where all tabs are removed without using 'Close All Tabs'
                    // option.
                    removeMessagesIfTabModelEmpty(/* numTabsToRemove= */ tabs.size());
                }

                @Override
                public void tabClosureUndone(Tab tab) {
                    TabGroupModelFilter tabGroupModelFilter =
                            mCurrentTabGroupModelFilterSupplier.get();
                    assumeNonNull(tabGroupModelFilter);
                    if (tabGroupModelFilter.getTabModel().getCount() == 1) {
                        restoreAllAppendedMessage();
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
    private final MessageCardProviderCoordinator<@MessageType Integer, @UiType Integer>
            mMessageCardProviderCoordinator;
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

    private @Nullable Profile mProfile;
    private @Nullable PriceWelcomeMessageController mPriceWelcomeMessageController;
    private @Nullable IncognitoReauthPromoMessageService mIncognitoReauthPromoMessageService;
    private @Nullable TabGroupSuggestionMessageService mTabGroupSuggestionMessageService;
    private @Nullable ArchivedTabsMessageService mArchivedTabsMessageService;

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

        mMessageCardProviderCoordinator =
                new MessageCardProviderCoordinator<@MessageType Integer, @UiType Integer>(
                        activity, this::dismissHandler);

        mTabGridIphDialogCoordinator =
                new TabGridIphDialogCoordinator(activity, mModalDialogManager);

        mMultiWindowModeStateDispatcher.addObserver(mMultiWindowModeObserver);
        currentTabGroupModelFilterSupplier.addSyncObserverAndCallIfNonNull(
                mOnTabGroupModelFilterChanged);
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mPaneManagerSupplier = paneManagerSupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
    }

    /**
     * Register message host delegate for a particular {@link TabListCoordinator}. Should only be
     * called once per coordinator.
     *
     * @param messageHostDelegate The {@link MessageHostDelegate} to register.
     */
    public void registerMessageHostDelegate(
            MessageHostDelegate<@MessageType Integer, @UiType Integer> messageHostDelegate) {
        mMessageCardProviderCoordinator.bindHostDelegate(messageHostDelegate);
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

    /** Post-native initialization. */
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
                        () -> appendNextMessage(MessageType.ARCHIVED_TABS_MESSAGE),
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
                new IphMessageService(this::getCurrentProfile, mTabGridIphDialogCoordinator);
        mMessageCardProviderCoordinator.subscribeMessageService(iphMessageService);

        if (IncognitoReauthManager.isIncognitoReauthFeatureAvailable()
                && mIncognitoReauthPromoMessageService == null) {
            IncognitoReauthManager incognitoReauthManager =
                    new IncognitoReauthManager(mActivity, profile);
            mIncognitoReauthPromoMessageService =
                    new IncognitoReauthPromoMessageService(
                            profile,
                            mActivity,
                            ChromeSharedPreferences.getInstance(),
                            incognitoReauthManager,
                            mSnackbarManager,
                            mLifecycleDispatcher);
            mMessageCardProviderCoordinator.subscribeMessageService(
                    mIncognitoReauthPromoMessageService);
        }
        if (ChromeFeatureList.sTabSwitcherGroupSuggestionsAndroid.isEnabled()) {
            mTabGroupSuggestionMessageService =
                    new TabGroupSuggestionMessageService(
                            mActivity,
                            mCurrentTabGroupModelFilterSupplier,
                            this::addTabGroupSuggestionMessage,
                            this::translateStartMergeAnimation);
            mMessageCardProviderCoordinator.subscribeMessageService(
                    mTabGroupSuggestionMessageService);
        }
        mPriceWelcomeMessageController =
                PriceWelcomeMessageController.build(
                        this,
                        mCurrentTabGroupModelFilterSupplier,
                        mMessageCardProviderCoordinator,
                        mPriceWelcomeMessageReviewActionProviderSupplier,
                        mProfile,
                        mTabListCoordinatorSupplier);
    }

    /**
     * Triggers an animation where a set of tabs merge into a single target tab.
     *
     * @param targetTabId The ID of the tab that other tabs will merge into.
     * @param tabIdsToMerge The IDs for all tabs that will be merged into the target tab.
     * @param onAnimationEnd Executed after the merge animation has finished.
     */
    private void translateStartMergeAnimation(
            @TabId int targetTabId, List<@TabId Integer> tabIdsToMerge, Runnable onAnimationEnd) {
        TabListCoordinator oldTabListCoordinator = mTabListCoordinatorSupplier.get();
        if (oldTabListCoordinator == null) return;

        TabListCoordinator tabListCoordinator = assumeNonNull(mTabListCoordinatorSupplier.get());
        int targetTabIndex = tabListCoordinator.getTabIndexFromTabId(targetTabId);
        List<Integer> tabIndexesToMerge =
                tabListCoordinator.getCardIndexesFromTabIds(tabIdsToMerge);
        tabListCoordinator.triggerMergeAnimation(targetTabIndex, tabIndexesToMerge, onAnimationEnd);
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
        if (mPriceWelcomeMessageController != null) {
            mPriceWelcomeMessageController.invalidate();
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
        if (mPriceWelcomeMessageController != null) {
            mPriceWelcomeMessageController.destroy();
        }

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

    public @Nullable TabGroupSuggestionMessageService getTabGroupSuggestionMessageService() {
        return mTabGroupSuggestionMessageService;
    }

    public void appendNextMessage(@MessageType int messageType) {
        assert mMessageCardProviderCoordinator != null;
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator == null) return;

        Message<@MessageType Integer> nextMessage =
                mMessageCardProviderCoordinator.getNextMessageItemForType(messageType);
        if (nextMessage == null || !shouldAppendMessage(nextMessage)) return;
        switch (messageType) {
            case MessageType.PRICE_MESSAGE -> tabListCoordinator.addSpecialListItem(
                    tabListCoordinator.getPriceWelcomeMessageInsertionIndex(),
                    UiType.PRICE_MESSAGE,
                    nextMessage.model);
            case MessageType.ARCHIVED_TABS_MESSAGE -> tabListCoordinator.addSpecialListItem(
                    0, UiType.ARCHIVED_TABS_MESSAGE, nextMessage.model);
            default -> tabListCoordinator.addSpecialListItem(
                    tabListCoordinator.getTabListModelSize(),
                    messageTypeToUiType(messageType),
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
        List<MessageService<@MessageType Integer, @UiType Integer>> messageServices =
                mMessageCardProviderCoordinator.getMessageServices();
        for (MessageService<@MessageType Integer, @UiType Integer> service : messageServices) {
            Message<@MessageType Integer> message = service.getNextMessageItem();
            if (message == null || !shouldAppendMessage(message)) continue;

            @MessageType int messageType = message.type;
            switch (messageType) {
                case MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE -> {
                    if (!mayAddIncognitoReauthPromoCard(message.model)) {
                        // Skip incrementing index if the message was not added.
                        continue;
                    }
                }
                    // Always add the archived tabs message to the start.
                case MessageType.ARCHIVED_TABS_MESSAGE -> tabListCoordinator.addSpecialListItem(
                        0, UiType.ARCHIVED_TABS_MESSAGE, message.model);
                default -> tabListCoordinator.addSpecialListItem(
                        index, messageTypeToUiType(messageType), message.model);
            }
            index++;
            sAppendedMessagesForTesting = true;
        }

        for (MessageUpdateObserver observer : mObservers) {
            observer.onAppendedMessage();
        }
    }

    private boolean mayAddIncognitoReauthPromoCard(PropertyModel model) {
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        assert tabListCoordinator != null;
        assumeNonNull(mIncognitoReauthPromoMessageService);
        if (mIncognitoReauthPromoMessageService.isIncognitoReauthPromoMessageEnabled(
                assumeNonNull(mProfile))) {
            tabListCoordinator.addSpecialListItem(
                    tabListCoordinator.getTabListModelSize(),
                    UiType.INCOGNITO_REAUTH_PROMO_MESSAGE,
                    model);
            mIncognitoReauthPromoMessageService.increasePromoShowCountAndMayDisableIfCountExceeds();
            return true;
        }
        return false;
    }

    private boolean shouldAppendMessage(Message<@MessageType Integer> message) {
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        assert tabListCoordinator != null;
        if (tabListCoordinator.specialItemExists(message.type)) return false;
        PropertyModel messageModel = message.model;

        @MessageCardScope
        int scope =
                messageModel.get(
                        MessageCardViewProperties
                                .MESSAGE_CARD_VISIBILITY_CONTROL_IN_REGULAR_AND_INCOGNITO_MODE);

        if (scope == MessageCardScope.BOTH) return true;

        TabGroupModelFilter filter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(filter);
        return filter.getTabModel().isIncognito()
                ? scope == MessageCardScope.INCOGNITO
                : scope == MessageCardScope.REGULAR;
    }

    /**
     * Remove all the message items in the model list. Right now this is used when all tabs are
     * closed in the grid tab switcher.
     */
    private void removeAllAppendedMessage() {
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator == null) return;

        tabListCoordinator.removeSpecialListItem(UiType.IPH_MESSAGE, MessageType.IPH);
        tabListCoordinator.removeSpecialListItem(UiType.PRICE_MESSAGE, MessageType.PRICE_MESSAGE);
        tabListCoordinator.removeSpecialListItem(
                UiType.INCOGNITO_REAUTH_PROMO_MESSAGE, MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE);
        tabListCoordinator.removeSpecialListItem(
                UiType.ARCHIVED_TABS_MESSAGE, MessageType.ARCHIVED_TABS_MESSAGE);

        // TODO(crbug.com/441040016): Refactor the lifecycle of the TabGroupSuggestionMessageService
        // so that we don't need to pass a dismiss runnable.
        if (mTabGroupSuggestionMessageService != null) {
            mTabGroupSuggestionMessageService.dismissMessage(
                    tabListCoordinator.getTabListHighlighter()::unhighlightTabs);
        }

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
        assert getCurrentProfile() != null;

        sAppendedMessagesForTesting = false;
        List<MessageService<@MessageType Integer, @UiType Integer>> messageServices =
                mMessageCardProviderCoordinator.getMessageServices();
        for (MessageService<@MessageType Integer, @UiType Integer> service : messageServices) {
            Message<@MessageType Integer> message = service.getNextMessageItem();
            if (message == null || !shouldAppendMessage(message)) continue;

            // The restore of PRICE_MESSAGE is handled in the restorePriceWelcomeMessage() below.
            PropertyModel model = message.model;
            @MessageType int msgType = message.type;
            switch (msgType) {
                case MessageType.PRICE_MESSAGE, MessageType.TAB_GROUP_SUGGESTION_MESSAGE -> {}
                case MessageType.ARCHIVED_TABS_MESSAGE -> tabListCoordinator.addSpecialListItem(
                        0, UiType.ARCHIVED_TABS_MESSAGE, model);
                default -> tabListCoordinator.addSpecialListItem(
                        tabListCoordinator.getTabListModelSize(),
                        messageTypeToUiType(msgType),
                        model);
            }
            sAppendedMessagesForTesting = true;
        }

        for (MessageUpdateObserver observer : mObservers) {
            observer.onRestoreAllAppendedMessage();
        }
    }

    @VisibleForTesting
    void dismissHandler(@MessageType int messageType) {
        // `bind` and `unbind` to attach to a `TabListCoordinator` are independent of whether the
        // `MessageCardProviderCoordinator`, has access to the `dismissHandler`. That means this
        // may be called while unbound by one of the message services.
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator == null) return;

        tabListCoordinator.removeSpecialListItem(messageTypeToUiType(messageType), messageType);
        if (messageType != MessageType.PRICE_MESSAGE
                && messageType != MessageType.INCOGNITO_REAUTH_PROMO_MESSAGE
                && messageType != MessageType.ARCHIVED_TABS_MESSAGE) {
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

    private void addTabGroupSuggestionMessage(@TabId int tabId) {
        @MessageType int messageType = MessageType.TAB_GROUP_SUGGESTION_MESSAGE;

        assert mMessageCardProviderCoordinator != null;
        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator == null) return;

        Message<@MessageType Integer> nextMessage =
                mMessageCardProviderCoordinator.getNextMessageItemForType(messageType);
        if (nextMessage == null || !shouldAppendMessage(nextMessage)) return;

        int index = tabListCoordinator.getIndexFromTabId(tabId);
        if (index == TabModel.INVALID_TAB_INDEX) return;

        tabListCoordinator.addSpecialListItem(
                index + 1, UiType.TAB_GROUP_SUGGESTION_MESSAGE, nextMessage.model);
        for (MessageUpdateObserver observer : mObservers) {
            observer.onAppendedMessage();
        }
    }

    public @Nullable PriceWelcomeMessageController getPriceWelcomeMessageController() {
        return mPriceWelcomeMessageController;
    }

    private void removeMessagesIfTabModelEmpty(int numTabsToRemove) {
        TabGroupModelFilter tabGroupModelFilter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(tabGroupModelFilter);
        if (tabGroupModelFilter.getTabModel().getCount() == numTabsToRemove) {
            removeAllAppendedMessage();
        }
    }

    private Profile getCurrentProfile() {
        TabGroupModelFilter tabGroupModelFilter = mCurrentTabGroupModelFilterSupplier.get();
        assumeNonNull(tabGroupModelFilter);
        return assumeNonNull(tabGroupModelFilter.getTabModel().getProfile());
    }
}
