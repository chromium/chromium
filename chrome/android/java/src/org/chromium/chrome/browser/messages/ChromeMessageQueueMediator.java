// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import androidx.annotation.Nullable;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager;
import org.chromium.chrome.browser.fullscreen.FullscreenManager.Observer;
import org.chromium.chrome.browser.fullscreen.FullscreenOptions;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageQueueDelegate;
import org.chromium.ui.modaldialog.ModalDialogManager;
import org.chromium.ui.modaldialog.ModalDialogManager.ModalDialogManagerObserver;
import org.chromium.ui.modelutil.PropertyModel;
import org.chromium.ui.util.TokenHolder;

/**
 * A glue class in chrome side to suspend and resume the queue. This is able
 * to observe the full screen mode and control the visibility of browser control in order to
 * suspend and resume the queue.
 */
public class ChromeMessageQueueMediator implements MessageQueueDelegate {
    private ManagedMessageDispatcher mQueueController;
    private MessageContainerCoordinator mContainerCoordinator;
    private BrowserControlsManager mBrowserControlsManager;
    private FullscreenManager mFullscreenManager;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
    private BrowserControlsObserver mBrowserControlsObserver;
    @Nullable
    private LayoutStateProvider mLayoutStateProvider;
    @Nullable
    private ActivityTabProvider mActivityTabProvider;
    @Nullable
    private ModalDialogManager mModalDialogManager;

    // TODO(crbug.com/1192907): Remove logic that suspends message queue on entering fullscreen
    // mode.
    private FullscreenManager.Observer mFullScreenObserver = new Observer() {
        private int mToken = TokenHolder.INVALID_TOKEN;
        @Override
        public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
            mToken = suspendQueue();
        }

        @Override
        public void onExitFullscreen(Tab tab) {
            resumeQueue(mToken);
        }
    };

    private LayoutStateObserver mLayoutStateObserver = new LayoutStateObserver() {
        private int mToken = TokenHolder.INVALID_TOKEN;

        // Suspend the queue until browsing mode is visible.
        @Override
        public void onStartedShowing(@LayoutType int layoutType, boolean showToolbar) {
            if (mToken == TokenHolder.INVALID_TOKEN && layoutType != LayoutType.BROWSING) {
                mToken = suspendQueue();
            }
        }

        @Override
        public void onFinishedShowing(@LayoutType int layoutType) {
            if (mToken != TokenHolder.INVALID_TOKEN && layoutType == LayoutType.BROWSING) {
                resumeQueue(mToken);
                mToken = TokenHolder.INVALID_TOKEN;
            }
        }
    };

    private ModalDialogManagerObserver mModalDialogManagerObserver =
            new ModalDialogManagerObserver() {
                private int mToken = TokenHolder.INVALID_TOKEN;

                @Override
                public void onDialogAdded(PropertyModel model) {
                    if (mToken == TokenHolder.INVALID_TOKEN) {
                        mToken = suspendQueue();
                    }
                }

                @Override
                public void onLastDialogDismissed() {
                    if (mToken != TokenHolder.INVALID_TOKEN) {
                        resumeQueue(mToken);
                        mToken = TokenHolder.INVALID_TOKEN;
                    }
                }
            };

    /**
     * @param browserControlsManager The browser controls manager able to toggle the visibility of
     *                               browser controls.
     * @param messageContainerCoordinator The coordinator able to show and hide message container.
     * @param fullscreenManager The full screen manager able to notify the fullscreen mode change.
     * @param activityTabProvider The {@link ActivityTabProvider} to get current tab of activity.
     * @param layoutStateProviderOneShotSupplier Supplier of the {@link LayoutStateProvider}.
     * @param modalDialogManagerSupplier Supplier of the {@link ModalDialogManager}.
     * @param messageDispatcher The {@link ManagedMessageDispatcher} able to suspend/resume queue.
     */
    public ChromeMessageQueueMediator(BrowserControlsManager browserControlsManager,
            MessageContainerCoordinator messageContainerCoordinator,
            FullscreenManager fullscreenManager, ActivityTabProvider activityTabProvider,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneShotSupplier,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            ManagedMessageDispatcher messageDispatcher) {
        mBrowserControlsManager = browserControlsManager;
        mContainerCoordinator = messageContainerCoordinator;
        mFullscreenManager = fullscreenManager;
        mQueueController = messageDispatcher;
        mActivityTabProvider = activityTabProvider;
        mFullscreenManager.addObserver(mFullScreenObserver);
        mBrowserControlsObserver = new BrowserControlsObserver();
        mBrowserControlsManager.addObserver(mBrowserControlsObserver);
        layoutStateProviderOneShotSupplier.onAvailable(this::setLayoutStateProvider);
        modalDialogManagerSupplier.addObserver(this::setModalDialogManager);
    }

    public void destroy() {
        mFullscreenManager.removeObserver(mFullScreenObserver);
        mBrowserControlsManager.removeObserver(mBrowserControlsObserver);
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        if (mModalDialogManager != null) {
            mModalDialogManager.removeObserver(mModalDialogManagerObserver);
        }
        mActivityTabProvider = null;
        mLayoutStateProvider = null;
        mQueueController = null;
        mContainerCoordinator = null;
        if (mBrowserControlsToken != TokenHolder.INVALID_TOKEN) {
            mBrowserControlsManager.getBrowserVisibilityDelegate().releasePersistentShowingToken(
                    mBrowserControlsToken);
        }
        mBrowserControlsManager = null;
        mFullscreenManager = null;
        mModalDialogManager = null;
    }

    @Override
    public void onStartShowing(Runnable runnable) {
        mBrowserControlsToken =
                mBrowserControlsManager.getBrowserVisibilityDelegate().showControlsPersistent();
        mContainerCoordinator.showMessageContainer();
        final Tab tab = mActivityTabProvider.get();
        if (TabBrowserControlsConstraintsHelper.getConstraints(tab) == BrowserControlsState.HIDDEN
                || BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager)) {
            runnable.run();
        } else {
            mBrowserControlsObserver.setOneTimeRunnableOnControlsFullyVisible(runnable);
        }
    }

    @Override
    public void onFinishHiding() {
        if (mBrowserControlsManager == null) return;
        mBrowserControlsManager.getBrowserVisibilityDelegate().releasePersistentShowingToken(
                mBrowserControlsToken);
        mContainerCoordinator.hideMessageContainer();
    }

    /**
     * Suspend queue so that the queue will not show a new message until it is resumed.
     * @return A token of {@link TokenHolder} required when resuming the queue.
     */
    int suspendQueue() {
        return mQueueController.suspend();
    }

    /**
     * @param token The token generated by {@link #suspendQueue()}.
     */
    void resumeQueue(int token) {
        mQueueController.resume(token);
    }

    /**
     * @param layoutStateProvider The provider able to add observer to observe overview mode.
     */
    private void setLayoutStateProvider(LayoutStateProvider layoutStateProvider) {
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        mLayoutStateProvider = layoutStateProvider;
        if (layoutStateProvider == null) return;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
    }

    private void setModalDialogManager(ModalDialogManager modalDialogManager) {
        if (mModalDialogManager != null) {
            mModalDialogManager.removeObserver(mModalDialogManagerObserver);
        }
        mModalDialogManager = modalDialogManager;
        if (modalDialogManager == null) return;
        mModalDialogManager.addObserver(mModalDialogManagerObserver);
    }

    class BrowserControlsObserver implements BrowserControlsStateProvider.Observer {
        private Runnable mRunOnControlsFullyVisible;

        @Override
        public void onControlsOffsetChanged(int topOffset, int topControlsMinHeightOffset,
                int bottomOffset, int bottomControlsMinHeightOffset, boolean needsAnimate) {
            if (mRunOnControlsFullyVisible != null
                    && BrowserControlsUtils.areBrowserControlsFullyVisible(
                            mBrowserControlsManager)) {
                mRunOnControlsFullyVisible.run();
                mRunOnControlsFullyVisible = null;
            }
        }

        void setOneTimeRunnableOnControlsFullyVisible(Runnable runnable) {
            mRunOnControlsFullyVisible = runnable;
        }
    }
}
