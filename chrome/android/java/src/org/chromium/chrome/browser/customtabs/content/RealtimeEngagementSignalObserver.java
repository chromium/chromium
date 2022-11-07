// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.graphics.Point;

import androidx.annotation.Nullable;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.MathUtils;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
import org.chromium.chrome.browser.tab.EmptyTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.RenderCoordinates;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Tab observer that sends engagement signal via the CCT service connection.
 */
public class RealtimeEngagementSignalObserver extends EmptyTabObserver {
    private final CustomTabsConnection mConnection;

    @Nullable
    private final CustomTabsSessionToken mSession;

    private GestureStateListener mGestureStateListener;
    private WebContentsObserver mEngagementSignalWebContentsObserver;
    @Nullable
    private ScrollState mScrollState;

    /**
     * A tab observer that will send real time scrolling signals to CustomTabsConnection, if a
     * active session exists.
     * @param tabObserverRegistrar See {@link
     *         BaseCustomTabActivityComponent#resolveTabObserverRegistrar()}.
     * @param initialTab The initial tab that this observer should attach to.
     * @param connection See {@link ChromeAppComponent#resolveCustomTabsConnection()}.
     * @param session See {@link CustomTabIntentDataProvider#getSession()}.
     */
    // TODO(https://crbug.com/1378410): Inject this class and implement NativeInitObserver.
    public RealtimeEngagementSignalObserver(TabObserverRegistrar tabObserverRegistrar,
            Tab initialTab, CustomTabsConnection connection,
            @Nullable CustomTabsSessionToken session) {
        mConnection = connection;
        mSession = session;

        // Do not register observer via tab#addObserver, so it can change tabs when necessary.
        tabObserverRegistrar.registerTabObserver(this);
        maybeStartSendingRealTimeEngagementSignals(initialTab);
    }

    // TabObserver
    @Override
    public void onContentChanged(Tab tab) {
        maybeStartSendingRealTimeEngagementSignals(tab);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        removeObserversFromWebContents(tab.getWebContents());
        super.onActivityAttachmentChanged(tab, window);
    }

    @Override
    public void webContentsWillSwap(Tab tab) {
        removeObserversFromWebContents(tab.getWebContents());
    }

    @Override
    public void onHidden(Tab tab, int reason) {
        if (reason == TabHidingType.ACTIVITY_HIDDEN) {
            collectUserInteraction(tab);
        }
    }

    @Override
    public void onDestroyed(Tab tab) {
        collectUserInteraction(tab);
        removeObserversFromWebContents(tab.getWebContents());
    }

    public void onFinishNativeInitialization() {
        mConnection.setGreatestScrollPercentageSupplier(
                () -> mScrollState != null ? mScrollState.mMaxReportedScrollPercentage : null);
    }

    /**
     * Create |mScrollState| and |mGestureStateListener| and start sending real-time engagement
     * signals through {@link androidx.browser.customtabs.CustomTabsCallback}.
     */
    private void maybeStartSendingRealTimeEngagementSignals(Tab tab) {
        if (!shouldSendEngagementSignal(tab)) return;

        if (mScrollState == null) mScrollState = new ScrollState(mConnection, mSession);
        if (mGestureStateListener == null) {
            mGestureStateListener = new GestureStateListener() {
                @Override
                public void onScrollStarted(
                        int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                    mScrollState.onScrollStarted(isDirectionUp);
                }

                @Override
                public void onScrollUpdateGestureConsumed(@Nullable Point rootScrollOffset) {
                    if (rootScrollOffset != null) {
                        RenderCoordinates renderCoordinates =
                                RenderCoordinates.fromWebContents(tab.getWebContents());
                        mScrollState.onScrollUpdate(
                                rootScrollOffset.y, renderCoordinates.getMaxVerticalScrollPixInt());
                    }
                }

                @Override
                public void onVerticalScrollDirectionChanged(
                        boolean directionUp, float currentScrollRatio) {
                    mScrollState.onScrollDirectionChanged(directionUp);
                }

                @Override
                public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                    mScrollState.onScrollEnded();
                }
            };
        }
        if (mEngagementSignalWebContentsObserver == null) {
            mEngagementSignalWebContentsObserver = new WebContentsObserver() {
                @Override
                public void navigationEntryCommitted(LoadCommittedDetails details) {
                    // TODO(https://crbug.com/1351026): Look into back navigation/scroll
                    // restoration to see if we need any changes to match PRD specs.
                    if (details.isMainFrame() && !details.isSameDocument()) {
                        mScrollState.resetMaxScrollPercentage();
                    }
                }
            };
        }

        GestureListenerManager gestureListenerManager =
                GestureListenerManager.fromWebContents(tab.getWebContents());
        if (!gestureListenerManager.hasListener(mGestureStateListener)) {
            gestureListenerManager.addListener(mGestureStateListener);
        }
        tab.getWebContents().addObserver(mEngagementSignalWebContentsObserver);
    }

    private void collectUserInteraction(Tab tab) {
        if (!shouldSendEngagementSignal(tab)) return;

        TabInteractionRecorder recorder = TabInteractionRecorder.getFromTab(tab);
        if (recorder == null) return;

        mConnection.notifyDidGetUserInteraction(mSession, recorder.didGetUserInteraction());
    }

    private void removeObserversFromWebContents(@Nullable WebContents webContents) {
        if (webContents == null) return;

        if (mGestureStateListener != null) {
            GestureListenerManager.fromWebContents(webContents)
                    .removeListener(mGestureStateListener);
        }
        if (mEngagementSignalWebContentsObserver != null) {
            webContents.removeObserver(mEngagementSignalWebContentsObserver);
        }
    }

    private boolean shouldSendEngagementSignal(Tab tab) {
        return tab != null && tab.getWebContents() != null
                && !tab.isIncognito()
                // Do not report engagement signals if user does not consent to report usage.
                && PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted();
    }

    private static class ScrollState {
        boolean mIsScrollActive;
        boolean mIsDirectionUp;
        int mMaxScrollPercentage;
        int mMaxReportedScrollPercentage;

        private final CustomTabsConnection mConnection;
        private final CustomTabsSessionToken mSession;

        ScrollState(CustomTabsConnection connection, CustomTabsSessionToken session) {
            mConnection = connection;
            mSession = session;
        }

        void onScrollStarted(boolean isDirectionUp) {
            assert !mIsScrollActive;
            mIsScrollActive = true;
            mIsDirectionUp = isDirectionUp;
            mConnection.notifyVerticalScrollEvent(mSession, mIsDirectionUp);
        }

        void onScrollUpdate(int verticalScrollOffset, int maxVerticalScrollOffset) {
            if (mIsScrollActive) {
                int scrollPercentage =
                        Math.round(((float) verticalScrollOffset / maxVerticalScrollOffset) * 100);
                scrollPercentage = MathUtils.clamp(scrollPercentage, 0, 100);
                if (scrollPercentage > mMaxScrollPercentage) {
                    mMaxScrollPercentage = scrollPercentage;
                }
            }
        }

        void onScrollDirectionChanged(boolean isDirectionUp) {
            if (mIsScrollActive && isDirectionUp != mIsDirectionUp) {
                mIsDirectionUp = isDirectionUp;
                mConnection.notifyVerticalScrollEvent(mSession, mIsDirectionUp);
            }
        }

        void onScrollEnded() {
            int maxScrollPercentageFivesMultiple =
                    mMaxScrollPercentage - (mMaxScrollPercentage % 5);
            if (maxScrollPercentageFivesMultiple > mMaxReportedScrollPercentage) {
                mMaxReportedScrollPercentage = maxScrollPercentageFivesMultiple;
                mConnection.notifyGreatestScrollPercentageIncreased(
                        mSession, mMaxReportedScrollPercentage);
            }
            mIsScrollActive = false;
        }

        void resetMaxScrollPercentage() {
            mMaxScrollPercentage = 0;
            mMaxReportedScrollPercentage = 0;
        }
    }
}
