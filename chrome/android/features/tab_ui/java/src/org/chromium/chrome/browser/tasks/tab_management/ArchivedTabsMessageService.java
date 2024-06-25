// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.ARCHIVE_TIME_DELTA_DAYS;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.CLICK_HANDLER;
import static org.chromium.chrome.browser.tasks.tab_management.ArchivedTabsCardViewProperties.NUMBER_OF_ARCHIVED_TABS;

import android.content.Context;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.view.ViewParent;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.Callback;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.tabmodel.ArchivedTabModelOrchestrator;
import org.chromium.chrome.browser.back_press.BackPressManager;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.tab.TabArchiveSettings;
import org.chromium.chrome.browser.tab_ui.TabContentManager;
import org.chromium.chrome.browser.tabmodel.TabCreator;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tasks.tab_management.MessageCardViewProperties.MessageCardScope;
import org.chromium.chrome.browser.tasks.tab_management.MessageService.MessageType;
import org.chromium.chrome.browser.tasks.tab_management.TabListCoordinator.TabListMode;
import org.chromium.chrome.browser.tasks.tab_management.TabListModel.CardProperties.ModelType;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcherMessageManager.MessageUpdateObserver;
import org.chromium.chrome.browser.ui.messages.snackbar.SnackbarManager;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.modelutil.PropertyModelChangeProcessor;

import java.util.concurrent.TimeUnit;

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

                    mArchivedTabModel = archivedTabModel;
                    mArchivedTabModel.getTabCountSupplier().addObserver(mTabCountObserver);

                    mCustomCardView =
                            LayoutInflater.from(mContext)
                                    .inflate(R.layout.archived_tabs_message_card_view, null);
                    mCustomCardModel =
                            new PropertyModel.Builder(ArchivedTabsCardViewProperties.ALL_KEYS)
                                    .with(
                                            CLICK_HANDLER,
                                            ArchivedTabsMessageService.this::openArchivedTabsDialog)
                                    .build();
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

    private final @NonNull Context mContext;
    private final @NonNull TabArchiveSettings mTabArchiveSettings;
    private final @NonNull ArchivedTabModelOrchestrator mArchivedTabModelOrchestrator;
    private final @NonNull BrowserControlsStateProvider mBrowserControlsStateProvider;
    private final @NonNull TabContentManager mTabContentManager;
    private final @TabListMode int mTabListMode;
    private final @NonNull ViewGroup mRootView;
    private final @NonNull SnackbarManager mSnackbarManager;
    private final @NonNull TabCreator mRegularTabCreator;
    private final @NonNull BackPressManager mBackPressManager;

    private ArchivedTabsDialogCoordinator mArchivedTabsDialogCoordinator;
    private TabModel mArchivedTabModel;
    private View mCustomCardView;
    private PropertyModel mCustomCardModel;
    private boolean mMessageSentToQueue;

    ArchivedTabsMessageService(
            @NonNull Context context,
            @NonNull ArchivedTabModelOrchestrator archivedTabModelOrchestrator,
            @NonNull BrowserControlsStateProvider browserControlStateProvider,
            @NonNull TabContentManager tabContentManager,
            @TabListMode int tabListMode,
            @NonNull ViewGroup rootView,
            @NonNull SnackbarManager snackbarManager,
            @NonNull TabCreator regularTabCreator,
            @NonNull BackPressManager backPressManager) {
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

        if (mArchivedTabModelOrchestrator.isTabModelInitialized()) {
            mArchivedTabModelOrchestratorObserver.onTabModelCreated(
                    mArchivedTabModelOrchestrator
                            .getTabModelSelector()
                            .getModel(/* incognito= */ false));
        } else {
            mArchivedTabModelOrchestrator.addObserver(mArchivedTabModelOrchestratorObserver);
        }
        mTabArchiveSettings = mArchivedTabModelOrchestrator.getTabArchiveSettings();
    }

    // CustomMessageCardViewProvider implementation.

    @Override
    public int getMessageType() {
        return MessageService.MessageType.ARCHIVED_TABS_MESSAGE;
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
    public void onRemoveAllAppendedMessage() {
        if (mCustomCardView == null) return;
        // When messages are removed, detach the custom view.
        ViewParent parent = mCustomCardView.getParent();
        if (parent != null) {
            ((ViewGroup) parent).removeView(mCustomCardView);
        }
    }

    // Private methods.

    @Override
    public void addObserver(MessageService.MessageObserver obs) {
        super.addObserver(obs);
        maybeSendMessageToQueue();
    }

    @VisibleForTesting
    void maybeSendMessageToQueue() {
        if (mMessageSentToQueue) return;
        if (mArchivedTabModel == null) return;
        if (mArchivedTabModel.getCount() <= 0) return;
        updateModelProperties();
        sendAvailabilityNotification(new ArchivedTabsMessageData(this));
        mMessageSentToQueue = true;
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
        mArchivedTabsDialogCoordinator.show();
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
                        mSnackbarManager,
                        mRegularTabCreator,
                        mBackPressManager);
    }

    private void updateModelProperties() {
        mCustomCardModel.set(NUMBER_OF_ARCHIVED_TABS, mArchivedTabModel.getCount());
        mCustomCardModel.set(ARCHIVE_TIME_DELTA_DAYS, getArchiveTimeDeltaInDays());
    }

    private int getArchiveTimeDeltaInDays() {
        return (int) TimeUnit.HOURS.toDays(mTabArchiveSettings.getArchiveTimeDeltaHours());
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
