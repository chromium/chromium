// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static android.view.ViewGroup.LayoutParams.MATCH_PARENT;

import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ARCHIVE_TIME_DELTA_DAYS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.WIDTH;

import android.content.Context;
import android.util.Size;
import android.view.LayoutInflater;
import android.view.View;
import android.view.View.OnAttachStateChangeListener;
import android.view.ViewGroup;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.OnTabSelectingListener;
import org.chromium.chrome.browser.tab_ui.RecyclerViewPosition;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MessageCardScope;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListItemSizeChangedObserver;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.components.browser_ui.desktop_windowing.DesktopWindowStateProvider;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightParams;
import org.chromium.components.browser_ui.widget.highlight.ViewHighlighter.HighlightShape;
import org.chromium.components.feature_engagement.Tracker;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

/** A message service to surface information about archived tabs. */
public class ArchivedTabsMessageService extends MessageService
        implements CustomMessageCardProvider, MessageUpdateObserver {

    static class ArchivedTabsMessageData implements MessageService.CustomMessageData {
        private CustomMessageCardProvider mProvider;

        public ArchivedTabsMessageData(CustomMessageCardProvider provider) {
            mProvider = provider;
        }

        @Override
        public CustomMessageCardProvider getProvider() {
            return mProvider;
        }
    }

    private final ArchivedTabModelOrchestrator.Observer mArchivedTabModelOrchestratorObserver =
            new ArchivedTabModelOrchestrator.Observer() {
                @Override
                public void onTabModelCreated(TabModel archivedTabModel) {
                    mArchivedTabModelOrchestrator.removeObserver(this);
                    mTabArchiveSettings = mArchivedTabModelOrchestrator.getTabArchiveSettings();
                    mTabArchiveSettings.addObserver(mTabArchiveSettingsObserver);
                    assert mTabArchiveSettings != null;

                    mArchivedTabModel = archivedTabModel;
                    mArchivedTabModel.getTabCountSupplier().addObserver(mTabCountObserver);

                    mCustomCardView =
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.archived_tabs_message_card_view, null);
                    if (mShowTwoStepIPH) {
                        mCustomCardView.addOnAttachStateChangeListener(
                                new OnAttachStateChangeListener() {
                                    @Override
                                    public void onViewAttachedToWindow(@NonNull View view) {
                                        HighlightParams params =
                                                new HighlightParams(HighlightShape.CIRCLE);
                                        params.setBoundsRespectPadding(false);
                                        ViewHighlighter.turnOnHighlight(mEndIconView, params);
                                        mCustomCardView.removeOnAttachStateChangeListener(this);
                                    }

                                    @Override
                                    public void onViewDetachedFromWindow(@NonNull View view) {}
                                });
                    }
                    mEndIconView = mCustomCardView.findViewById(R.id.end_image);
                    PropertyModelChangeProcessor.create(
                            mCustomCardModel, mCustomCardView, ArchivedTabsCardViewBinder::bind);
                }
            };

    private final Callback<Integer> mTabCountObserver =
            (count) -> {
                updateModelProperties();
                if (count > 0) {
                    maybeSendMessageToQueue();
                } else {
                    maybeInvalidatePreviouslySentMessage();
                }
            };

    /** When the settings change, the message subtitle may need to be updated. */
    private final TabArchiveSettings.Observer mTabArchiveSettingsObserver =
            new TabArchiveSettings.Observer() {
                @Override
                public void onSettingChanged() {
                    updateModelProperties();
                }
            };

    private final TabListItemSizeChangedObserver mTabListItemSizeChangedObserver =
            this::maybeResizeCard;

    private final @NonNull Context mContext;
    private final @NonNull ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private final @NonNull BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final @NonNull TabContentManager mTabContentManager;
    private final @TabListMode int mTabListMode;
    private final @NonNull ViewGroup mRootView;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull TabCreator mRegularTabCreator;
    private final @NonNull BackPressManager mBackPressManager;
    private final @NonNull ModalDialogManager mModalDialogManager;
    private final @NonNull Tracker mTracker;
    private final @NonNull Runnable mAppendMessageRunnable;
    private final @NonNull ObservableSupplier<TabListCoordinator> mTabListCoordinatorSupplier;
    private final @Nullable DesktopWindowStateProvider mDesktopWindowStateProvider;

    private TabArchiveSettings mTabArchiveSettings;
    private ArchivedTabsDialogCoordinator mArchivedTabsDialogCoordinator;
    private TabModel mArchivedTabModel;
    private View mCustomCardView;
    private View mEndIconView;
    private PropertyModel mCustomCardModel;
    private boolean mMessageSentToQueue;
    private OnTabSelectingListener mOnTabSelectingListener;
    private boolean mShowTwoStepIPH;

    ArchivedTabsMessageService(
            @NonNull Context context,
            @NonNull ArchivedTabModelOrchestrator archivedTabModelOrchestrator,
            @NonNull BrowserControlsStateProvider browserControlStateProvider,
            @NonNull TabContentManager tabContentManager,
            @TabListMode int tabListMode,
            @NonNull ViewGroup rootView,
            @NonNull SnackbarManager snackbarManager,
            @NonNull TabCreator regularTabCreator,
            @NonNull BackPressManager backPressManager,
            @NonNull ModalDialogManager modalDialogManager,
            @NonNull Tracker tracker,
            @NonNull Runnable appendMessageRunnable,
            @NonNull ObservableSupplier<TabListCoordinator> tabListCoordinatorSupplier,
            @Nullable DesktopWindowStateProvider desktopWindowStateProvider) {
        super(MessageType.ARCHIVED_TABS_MESSAGE);
        mContext = context;
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
        mDesktopWindowStateProvider = desktopWindowStateProvider;
        mTabListCoordinatorSupplier.addObserver(
                (tabListCoordinator) -> {
                    if (tabListCoordinator == null) return;
                    tabListCoordinator.addTabListItemSizeChangedObserver(
                            mTabListItemSizeChangedObserver);
                });
        mCustomCardModel =
                new PropertyModel.Builder(ArchivedTabsCardViewProperties.ALL_KEYS)
                        .with(
                                CLICK_HANDLER,
                                ArchivedTabsMessageService.this::openArchivedTabsDialog)
                        .build();
        // Capture this value immediately before it expires when the IPH is dismissed, which will
        // happen regardless of user behavior. The TabArchiveSettings tracks whether the main IPH
        // was followed. When that's true, the archived tabs message should be highlighted as part
        // of the 2-step IPH.
        mShowTwoStepIPH = TabArchiveSettings.getIphShownThisSession();

        if (mArchivedTabModelOrchestrator.isTabModelInitialized()) {
            mArchivedTabModelOrchestratorObserver.onTabModelCreated(
                    mArchivedTabModelOrchestrator
                            .getTabModelSelector()
                            .getModel(/* incognito= */ false));
        } else {
            mArchivedTabModelOrchestrator.addObserver(mArchivedTabModelOrchestratorObserver);
        }
    }

    public void destroy() {
        if (mTabArchiveSettings != null) {
            mTabArchiveSettings.removeObserver(mTabArchiveSettingsObserver);
        }

        if (mArchivedTabsDialogCoordinator != null) {
            mArchivedTabsDialogCoordinator.destroy();
        }

        if (mTabListCoordinatorSupplier.hasValue()) {
            mTabListCoordinatorSupplier
                    .get()
                    .removeTabListItemSizeChangedObserver(mTabListItemSizeChangedObserver);
        }
    }

    // CustomMessageCardViewProvider implementation.

    @Override
    public int getMessageType() {
        return MessageType.ARCHIVED_TABS_MESSAGE;
    }

    @Override
    public View getCustomView() {
        return mCustomCardView;
    }

    @Override
    public @MessageCardScope int getMessageCardVisibilityControl() {
        return MessageCardViewProperties.MessageCardScope.REGULAR;
    }

    @Override
    public @ModelType int getCardType() {
        return TabListModel.CardProperties.ModelType.MESSAGE;
    }

    @Override
    public void setIsIncognito(boolean isIncognito) {
        // No-op
    }

    // MessageUpdateObserver implementation.

    @Override
    public void onAppendedMessage() {
        // When the two-step IPH is active, highlight the end icon.
        if (mShowTwoStepIPH) {
            mShowTwoStepIPH = false;
            // Reset this manually, in case the IPH wasn't dismissed for some reason.
            TabArchiveSettings.setIphShownThisSession(false);
            // Scrolling the recycler view only works when posted.
            PostTask.postTask(
                    TaskTraits.UI_DEFAULT,
                    () -> {
                        if (!mTabListCoordinatorSupplier.hasValue()) return;
                        TabListCoordinator tabListCoordiantor = mTabListCoordinatorSupplier.get();
                        if (tabListCoordiantor.specialItemExists(
                                MessageType.ARCHIVED_TABS_MESSAGE)) {
                            mTabListCoordinatorSupplier
                                    .get()
                                    .setRecyclerViewPosition(new RecyclerViewPosition(0, 0));
                        }
                    });
        }
    }

    @Override
    public void addObserver(MessageService.MessageObserver obs) {
        super.addObserver(obs);
        maybeSendMessageToQueue();
    }

    public void setOnTabSelectingListener(OnTabSelectingListener onTabSelectingListener) {
        mOnTabSelectingListener = onTabSelectingListener;
    }

    // Private methods.

    @VisibleForTesting
    void maybeSendMessageToQueue() {
        if (mMessageSentToQueue) return;
        if (mArchivedTabModel == null) return;
        if (mArchivedTabModel.getCount() <= 0) return;
        updateModelProperties();
        sendAvailabilityNotification(new ArchivedTabsMessageData(this));
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
        ViewHighlighter.turnOffHighlight(mEndIconView);
    }

    private void createArchivedTabsDialogCoordinator() {
        mArchivedTabsDialogCoordinator =
                new ArchivedTabsDialogCoordinator(
                        mContext,
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
                        mDesktopWindowStateProvider);
    }

    private void updateModelProperties() {
        mCustomCardModel.set(NUMBER_OF_ARCHIVED_TABS, mArchivedTabModel.getCount());
        mCustomCardModel.set(
                ARCHIVE_TIME_DELTA_DAYS, mTabArchiveSettings.getArchiveTimeDeltaDays());
    }

    private void maybeResizeCard(int spanCount, Size cardSize) {
        mCustomCardModel.set(WIDTH, spanCount == 4 ? cardSize.getWidth() * 2 : MATCH_PARENT);
    }

    // Testing methods.

    PropertyModel getCustomCardModelForTesting() {
        return mCustomCardModel;
    }

    ArchivedTabModelOrchestrator.Observer getArchivedTabModelOrchestratorObserverForTesting() {
        return mArchivedTabModelOrchestratorObserver;
    }

    void setArchivedTabsDialogCoordiantorForTesting(
            ArchivedTabsDialogCoordinator archivedTabsDialogCoordinator) {
        mArchivedTabsDialogCoordinator = archivedTabsDialogCoordinator;
    }

    Callback<Integer> getTabCountObserverForTesting() {
        return mTabCountObserver;
    }
}
