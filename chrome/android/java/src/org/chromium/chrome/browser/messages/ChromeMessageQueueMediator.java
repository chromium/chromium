// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import org.chromium.base.supplier.OneshotSupplier;
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
import org.chromium.components.messages.ManagedMessageDispatcher;
import org.chromium.components.messages.MessageQueueDelegate;
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
    private LayoutStateProvider mLayoutStateProvider;

    private FullscreenManager.Observer mFullScreenObserver = new Observer() {
        private int mToken = TokenHolder.INVALID_TOKEN;
        @Override
        public void onEnterFullscreen(Tab tab, FullscreenOptions options) {
            // TODO(crbug.com/1123947): may not suspend when displaying a permission request
            //                            message.
            mToken = suspendQueue();
        }

        @Override
        public void onExitFullscreen(Tab tab) {
            resumeQueue(mToken);
        }
    };

    private LayoutStateObserver mLayoutStateObserver = new LayoutStateObserver() {
        private int mToken = TokenHolder.INVALID_TOKEN;

        @Override
        public void onStartedShowing(int layoutType, boolean showToolbar) {
            if (layoutType == LayoutType.TAB_SWITCHER) {
                mToken = suspendQueue();
            }
        }

        @Override
        public void onFinishedHiding(int layoutType) {
            if (layoutType == LayoutType.TAB_SWITCHER) {
                resumeQueue(mToken);
            }
        }
    };

    /**
     * @param browserControlsManager The browser controls manager able to toggle the visibility of
     *                               browser controls.
     * @param messageContainerCoordinator The coordinator able to show and hide message container.
     * @param fullscreenManager The full screen manager able to notify the fullscreen mode change.
     * @param layoutStateProviderOneShotSupplier Supplier of the {@link LayoutStateProvider}.
     * @param messageDispatcher The {@link ManagedMessageDispatcher} able to suspend/resume queue.
     */
    public ChromeMessageQueueMediator(BrowserControlsManager browserControlsManager,
            MessageContainerCoordinator messageContainerCoordinator,
            FullscreenManager fullscreenManager,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneShotSupplier,
            ManagedMessageDispatcher messageDispatcher) {
        mBrowserControlsManager = browserControlsManager;
        mContainerCoordinator = messageContainerCoordinator;
        mFullscreenManager = fullscreenManager;
        mQueueController = messageDispatcher;
        mFullscreenManager.addObserver(mFullScreenObserver);
        mBrowserControlsObserver = new BrowserControlsObserver();
        mBrowserControlsManager.addObserver(mBrowserControlsObserver);
        layoutStateProviderOneShotSupplier.onAvailable(this::setLayoutStateProvider);
    }

    public void destroy() {
        mFullscreenManager.removeObserver(mFullScreenObserver);
        mBrowserControlsManager.removeObserver(mBrowserControlsObserver);
        if (mLayoutStateProvider != null) {
            mLayoutStateProvider.removeObserver(mLayoutStateObserver);
        }
        mLayoutStateProvider = null;
        mQueueController = null;
        mContainerCoordinator = null;
        mBrowserControlsManager = null;
        mFullscreenManager = null;
    }

    @Override
    public void onStartShowing(Runnable runnable) {
        mBrowserControlsToken =
                mBrowserControlsManager.getBrowserVisibilityDelegate().showControlsPersistent();
        mContainerCoordinator.showMessageContainer();
        if (BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager)) {
            runnable.run();
        } else {
            mBrowserControlsObserver.setOneTimeRunnableOnControlsFullyVisible(runnable);
        }
    }

    @Override
    public void onFinishHiding() {
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
        mLayoutStateProvider = layoutStateProvider;
        mLayoutStateProvider.addObserver(mLayoutStateObserver);
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
