// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.build.NullUtil.assumeNonNull;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ICON_HIGHLIGHTED;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.WIDTH;

import android.app.Activity;
import android.util.Size;
import android.view.ViewGroup;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.EnsuresNonNull;
import org.chromium.build.annotations.Initializer;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.hub.PaneManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabGroupModelFilter;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabProperties.UiType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.browser.ui.edge_to_edge.EdgeToEdgeController;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateManager;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.components.tab_group_sync.TabGroupSyncService;
import org.chromium.components.tab_group_sync.TabGroupUiActionHandler;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;

import java.util.List;
import java.util.function.Supplier;

/** A message service to surface information about archived tabs. */
@NullMarked
public class ArchivedTabsMessageService
        extends MessageService<@MessageType Integer, @UiType Integer>
        implements MessageUpdateObserver {

    /** Provides message data for the archived message card. */
    public static class ArchivedTabsMessageData {
        public final Runnable onClickRunnable;

        public ArchivedTabsMessageData(Runnable onClickRunnable) {
            this.onClickRunnable = onClickRunnable;
        }
    }

    private final ArchivedTabModelOrchestrator.Observer mArchivedTabModelOrchestratorObserver =
            this::tabModelCreated;

    private final Callback<Integer> mTabCountObserver =
            (tabCount) -> {
                updateModelProperties(tabCount);
                if (tabCount > 0) {
                    maybeSendMessageToQueue(tabCount);
                } else {
                    maybeInvalidatePreviouslySentMessage();
                }
            };

    /** When the settings change, the message subtitle may need to be updated. */
    private final TabArchiveSettings.Observer mTabArchiveSettingsObserver =
            new TabArchiveSettings.Observer() {
                @Override
                public void onSettingChanged() {
                    updateModelProperties(mTabCountSupplier.get());
                }
            };

    private final TabListItemSizeChangedObserver mTabListItemSizeChangedObserver =
            this::maybeResizeCard;

    private final Activity mActivity;
    private final ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private final BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final TabContentManager mTabContentManager;
    private final @TabListMode int mTabListMode;
    private final ViewGroup mRootView;
    private final SnackbarManager mSnackbarManager;
    private final TabCreator mRegularTabCreator;
    private final BackPressManager mBackPressManager;
    private final ModalDialogManager mModalDialogManager;
    private final Tracker mTracker;
    private final Runnable mAppendMessageRunnable;
    private final ObservableSupplier<@Nullable TabListCoordinator> mTabListCoordinatorSupplier;
    private final @Nullable DesktopWindowStateManager mDesktopWindowStateManager;
    private final ObservableSupplier<EdgeToEdgeController> mEdgeToEdgeSupplier;
    private final @Nullable TabGroupSyncService mTabGroupSyncService;
    private final Supplier<PaneManager> mPaneManagerSupplier;
    private final Supplier<TabGroupUiActionHandler> mTabGroupUiActionHandlerSupplier;
    private final ObservableSupplier<@Nullable TabGroupModelFilter>
            mCurrentTabGroupModelFilterSupplier;
    private final ObservableSupplier<Integer> mTabCountSupplier;
    private final Supplier<LayoutStateProvider> mLayoutStateProviderSupplier;
    private final LayoutStateObserver mLayoutStateObserver =
            new LayoutStateObserver() {
                @Override
                public void onStartedHiding(@LayoutType int layoutType) {
                    if (layoutType == LayoutType.TAB_SWITCHER) {
                        maybeDestroyArchivedTabsDialog();
                    }
                }
            };

    private TabArchiveSettings mTabArchiveSettings;
    private @Nullable ArchivedTabsDialogCoordinator mArchivedTabsDialogCoordinator;
    private TabModel mArchivedTabModel;
    private final PropertyModel mModel;
    private boolean mMessageSentToQueue;
    private OnTabSelectingListener mOnTabSelectingListener;
    private boolean mShowTwoStepIph;

    ArchivedTabsMessageService(
            Activity activity,
            ArchivedTabModelOrchestrator archivedTabModelOrchestrator,
            BrowserControlsStateProvider browserControlStateProvider,
            TabContentManager tabContentManager,
            @TabListMode int tabListMode,
            ViewGroup rootView,
            SnackbarManager snackbarManager,
            TabCreator regularTabCreator,
            BackPressManager backPressManager,
            ModalDialogManager modalDialogManager,
            Tracker tracker,
            Runnable appendMessageRunnable,
            ObservableSupplier<@Nullable TabListCoordinator> tabListCoordinatorSupplier,
            @Nullable DesktopWindowStateManager desktopWindowStateManager,
            ObservableSupplier<EdgeToEdgeController> edgeToEdgeSupplier,
            @Nullable TabGroupSyncService tabGroupSyncService,
            Supplier<PaneManager> paneManagerSupplier,
            Supplier<TabGroupUiActionHandler> tabGroupUiActionHandlerSupplier,
            ObservableSupplier<@Nullable TabGroupModelFilter> currentTabGroupModelFilterSupplier,
            Supplier<LayoutStateProvider> layoutStateProviderSupplier) {
        super(
                MessageType.ARCHIVED_TABS_MESSAGE,
                UiType.ARCHIVED_TABS_MESSAGE,
                R.layout.archived_tabs_message_card_view,
                ArchivedTabsCardViewBinder::bind);
        mActivity = activity;
        mArchivedTabModelOrchestrator = archivedTabModelOrchestrator;
        mBrowserControlsStateProvider = browserControlStateProvider;
        mTabContentManager = tabContentManager;
        mTabListMode = tabListMode;
        mRootView = rootView;
        mSnackbarManager = snackbarManager;
        mRegularTabCreator = regularTabCreator;
        mBackPressManager = backPressManager;
        mModalDialogManager = modalDialogManager;
        mTracker = tracker;
        mAppendMessageRunnable = appendMessageRunnable;
        mTabListCoordinatorSupplier = tabListCoordinatorSupplier;
        mDesktopWindowStateManager = desktopWindowStateManager;
        mEdgeToEdgeSupplier = edgeToEdgeSupplier;
        mTabGroupSyncService = tabGroupSyncService;
        mPaneManagerSupplier = paneManagerSupplier;
        mTabGroupUiActionHandlerSupplier = tabGroupUiActionHandlerSupplier;
        mCurrentTabGroupModelFilterSupplier = currentTabGroupModelFilterSupplier;
        mLayoutStateProviderSupplier = layoutStateProviderSupplier;
        var layoutStateProvider = mLayoutStateProviderSupplier.get();
        if (layoutStateProvider != null) {
            layoutStateProvider.addObserver(mLayoutStateObserver);
        }

        mTabListCoordinatorSupplier.addObserver(
                (tabListCoordinator) -> {
                    if (tabListCoordinator == null) return;
                    tabListCoordinator.addTabListItemSizeChangedObserver(
                            mTabListItemSizeChangedObserver);
                    if (!ChromeFeatureList.sTabArchivalDragDropAndroid.isEnabled()) return;

                    tabListCoordinator.setOnDropOnArchivalMessageCardEventListener(
                            tabId -> {
                                TabGroupModelFilter tabGroupModelFilter =
                                        currentTabGroupModelFilterSupplier.get();
                                assumeNonNull(tabGroupModelFilter);
                                Tab tab = tabGroupModelFilter.getTabModel().getTabById(tabId);

                                mArchivedTabModelOrchestrator
                                        .getTabArchiver()
                                        .archiveAndRemoveTabs(tabGroupModelFilter, List.of(tab));
                            });
                });
        ArchivedTabsMessageData data = new ArchivedTabsMessageData(this::openArchivedTabsDialog);
        mModel = ArchivedTabsCardViewBinder.createPropertyModel(data);

        // Capture this value immediately before it expires when the IPH is dismissed, which will
        // happen regardless of user behavior. The TabArchiveSettings tracks whether the main IPH
        // was followed. When that's true, the archived tabs message should be highlighted as part
        // of the 2-step IPH.
        mShowTwoStepIph = TabArchiveSettings.getIphShownThisSession();

        mTabCountSupplier = mArchivedTabModelOrchestrator.getTabCountSupplier();

        if (mArchivedTabModelOrchestrator.isTabModelInitialized()) {
            mArchivedTabModelOrchestratorObserver.onTabModelCreated(
                    assumeNonNull(mArchivedTabModelOrchestrator.getTabModelSelector())
                            .getModel(/* incognito= */ false));
        } else {
            mArchivedTabModelOrchestrator.addObserver(mArchivedTabModelOrchestratorObserver);
        }
    }

    @Initializer
    private void tabModelCreated(TabModel archivedTabModel) {
        mArchivedTabModelOrchestrator.removeObserver(mArchivedTabModelOrchestratorObserver);
        mTabArchiveSettings = mArchivedTabModelOrchestrator.getTabArchiveSettings();
        mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);
        assert mTabArchiveSettings != null;

        mArchivedTabModel = archivedTabModel;
        mTabCountSupplier.addObserver(mTabCountObserver);

        mModel.set(ICON_HIGHLIGHTED, mShowTwoStepIph);
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mTabArchiveSettings != null) {
            mTabArchiveSettings.removeObserver(mTabArchiveSettingsObserver);
        }

        if (mArchivedTabsDialogCoordinator != null) {
            mArchivedTabsDialogCoordinator.destroy();
        }

        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
        if (tabListCoordinator != null) {
            tabListCoordinator.removeTabListItemSizeChangedObserver(
                    mTabListItemSizeChangedObserver);
        }

        if (mTabCountSupplier != null) {
            mTabCountSupplier.removeObserver(mTabCountObserver);
        }

        var layoutStateProvider = mLayoutStateProviderSupplier.get();
        if (layoutStateProvider != null) {
            layoutStateProvider.removeObserver(mLayoutStateObserver);
        }
    }

    // MessageUpdateObserver implementation.

    @Override
    public void onAppendedMessage() {
        // When the two-step IPH is active, highlight the end icon.
        if (mShowTwoStepIph) {
            mShowTwoStepIph = false;
            // Reset this manually, in case the IPH wasn't dismissed for some reason.
            TabArchiveSettings.setIphShownThisSession(false);
            // Scrolling the recycler view only works when posted.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        TabListCoordinator tabListCoordinator = mTabListCoordinatorSupplier.get();
                        if (tabListCoordinator == null) return;
                        if (tabListCoordinator.specialItemExists(
                                MessageType.ARCHIVED_TABS_MESSAGE)) {
                            tabListCoordinator.setRecyclerViewPosition(
                                    new RecyclerViewPosition(0, 0));
                        }
                    });
        }
    }

    @Override
    public void addObserver(MessageService.MessageObserver<@MessageType Integer> obs) {
        super.addObserver(obs);
        maybeSendMessageToQueue(mTabCountSupplier.get());
    }

    @Initializer
    public void setOnTabSelectingListener(OnTabSelectingListener onTabSelectingListener) {
        mOnTabSelectingListener = onTabSelectingListener;
    }

    // Private methods.

    @VisibleForTesting
    void maybeSendMessageToQueue(int tabCount) {
        if (mMessageSentToQueue) return;
        if (mArchivedTabModel == null) return;
        if (mTabGroupSyncService == null) return;
        if (tabCount <= 0) return;
        updateModelProperties(tabCount);
        sendAvailabilityNotification((a, b) -> mModel);
        mMessageSentToQueue = true;
        mAppendMessageRunnable.run();
    }

    @VisibleForTesting
    void maybeInvalidatePreviouslySentMessage() {
        if (!mMessageSentToQueue) return;
        sendInvalidNotification();
        mMessageSentToQueue = false;
    }

    private void openArchivedTabsDialog() {
        if (mArchivedTabsDialogCoordinator == null) {
            createArchivedTabsDialogCoordinator();
        }
        mTracker.notifyEvent("android_tab_declutter_button_clicked");
        mArchivedTabsDialogCoordinator.show(mOnTabSelectingListener);
        mModel.set(ICON_HIGHLIGHTED, false);
    }

    @EnsuresNonNull("mArchivedTabsDialogCoordinator")
    private void createArchivedTabsDialogCoordinator() {
        mArchivedTabsDialogCoordinator =
                new ArchivedTabsDialogCoordinator(
                        mActivity,
                        mArchivedTabModelOrchestrator,
                        mBrowserControlsStateProvider,
                        mTabContentManager,
                        mTabListMode,
                        mRootView,
                        mRootView.findViewById(R.id.tab_switcher_view_holder),
                        mSnackbarManager,
                        mRegularTabCreator,
                        mBackPressManager,
                        mTabArchiveSettings,
                        mModalDialogManager,
                        mDesktopWindowStateManager,
                        mEdgeToEdgeSupplier,
                        mTabGroupSyncService,
                        mPaneManagerSupplier,
                        mTabGroupUiActionHandlerSupplier,
                        mCurrentTabGroupModelFilterSupplier);
    }

    private void updateModelProperties(int tabCount) {
        mModel.set(NUMBER_OF_ARCHIVED_TABS, tabCount);
    }

    private void maybeResizeCard(int spanCount, Size cardSize) {
        mModel.set(WIDTH, spanCount == 4 ? cardSize.getWidth() * 2 : MATCH_PARENT);
    }

    @SuppressWarnings("NullAway")
    private void maybeDestroyArchivedTabsDialog() {
        if (mArchivedTabsDialogCoordinator == null) return;
        mArchivedTabsDialogCoordinator.destroy();
        mArchivedTabsDialogCoordinator = null;
    }

    // Testing methods.

    PropertyModel getCustomCardModelForTesting() {
        return mModel;
    }

    ArchivedTabModelOrchestrator.Observer getArchivedTabModelOrchestratorObserverForTesting() {
        return mArchivedTabModelOrchestratorObserver;
    }

    void setArchivedTabsDialogCoordiantorForTesting(
            ArchivedTabsDialogCoordinator archivedTabsDialogCoordinator) {
        mArchivedTabsDialogCoordinator = archivedTabsDialogCoordinator;
    }

    @Nullable ArchivedTabsDialogCoordinator getArchivedTabsDialogCoordinatorForTesting() {
        return mArchivedTabsDialogCoordinator;
    }
}
