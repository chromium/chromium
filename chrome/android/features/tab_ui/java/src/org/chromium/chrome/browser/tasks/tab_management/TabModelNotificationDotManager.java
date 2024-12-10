// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tasks.tab_management;

import androidx.annotation.NonNull;

import org.chromium.base.CallbackController;
import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.collaboration.messaging.MessagingBackendServiceFactory;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabmodel.TabModelUtils;
import org.chromium.components.collaboration.messaging.MessageUtils;
import org.chromium.components.collaboration.messaging.MessagingBackendService;
import org.chromium.components.collaboration.messaging.MessagingBackendService.PersistentMessageObserver;
import org.chromium.components.collaboration.messaging.PersistentMessage;
import org.chromium.components.collaboration.messaging.PersistentNotificationType;

import java.util.List;
import java.util.Optional;

/** Pushes whether a notification dot should be shown for a tab model. */
public class TabModelNotificationDotManager implements Destroyable {
    private final PersistentMessageObserver mPersistentMessageObserver =
            new PersistentMessageObserver() {
                @Override
                public void onMessagingBackendServiceInitialized() {
                    mMessagingBackendServiceInitialized = true;
                    computeUpdate();
                }

                @Override
                public void displayPersistentMessage(PersistentMessage message) {
                    if (message.type != PersistentNotificationType.DIRTY_TAB) return;

                    if (Boolean.TRUE.equals(mNotificationDotObservableSupplier.get())) return;

                    computeUpdate();
                }

                @Override
                public void hidePersistentMessage(PersistentMessage message) {
                    if (message.type != PersistentNotificationType.DIRTY_TAB) return;

                    if (Boolean.FALSE.equals(mNotificationDotObservableSupplier.get())) return;

                    computeUpdate();
                }
            };

    private final ObservableSupplierImpl<Boolean> mNotificationDotObservableSupplier =
            new ObservableSupplierImpl<>(false);
    private final CallbackController mCallbackController = new CallbackController();
    private final MessagingBackendService mMessagingBackendService;
    private final TabModel mTabModel;
    private boolean mTabModelSelectorInitialized;
    private boolean mMessagingBackendServiceInitialized;

    /**
     * Notification dot manager for the regular tab model. Should only be constructed post-native
     * initialization.
     *
     * @param tabModelSelector The tab model selector to use. Only the regular tab model is
     *     observed. However, the selector is needed to know when the tab model is initialized.
     */
    public TabModelNotificationDotManager(TabModelSelector tabModelSelector) {
        mTabModel = tabModelSelector.getModel(/* incognito= */ false);
        assert mTabModel != null : "TabModel & native should be initialized.";
        mMessagingBackendService =
                MessagingBackendServiceFactory.getForProfile(mTabModel.getProfile());
        mMessagingBackendService.addPersistentMessageObserver(mPersistentMessageObserver);
        TabModelUtils.runOnTabStateInitialized(
                tabModelSelector,
                mCallbackController.makeCancelable(
                        unused -> {
                            mTabModelSelectorInitialized = true;
                            computeUpdate();
                        }));
    }

    /**
     * Returns an {@link ObservableSupplier} that contains true when the notification dot should be
     * shown.
     */
    public @NonNull ObservableSupplier<Boolean> getNotificationDotObservableSupplier() {
        return mNotificationDotObservableSupplier;
    }

    @Override
    public void destroy() {
        mCallbackController.destroy();
        mMessagingBackendService.removePersistentMessageObserver(mPersistentMessageObserver);
    }

    private void computeUpdate() {
        if (!mMessagingBackendServiceInitialized || !mTabModelSelectorInitialized) return;

        boolean shouldShowNotificationDot = anyTabsInModelHaveDirtyBit();
        mNotificationDotObservableSupplier.set(shouldShowNotificationDot);
    }

    private boolean anyTabsInModelHaveDirtyBit() {
        List<PersistentMessage> messages =
                mMessagingBackendService.getMessages(
                        Optional.of(PersistentNotificationType.DIRTY_TAB));
        for (PersistentMessage message : messages) {
            int tabId = MessageUtils.extractTabId(message);
            if (tabId == Tab.INVALID_TAB_ID) continue;

            if (mTabModel.getTabById(tabId) != null) return true;
        }
        return false;
    }
}
