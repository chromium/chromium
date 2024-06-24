// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.messages;

import android.os.Handler;

import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.CallbackController;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.cc.input.BrowserControlsState;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsStateProvider;
import org.chromium.chrome.browser.browser_controls.BrowserControlsUtils;
import org.chromium.chrome.browser.fullscreen.BrowserControlsManager;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutStateProvider.LayoutStateObserver;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.PauseResumeWithNativeObserver;
import org.chromium.chrome.browser.omnibox.UrlFocusChangeListener;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabBrowserControlsConstraintsHelper;
import org.chromium.components.browser_ui.bottomsheet.BottomSheetController;
import org.chromium.components.browser_ui.bottomsheet.EmptyBottomSheetObserver;
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
public class ChromeMessageQueueMediator implements MessageQueueDelegate, UrlFocusChangeListener {
    private static final long QUEUE_RESUMPTION_ON_URL_UNFOCUS_WAIT_DURATION_MS = 1000;

    private ManagedMessageDispatcher mQueueController;
    private MessageContainerCoordinator mContainerCoordinator;
    private BrowserControlsManager mBrowserControlsManager;
    private int mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
    private BrowserControlsObserver mBrowserControlsObserver;
    @Nullable private LayoutStateProvider mLayoutStateProvider;
    @Nullable private ActivityTabProvider mActivityTabProvider;
    @Nullable private ModalDialogManager mModalDialogManager;
    private BottomSheetController mBottomSheetController;
    private ActivityLifecycleDispatcher mActivityLifecycleDispatcher;
    private final CallbackController mCallbackController = new CallbackController();
    private final ActivityTabProvider.ActivityTabTabObserver mActivityTabTabObserver;
    private int mUrlFocusToken = TokenHolder.INVALID_TOKEN;
    private Handler mQueueHandler;

    private boolean mIsDestroyed;

    private LayoutStateObserver mLayoutStateObserver =
            new LayoutStateObserver() {
                private int mToken = TokenHolder.INVALID_TOKEN;

                // Suspend the queue until browsing mode is visible.
                @Override
                public void onStartedShowing(@LayoutType int layoutType) {
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

    private PauseResumeWithNativeObserver mPauseResumeWithNativeObserver =
            new PauseResumeWithNativeObserver() {
                private int mToken = TokenHolder.INVALID_TOKEN;

                @Override
                public void onPauseWithNative() {
                    if (mToken == TokenHolder.INVALID_TOKEN) {
                        mToken = suspendQueue();
                    }
                }

                @Override
                public void onResumeWithNative() {
                    if (mToken != TokenHolder.INVALID_TOKEN) {
                        resumeQueue(mToken);
                        mToken = TokenHolder.INVALID_TOKEN;
                    }
                }
            };

    private EmptyBottomSheetObserver mBottomSheetObserver =
            new EmptyBottomSheetObserver() {
                private int mToken = TokenHolder.INVALID_TOKEN;

                @Override
                public void onSheetOpened(int reason) {
                    if (mToken == TokenHolder.INVALID_TOKEN) {
                        mToken = suspendQueue();
                    }
                }

                @Override
                public void onSheetClosed(int reason) {
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
     * @param activityTabProvider The {@link ActivityTabProvider} to get current tab of activity.
     * @param layoutStateProviderOneShotSupplier Supplier of the {@link LayoutStateProvider}.
     * @param modalDialogManagerSupplier Supplier of the {@link ModalDialogManager}.
     * @param bottomSheetController The {@link BottomSheetController} able to observe the
     *                              open/closed state of bottom sheets.
     * @param activityLifecycleDispatcher The dispatcher of activity life cycles.
     * @param messageDispatcher The {@link ManagedMessageDispatcher} able to suspend/resume queue.
     */
    public ChromeMessageQueueMediator(
            BrowserControlsManager browserControlsManager,
            MessageContainerCoordinator messageContainerCoordinator,
            ActivityTabProvider activityTabProvider,
            OneshotSupplier<LayoutStateProvider> layoutStateProviderOneShotSupplier,
            ObservableSupplier<ModalDialogManager> modalDialogManagerSupplier,
            BottomSheetController bottomSheetController,
            ActivityLifecycleDispatcher activityLifecycleDispatcher,
            ManagedMessageDispatcher messageDispatcher) {
        mBrowserControlsManager = browserControlsManager;
        mContainerCoordinator = messageContainerCoordinator;
        mQueueController = messageDispatcher;
        mActivityTabProvider = activityTabProvider;
        mBrowserControlsObserver = new BrowserControlsObserver();
        mBrowserControlsManager.addObserver(mBrowserControlsObserver);
        layoutStateProviderOneShotSupplier.onAvailable(
                mCallbackController.makeCancelable(this::setLayoutStateProvider));
        modalDialogManagerSupplier.addObserver(this::setModalDialogManager);
        mBottomSheetController = bottomSheetController;
        mBottomSheetController.addObserver(mBottomSheetObserver);
        mActivityLifecycleDispatcher = activityLifecycleDispatcher;
        activityLifecycleDispatcher.register(mPauseResumeWithNativeObserver);
        mQueueHandler = new Handler();
        mActivityTabTabObserver =
                new ActivityTabProvider.ActivityTabTabObserver(activityTabProvider, true) {
                    private int mToken = TokenHolder.INVALID_TOKEN;

                    @Override
                    protected void onObservingDifferentTab(Tab tab, boolean hint) {
                        if (mToken == TokenHolder.INVALID_TOKEN && tab == null) {
                            mToken = suspendQueue();
                        } else if (mToken != TokenHolder.INVALID_TOKEN && tab != null) {
                            resumeQueue(mToken);
                            mToken = TokenHolder.INVALID_TOKEN;
                        }
                    }
                };
    }

    public void destroy() {
        mIsDestroyed = true;
        mActivityLifecycleDispatcher.unregister(mPauseResumeWithNativeObserver);
        mActivityLifecycleDispatcher = null;
        mBottomSheetController.removeObserver(mBottomSheetObserver);
        mBottomSheetController = null;
        mCallbackController.destroy();
        mBrowserControlsManager.removeObserver(mBrowserControlsObserver);
        setLayoutStateProvider(null);
        setModalDialogManager(null);
        mActivityTabTabObserver.destroy();
        mActivityTabProvider = null;
        mQueueController = null;
        mContainerCoordinator = null;
        if (mBrowserControlsToken != TokenHolder.INVALID_TOKEN) {
            mBrowserControlsManager
                    .getBrowserVisibilityDelegate()
                    .releasePersistentShowingToken(mBrowserControlsToken);
        }
        mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
        mBrowserControlsManager = null;
        mUrlFocusToken = TokenHolder.INVALID_TOKEN;
        mQueueHandler.removeCallbacksAndMessages(null);
        mQueueHandler = null;
    }

    @Override
    public void onRequestShowing(Runnable runnable) {
        if (mBrowserControlsManager == null) return;
        if (mBrowserControlsToken != TokenHolder.INVALID_TOKEN) {
            // It is possible for #onRequestShowing to be invoked for a second message even after
            // the first message has acquired the browser controls token and is being displayed, if
            // the tab browser controls constraints state changes while browser controls is not
            // fully visible, before the second message is enqueued.
            assert !areBrowserControlsReady()
                    : "Should not be requested when browser controls is ready.";
            assert !mBrowserControlsObserver.isRequesting();
            mBrowserControlsObserver.setOneTimeRunnableOnControlsFullyVisible(runnable);
            return;
        }
        mBrowserControlsToken =
                mBrowserControlsManager.getBrowserVisibilityDelegate().showControlsPersistent();

        mContainerCoordinator.showMessageContainer();
        if (areBrowserControlsReady()) {
            mBrowserControlsObserver.setOneTimeRunnableOnControlsFullyVisible(null);
            runnable.run();
        } else {
            mBrowserControlsObserver.setOneTimeRunnableOnControlsFullyVisible(runnable);
        }
    }

    @Override
    public boolean isReadyForShowing() {
        return mBrowserControlsToken != TokenHolder.INVALID_TOKEN && areBrowserControlsReady();
    }

    @Override
    public boolean isPendingShow() {
        return mBrowserControlsObserver.isRequesting();
    }

    @Override
    public void onFinishHiding() {
        if (mBrowserControlsManager == null) return;
        mBrowserControlsManager
                .getBrowserVisibilityDelegate()
                .releasePersistentShowingToken(mBrowserControlsToken);
        mBrowserControlsToken = TokenHolder.INVALID_TOKEN;
        mContainerCoordinator.hideMessageContainer();
        mBrowserControlsObserver.setOneTimeRunnableOnControlsFullyVisible(null);
    }

    @Override
    public void onAnimationStart() {
        if (mContainerCoordinator == null) return;
        mContainerCoordinator.onAnimationStart();
    }

    @Override
    public void onAnimationEnd() {
        if (mContainerCoordinator == null) {
            assert mIsDestroyed;
            return;
        }
        mContainerCoordinator.onAnimationEnd();
    }

    @Override
    public boolean isDestroyed() {
        return mIsDestroyed;
    }

    @Override
    public boolean isSwitchingScope() {
        if (mActivityTabProvider == null) return false;
        final Tab tab = mActivityTabProvider.get();
        return tab != null && tab.isDestroyed();
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

    @VisibleForTesting
    boolean areBrowserControlsReady() {
        if (mIsDestroyed) return false;
        assert mActivityTabProvider != null;
        final Tab tab = mActivityTabProvider.get();
        if (tab == null || tab.isDestroyed()) return false;
        return TabBrowserControlsConstraintsHelper.getConstraints(tab)
                        == BrowserControlsState.HIDDEN
                || BrowserControlsUtils.areBrowserControlsFullyVisible(mBrowserControlsManager);
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
        // TODO(crbug.com/40761037): The crash is possible when #setLayoutStateProvider() is called
        // after #destroy() was called. This sequence of calls is unexpected. Below check throws an
        // exception to help identify the caller.
        if (mQueueController == null) {
            throw new IllegalStateException("setLayoutStateProvider() is called after destroy()");
        }
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

    @Override
    public void onUrlFocusChange(boolean hasFocus) {
        if (hasFocus) {
            if (mUrlFocusToken == TokenHolder.INVALID_TOKEN) {
                mUrlFocusToken = suspendQueue();
            }
            mQueueHandler.removeCallbacksAndMessages(null);
        } else {
            mQueueHandler.postDelayed(
                    () -> {
                        resumeQueue(mUrlFocusToken);
                        mUrlFocusToken = TokenHolder.INVALID_TOKEN;
                    },
                    QUEUE_RESUMPTION_ON_URL_UNFOCUS_WAIT_DURATION_MS);
        }
    }

    class BrowserControlsObserver implements BrowserControlsStateProvider.Observer {
        private Runnable mRunOnControlsFullyVisible;

        @Override
        public void onControlsOffsetChanged(
                int topOffset,
                int topControlsMinHeightOffset,
                int bottomOffset,
                int bottomControlsMinHeightOffset,
                boolean needsAnimate,
                boolean isVisibilityForced) {
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

        Runnable getRunnableForTesting() {
            return mRunOnControlsFullyVisible;
        }

        boolean isRequesting() {
            return mRunOnControlsFullyVisible != null;
        }
    }

    void setQueueHandlerForTesting(Handler handler) {
        mQueueHandler = handler;
    }

    int getUrlFocusTokenForTesting() {
        return mUrlFocusToken;
    }
}
