// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Rect;
import android.view.View;
import android.view.ViewGroup;
import android.view.accessibility.AccessibilityEvent;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.recyclerview.widget.RecyclerView.ViewHolder;

import org.chromium.base.Callback;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.ChromeTabbedActivity;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.compositor.layouts.content.TabContentManager;
import org.chromium.chrome.browser.flags.CachedFeatureFlags;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.multiwindow.MultiWindowModeStateDispatcher;
import org.chromium.chrome.browser.price_tracking.PriceDropNotificationManager;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.tabmodel.TabCreatorManager;
import org.chromium.chrome.browser.tabmodel.TabList;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tasks.pseudotab.PseudoTab;
import org.chromium.chrome.browser.tasks.pseudotab.TabAttributeCache;
import org.chromium.chrome.browser.tasks.tab_management.PriceMessageService.PriceMessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabSelectionEditorCoordinator.TabSelectionEditorNavigationProvider;
import org.chromium.chrome.browser.tasks.tab_management.suggestions.TabSuggestionsOrchestrator;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.chrome.tab_ui.R;
import org.chromium.components.browser_ui.widget.MenuOrKeyboardActionController;
import org.chromium.components.browser_ui.widget.scrim.ScrimCoordinator;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.LayoutViewBuilder;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;
import org.chromium.ui.resources.dynamics.DynamicResourceLoader;

import java.util.List;

/**
 * Parent coordinator that is responsible for showing a grid or carousel of tabs for the main
 * TabSwitcher UI.
 */
public class TabSwitcherCoordinator
        implements Destroyable, TabSwitcher, TabSwitcher.TabListDelegate,
                   TabSwitcherMediator.ResetHandler, TabSwitcherMediator.MessageItemsController,
                   TabSwitcherMediator.PriceWelcomeMessageController {
    /**
     * Interface to control the IPH dialog.
     */
    interface IphController {
        /**
         * Show the dialog with IPH.
         */
        void showIph();
    }

    private class TabGroupManualSelectionMode {
        public final String actionString;
        public final int actionButtonDescriptionResourceId;
        public final int enablingThreshold;
        public final TabSelectionEditorActionProvider actionProvider;
        public final TabSelectionEditorCoordinator
                .TabSelectionEditorNavigationProvider navigationProvider;

        TabGroupManualSelectionMode(String actionString, int actionButtonDescriptionResourceId,
                int enablingThreshold, TabSelectionEditorActionProvider actionProvider,
                TabSelectionEditorNavigationProvider navigationProvider) {
            this.actionString = actionString;
            this.actionButtonDescriptionResourceId = actionButtonDescriptionResourceId;
            this.enablingThreshold = enablingThreshold;
            this.actionProvider = actionProvider;
            this.navigationProvider = navigationProvider;
        }
    }

    // TODO(crbug.com/982018): Rename 'COMPONENT_NAME' so as to add different metrics for carousel
    // tab switcher.
    static final String COMPONENT_NAME = "GridTabSwitcher";
    private static boolean sAppendedMessagesForTesting;
    private final PropertyModelChangeProcessor mContainerViewChangeProcessor;
    private final ActivityLifecycleDispatcher mLifecycleDispatcher;
    private final MenuOrKeyboardActionController mMenuOrKeyboardActionController;
    private final TabListCoordinator mTabListCoordinator;
    private final TabSwitcherMediator mMediator;
    private final MultiThumbnailCardProvider mMultiThumbnailCardProvider;
    private final TabGridDialogCoordinator mTabGridDialogCoordinator;
    private final TabModelSelector mTabModelSelector;
    private final @TabListCoordinator.TabListMode int mMode;
    private final MessageCardProviderCoordinator mMessageCardProviderCoordinator;
    private final MultiWindowModeStateDispatcher mMultiWindowModeStateDispatcher;

    private TabSelectionEditorCoordinator mTabSelectionEditorCoordinator;
    private TabGroupManualSelectionMode mTabGroupManualSelectionMode;
    private UndoGroupSnackbarController mUndoGroupSnackbarController;
    private TabSuggestionsOrchestrator mTabSuggestionsOrchestrator;
    private NewTabTileCoordinator mNewTabTileCoordinator;
    private TabAttributeCache mTabAttributeCache;
    private ViewGroup mContainer;
    private TabCreatorManager mTabCreatorManager;
    private boolean mIsInitialized;
    private PriceMessageService mPriceMessageService;
    private final ViewGroup mRootView;

    private final MenuOrKeyboardActionController
            .MenuOrKeyboardActionHandler mTabSwitcherMenuActionHandler =
            new MenuOrKeyboardActionController.MenuOrKeyboardActionHandler() {
                @Override
                public boolean handleMenuOrKeyboardAction(int id, boolean fromMenu) {
                    if (id == R.id.menu_group_tabs) {
                        assert mTabGroupManualSelectionMode != null;

                        mTabSelectionEditorCoordinator.getController().configureToolbar(
                                mTabGroupManualSelectionMode.actionString,
                                mTabGroupManualSelectionMode.actionButtonDescriptionResourceId,
                                mTabGroupManualSelectionMode.actionProvider,
                                mTabGroupManualSelectionMode.enablingThreshold,
                                mTabGroupManualSelectionMode.navigationProvider);

                        mTabSelectionEditorCoordinator.getController().show(
                                mTabModelSelector.getTabModelFilterProvider()
                                        .getCurrentTabModelFilter()
                                        .getTabsWithNoOtherRelatedTabs());
                        RecordUserAction.record("MobileMenuGroupTabs");
                        return true;
                    } else if (id == R.id.track_prices_row_menu_id) {
                        mPriceTrackingDialogCoordinator.show();
                        return true;
                    }
                    return false;
                }
            };
    private TabGridIphDialogCoordinator mTabGridIphDialogCoordinator;
    private PriceTrackingDialogCoordinator mPriceTrackingDialogCoordinator;

    public TabSwitcherCoordinator(Context context, ActivityLifecycleDispatcher lifecycleDispatcher,
            TabModelSelector tabModelSelector, TabContentManager tabContentManager,
            BrowserControlsStateProvider browserControls, TabCreatorManager tabCreatorManager,
            MenuOrKeyboardActionController menuOrKeyboardActionController, ViewGroup container,
            ObservableSupplier<ShareDelegate> shareDelegateSupplier,
            MultiWindowModeStateDispatcher multiWindowModeStateDispatcher,
            ScrimCoordinator scrimCoordinator, @TabListCoordinator.TabListMode int mode) {
        mMode = mode;
        mTabModelSelector = tabModelSelector;
        mContainer = container;
        mRootView = ((ChromeTabbedActivity) context).findViewById(R.id.coordinator);
        mTabCreatorManager = tabCreatorManager;
        mMultiWindowModeStateDispatcher = multiWindowModeStateDispatcher;

        PropertyModel containerViewModel = new PropertyModel(TabListContainerProperties.ALL_KEYS);

        mMediator = new TabSwitcherMediator(context, this, containerViewModel, tabModelSelector,
                browserControls, container, tabContentManager, this, this,
                multiWindowModeStateDispatcher, mode);

        mMultiThumbnailCardProvider =
                new MultiThumbnailCardProvider(context, tabContentManager, tabModelSelector);

        PseudoTab.TitleProvider titleProvider = tab -> {
            int numRelatedTabs = PseudoTab.getRelatedTabs(tab, tabModelSelector).size();
            if (numRelatedTabs == 1) return tab.getTitle();
            return context.getResources().getQuantityString(
                    R.plurals.bottom_tab_grid_title_placeholder, numRelatedTabs, numRelatedTabs);
        };

        mTabListCoordinator = new TabListCoordinator(mode, context, tabModelSelector,
                mMultiThumbnailCardProvider, titleProvider, true, mMediator, null,
                TabProperties.UiType.CLOSABLE, null, this, container, true, COMPONENT_NAME);
        mContainerViewChangeProcessor = PropertyModelChangeProcessor.create(containerViewModel,
                mTabListCoordinator.getContainerView(), TabListContainerViewBinder::bind);

        if (TabUiFeatureUtilities.isLaunchPolishEnabled()
                && TabUiFeatureUtilities.isTabGroupsAndroidContinuationEnabled()) {
            mMediator.addOverviewModeObserver(new OverviewModeObserver() {
                @Override
                public void startedShowing() {}

                @Override
                public void finishedShowing() {
                    if (!mTabModelSelector.isTabStateInitialized()) return;

                    int selectedIndex = mTabModelSelector.getTabModelFilterProvider()
                                                .getCurrentTabModelFilter()
                                                .index();
                    ViewHolder selectedViewHolder =
                            mTabListCoordinator.getContainerView().findViewHolderForAdapterPosition(
                                    selectedIndex);

                    if (selectedViewHolder == null) return;

                    View focusView = selectedViewHolder.itemView;
                    focusView.requestFocus();
                    focusView.sendAccessibilityEvent(AccessibilityEvent.TYPE_VIEW_FOCUSED);
                }

                @Override
                public void startedHiding() {}

                @Override
                public void finishedHiding() {}
            });
        }

        mMessageCardProviderCoordinator = new MessageCardProviderCoordinator(
                context, tabModelSelector::isIncognitoSelected, (identifier) -> {
                    if (identifier == MessageService.MessageType.PRICE_MESSAGE) {
                        mTabListCoordinator.removeSpecialListItem(
                                TabProperties.UiType.LARGE_MESSAGE, identifier);
                    } else {
                        mTabListCoordinator.removeSpecialListItem(
                                TabProperties.UiType.MESSAGE, identifier);
                        appendNextMessage(identifier);
                    }
                });

        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()) {
            mTabGridDialogCoordinator = new TabGridDialogCoordinator(context, tabModelSelector,
                    tabContentManager, tabCreatorManager, mRootView, this, mMediator,
                    this::getTabGridDialogAnimationSourceView, shareDelegateSupplier,
                    scrimCoordinator);
            mMediator.setTabGridDialogController(mTabGridDialogCoordinator.getDialogController());
        } else {
            mTabGridDialogCoordinator = null;
        }

        if (mode == TabListCoordinator.TabListMode.GRID) {
            if (shouldRegisterMessageItemType()) {
                mTabListCoordinator.registerItemType(TabProperties.UiType.MESSAGE,
                        new LayoutViewBuilder(R.layout.tab_grid_message_card_item),
                        MessageCardViewBinder::bind);
            }

            if (TabUiFeatureUtilities.isTabGridLayoutAndroidNewTabTileEnabled()) {
                mTabListCoordinator.registerItemType(TabProperties.UiType.NEW_TAB_TILE,
                        new LayoutViewBuilder(R.layout.new_tab_tile_card_item),
                        NewTabTileViewBinder::bind);
            }

            if (TabUiFeatureUtilities.isPriceTrackingEnabled()) {
                mTabListCoordinator.registerItemType(TabProperties.UiType.LARGE_MESSAGE,
                        new LayoutViewBuilder(R.layout.large_message_card_item),
                        LargeMessageCardViewBinder::bind);
            }
        }

        if (CachedFeatureFlags.isEnabled(ChromeFeatureList.INSTANT_START)
                || TabUiFeatureUtilities.ENABLE_SEARCH_CHIP.getValue()
                        && mode != TabListCoordinator.TabListMode.CAROUSEL) {
            mTabAttributeCache = new TabAttributeCache(mTabModelSelector);
        }

        mMenuOrKeyboardActionController = menuOrKeyboardActionController;
        mMenuOrKeyboardActionController.registerMenuOrKeyboardActionHandler(
                mTabSwitcherMenuActionHandler);

        mLifecycleDispatcher = lifecycleDispatcher;
        mLifecycleDispatcher.register(this);
    }

    @VisibleForTesting
    public static boolean hasAppendedMessagesForTesting() {
        return sAppendedMessagesForTesting;
    }

    @Override
    public void initWithNative(Context context, TabContentManager tabContentManager,
            DynamicResourceLoader dynamicResourceLoader,
            SnackbarManager.SnackbarManageable snackbarManageable,
            ModalDialogManager modalDialogManager) {
        if (mIsInitialized) return;

        setUpTabGroupManualSelectionMode(context, tabContentManager);

        mTabListCoordinator.initWithNative(dynamicResourceLoader);
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.initWithNative(context, mTabModelSelector, tabContentManager,
                    mTabListCoordinator.getTabGroupTitleEditor());
        }

        mMultiThumbnailCardProvider.initWithNative();

        if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()) {
            mUndoGroupSnackbarController =
                    new UndoGroupSnackbarController(context, mTabModelSelector, snackbarManageable);
        } else {
            mUndoGroupSnackbarController = null;
        }

        if (mMode == TabListCoordinator.TabListMode.GRID) {
            if (CachedFeatureFlags.isEnabled(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS)) {
                mTabSuggestionsOrchestrator =
                        new TabSuggestionsOrchestrator(mTabModelSelector, mLifecycleDispatcher);
                TabSuggestionMessageService tabSuggestionMessageService =
                        new TabSuggestionMessageService(context, mTabModelSelector,
                                mTabSelectionEditorCoordinator.getController());
                mTabSuggestionsOrchestrator.addObserver(tabSuggestionMessageService);
                mMessageCardProviderCoordinator.subscribeMessageService(
                        tabSuggestionMessageService);
            }

            if (TabUiFeatureUtilities.isTabGridLayoutAndroidNewTabTileEnabled()) {
                mNewTabTileCoordinator =
                        new NewTabTileCoordinator(mTabModelSelector, mTabCreatorManager);
            }

            if (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                    && !TabSwitcherMediator.isShowingTabsInMRUOrder()) {
                mTabGridIphDialogCoordinator =
                        new TabGridIphDialogCoordinator(context, mContainer, modalDialogManager);
                IphMessageService iphMessageService =
                        new IphMessageService(mTabGridIphDialogCoordinator);
                mMessageCardProviderCoordinator.subscribeMessageService(iphMessageService);
            }

            if (TabUiFeatureUtilities.isPriceTrackingEnabled()) {
                PriceDropNotificationManager notificationManager =
                        new PriceDropNotificationManager();
                mPriceTrackingDialogCoordinator = new PriceTrackingDialogCoordinator(
                        context, modalDialogManager, this, mTabModelSelector, notificationManager);
                mPriceMessageService = new PriceMessageService(
                        mTabListCoordinator, mMediator, notificationManager);
                mMessageCardProviderCoordinator.subscribeMessageService(mPriceMessageService);
                mMediator.setPriceMessageService(mPriceMessageService);
            }
        }
        mIsInitialized = true;
    }

    private void setUpTabGroupManualSelectionMode(
            Context context, TabContentManager tabContentManager) {
        // For tab switcher in carousel mode, the selection editor should still follow grid style.
        int selectionEditorMode = mMode == TabListCoordinator.TabListMode.CAROUSEL
                ? TabListCoordinator.TabListMode.GRID
                : mMode;
        mTabSelectionEditorCoordinator = new TabSelectionEditorCoordinator(
                context, mRootView, mTabModelSelector, tabContentManager, selectionEditorMode);
        mMediator.initWithNative(mTabSelectionEditorCoordinator.getController());

        mTabGroupManualSelectionMode = new TabGroupManualSelectionMode(
                context.getString(R.string.tab_selection_editor_group),
                R.plurals.accessibility_tab_selection_editor_group_button, 2,
                new TabSelectionEditorActionProvider(mTabSelectionEditorCoordinator.getController(),
                        TabSelectionEditorActionProvider.TabSelectionEditorAction.GROUP),
                new TabSelectionEditorNavigationProvider(
                        mTabSelectionEditorCoordinator.getController()));
    }

    // TabSwitcher implementation.
    @Override
    public void setOnTabSelectingListener(OnTabSelectingListener listener) {
        mMediator.setOnTabSelectingListener(listener);
    }

    @Override
    public Controller getController() {
        return mMediator;
    }

    @Override
    public TabListDelegate getTabListDelegate() {
        return this;
    }

    @Override
    public Supplier<Boolean> getTabGridDialogVisibilitySupplier() {
        return mTabGridDialogCoordinator::isVisible;
    }

    @Override
    public int getTabListTopOffset() {
        return mTabListCoordinator.getTabListTopOffset();
    }

    @Override
    public int getListModeForTesting() {
        return mMode;
    }

    @Override
    public boolean prepareOverview() {
        boolean quick = mMediator.prepareOverview();
        mTabListCoordinator.prepareOverview();
        return quick;
    }

    @Override
    public void postHiding() {
        mTabListCoordinator.postHiding();
        mMediator.postHiding();
    }

    @Override
    @NonNull
    public Rect getThumbnailLocationOfCurrentTab(boolean forceUpdate) {
        if (mTabGridDialogCoordinator != null && mTabGridDialogCoordinator.isVisible()) {
            assert forceUpdate;
            Rect thumbnail = mTabGridDialogCoordinator.getGlobalLocationOfCurrentThumbnail();
            // Adjust to the relative coordinate.
            Rect root = mTabListCoordinator.getRecyclerViewLocation();
            thumbnail.offset(-root.left, -root.top);
            return thumbnail;
        }
        if (forceUpdate) mTabListCoordinator.updateThumbnailLocation();
        return mTabListCoordinator.getThumbnailLocationOfCurrentTab();
    }

    // TabListDelegate implementation.
    @Override
    public int getResourceId() {
        return mTabListCoordinator.getResourceId();
    }

    @Override
    public long getLastDirtyTime() {
        return mTabListCoordinator.getLastDirtyTime();
    }

    @Override
    @VisibleForTesting
    public void setBitmapCallbackForTesting(Callback<Bitmap> callback) {
        TabListMediator.ThumbnailFetcher.sBitmapCallbackForTesting = callback;
    }

    @Override
    @VisibleForTesting
    public int getBitmapFetchCountForTesting() {
        return TabListMediator.ThumbnailFetcher.sFetchCountForTesting;
    }

    @Override
    @VisibleForTesting
    public int getSoftCleanupDelayForTesting() {
        return mMediator.getSoftCleanupDelayForTesting();
    }

    @Override
    @VisibleForTesting
    public int getCleanupDelayForTesting() {
        return mMediator.getCleanupDelayForTesting();
    }

    // ResetHandler implementation.
    @Override
    public boolean resetWithTabList(@Nullable TabList tabList, boolean quickMode, boolean mruMode) {
        return resetWithTabs(PseudoTab.getListOfPseudoTab(tabList), quickMode, mruMode);
    }

    @Override
    public boolean resetWithTabs(
            @Nullable List<PseudoTab> tabs, boolean quickMode, boolean mruMode) {
        mMediator.registerFirstMeaningfulPaintRecorder();
        // Invalidate price welcome message for every reset so that the stale message won't be
        // restored by mistake (e.g. from tabClosureUndone in TabSwitcherMediator).
        if (mPriceMessageService != null) {
            mPriceMessageService.invalidateMessage();
        }
        boolean showQuickly = mTabListCoordinator.resetWithListOfTabs(tabs, quickMode, mruMode);
        if (showQuickly) {
            mTabListCoordinator.removeSpecialListItem(TabProperties.UiType.NEW_TAB_TILE, 0);
            removeAllAppendedMessage();
        }

        int cardsCount = tabs == null ? 0 : tabs.size();
        if (tabs != null && mNewTabTileCoordinator != null) {
            mTabListCoordinator.addSpecialListItem(tabs.size(), TabProperties.UiType.NEW_TAB_TILE,
                    mNewTabTileCoordinator.getModel());
            cardsCount += 1;
        }

        if (tabs != null && tabs.size() > 0) {
            if (mPriceMessageService != null
                    && PriceTrackingUtilities.isPriceAlertsMessageCardEnabled()) {
                mPriceMessageService.preparePriceMessage(PriceMessageType.PRICE_ALERTS, null);
            }
            appendMessagesTo(cardsCount);
        }

        return showQuickly;
    }

    // MessageItemsController implementation.
    @Override
    public void removeAllAppendedMessage() {
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.MESSAGE, MessageService.MessageType.ALL);
        sAppendedMessagesForTesting = false;
    }

    @Override
    public void restoreAllAppendedMessage() {
        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            // The restore of PRICE_MESSAGE is handled in the restorePriceWelcomeMessage() below.
            if (messages.get(i).type == MessageService.MessageType.PRICE_MESSAGE) continue;
            mTabListCoordinator.addSpecialListItemToEnd(
                    TabProperties.UiType.MESSAGE, messages.get(i).model);
        }
        sAppendedMessagesForTesting = messages.size() > 0;
    }

    // PriceWelcomeMessageController implementation.
    @Override
    public void removePriceWelcomeMessage() {
        mTabListCoordinator.removeSpecialListItem(
                TabProperties.UiType.LARGE_MESSAGE, MessageService.MessageType.PRICE_MESSAGE);
    }

    @Override
    public void restorePriceWelcomeMessage() {
        appendNextMessage(MessageService.MessageType.PRICE_MESSAGE);
    }

    @Override
    public void showPriceWelcomeMessage(PriceMessageService.PriceTabData priceTabData) {
        if (mPriceMessageService == null
                || !PriceTrackingUtilities.isPriceWelcomeMessageCardEnabled()) {
            return;
        }
        mPriceMessageService.preparePriceMessage(PriceMessageType.PRICE_WELCOME, priceTabData);
        appendNextMessage(MessageService.MessageType.PRICE_MESSAGE);
        // To make the message card in view when user enters tab switcher, we should scroll to
        // current tab with 0 offset. See {@link TabSwitcherMediator#setInitialScrollIndexOffset}
        // for more details.
        mMediator.scrollToTab(
                mTabModelSelector.getTabModelFilterProvider().getCurrentTabModelFilter().index());
    }

    private void appendMessagesTo(int index) {
        if (mMultiWindowModeStateDispatcher.isInMultiWindowMode()) return;
        sAppendedMessagesForTesting = false;
        List<MessageCardProviderMediator.Message> messages =
                mMessageCardProviderCoordinator.getMessageItems();
        for (int i = 0; i < messages.size(); i++) {
            if (messages.get(i).type == MessageService.MessageType.PRICE_MESSAGE) {
                mTabListCoordinator.addSpecialListItem(
                        index, TabProperties.UiType.LARGE_MESSAGE, messages.get(i).model);
            } else {
                mTabListCoordinator.addSpecialListItem(
                        index, TabProperties.UiType.MESSAGE, messages.get(i).model);
            }
            index++;
        }
        if (messages.size() > 0) sAppendedMessagesForTesting = true;
    }

    private void appendNextMessage(@MessageService.MessageType int messageType) {
        assert mMessageCardProviderCoordinator != null;

        MessageCardProviderMediator.Message nextMessage =
                mMessageCardProviderCoordinator.getNextMessageItemForType(messageType);
        if (nextMessage == null) return;
        if (messageType == MessageService.MessageType.PRICE_MESSAGE) {
            mTabListCoordinator.addSpecialListItem(
                    mTabListCoordinator.getPriceWelcomeMessageInsertionIndex(),
                    TabProperties.UiType.LARGE_MESSAGE, nextMessage.model);
        } else {
            mTabListCoordinator.addSpecialListItemToEnd(
                    TabProperties.UiType.MESSAGE, nextMessage.model);
        }
    }

    private View getTabGridDialogAnimationSourceView(int tabId) {
        int index = mTabListCoordinator.indexOfTab(tabId);
        // TODO(crbug.com/999372): This is band-aid fix that will show basic fade-in/fade-out
        // animation when we cannot find the animation source view holder. This is happening due to
        // current group id in TabGridDialog can not be indexed in TabListModel, which should never
        // happen. Remove this when figure out the actual cause.
        ViewHolder sourceViewHolder =
                mTabListCoordinator.getContainerView().findViewHolderForAdapterPosition(index);
        if (sourceViewHolder == null) return null;
        return sourceViewHolder.itemView;
    }

    private boolean shouldRegisterMessageItemType() {
        return CachedFeatureFlags.isEnabled(ChromeFeatureList.CLOSE_TAB_SUGGESTIONS)
                || (TabUiFeatureUtilities.isTabGroupsAndroidEnabled()
                        && !TabSwitcherMediator.isShowingTabsInMRUOrder());
    }

    @Override
    public void softCleanup() {
        mTabListCoordinator.softCleanup();
    }

    // ResetHandler implementation.
    @Override
    public void destroy() {
        mMenuOrKeyboardActionController.unregisterMenuOrKeyboardActionHandler(
                mTabSwitcherMenuActionHandler);
        mTabListCoordinator.destroy();
        mMessageCardProviderCoordinator.destroy();
        mContainerViewChangeProcessor.destroy();
        if (mTabGridDialogCoordinator != null) {
            mTabGridDialogCoordinator.destroy();
        }
        if (mUndoGroupSnackbarController != null) {
            mUndoGroupSnackbarController.destroy();
        }
        if (mTabGridIphDialogCoordinator != null) {
            mTabGridIphDialogCoordinator.destroy();
        }
        if (mNewTabTileCoordinator != null) {
            mNewTabTileCoordinator.destroy();
        }
        mMultiThumbnailCardProvider.destroy();
        if (mTabSelectionEditorCoordinator != null) {
            mTabSelectionEditorCoordinator.destroy();
        }
        mMediator.destroy();
        mLifecycleDispatcher.unregister(this);
        if (mTabAttributeCache != null) {
            mTabAttributeCache.destroy();
        }
    }
}
