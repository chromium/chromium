// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import android.graphics.Point;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.annotation.VisibleForTesting;
import androidx.browser.customtabs.CustomTabsSessionToken;

import org.chromium.base.MathUtils;
import org.chromium.base.UserData;
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
 * Tab observer that tracks and sends engagement signal via the CCT service connection. The
 * engagement signals are sticky between tab switching (given the assumption that only one
 * meaningful active tab will live in CCT).
 */
class RealtimeEngagementSignalObserver extends EmptyTabObserver {
    private static final int SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING = -1;

    private final CustomTabsConnection mConnection;
    private final CustomTabActivityTabProvider mActivityTabProvider;
    @Nullable
    private final CustomTabsSessionToken mSession;

    private CustomTabActivityTabProvider.Observer mActivityTabObserver;

    @Nullable
    private WebContents mWebContents;
    @Nullable
    private GestureStateListener mGestureStateListener;
    @Nullable
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
            CustomTabActivityTabProvider activityTabProvider, CustomTabsConnection connection,
            @Nullable CustomTabsSessionToken session) {
        mConnection = connection;
        mSession = session;
        mActivityTabProvider = activityTabProvider;

        mActivityTabObserver = createTabProviderObserver();
        activityTabProvider.addObserver(mActivityTabObserver);

        // Do not register observer via tab#addObserver, so it can change tabs when necessary.
        tabObserverRegistrar.registerTabObserver(this);
    }

    // TabObserver
    @Override
    public void onContentChanged(Tab tab) {
        maybeStartSendingRealTimeEngagementSignals(tab);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        removeWebContentsDependencies(tab.getWebContents());
        super.onActivityAttachmentChanged(tab, window);
    }

    @Override
    public void webContentsWillSwap(Tab tab) {
        removeWebContentsDependencies(tab.getWebContents());
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
        removeWebContentsDependencies(tab.getWebContents());
    }

    public void onFinishNativeInitialization() {
        mConnection.setGreatestScrollPercentageSupplier(
                () -> mScrollState != null ? mScrollState.mMaxReportedScrollPercentage : null);
    }

    /**
     * Remove all the dependencies and destroy the instance.
     */
    public void destroy() {
        if (mWebContents != null) {
            removeWebContentsDependencies(mWebContents);
        }
        if (mActivityTabObserver != null) {
            mActivityTabProvider.removeObserver(mActivityTabObserver);
            mActivityTabObserver = null;
        }
    }

    /**
     * Create |mScrollState| and |mGestureStateListener| and start sending real-time engagement
     * signals through {@link androidx.browser.customtabs.CustomTabsCallback}.
     */
    private void maybeStartSendingRealTimeEngagementSignals(Tab tab) {
        if (!shouldSendEngagementSignal(tab)) return;

        if (mWebContents != null) {
            removeWebContentsDependencies(mWebContents);
        }

        mWebContents = tab.getWebContents();
        mScrollState = ScrollState.from(tab);

        if (mGestureStateListener == null) {
            mGestureStateListener = new GestureStateListener() {
                @Override
                public void onScrollStarted(
                        int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                    mScrollState.onScrollStarted(isDirectionUp);
                    mConnection.notifyVerticalScrollEvent(mSession, isDirectionUp);
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
                    if (mScrollState.onScrollDirectionChanged(directionUp)) {
                        mConnection.notifyVerticalScrollEvent(mSession, directionUp);
                    }
                }

                @Override
                public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                    int resultPercentage = mScrollState.onScrollEnded();
                    if (resultPercentage != SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING) {
                        mConnection.notifyGreatestScrollPercentageIncreased(
                                mSession, resultPercentage);
                    }
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
                GestureListenerManager.fromWebContents(mWebContents);
        if (!gestureListenerManager.hasListener(mGestureStateListener)) {
            gestureListenerManager.addListener(mGestureStateListener);
        }
        mWebContents.addObserver(mEngagementSignalWebContentsObserver);
    }

    private void collectUserInteraction(Tab tab) {
        if (!shouldSendEngagementSignal(tab)) return;

        TabInteractionRecorder recorder = TabInteractionRecorder.getFromTab(tab);
        if (recorder == null) return;

        mConnection.notifyDidGetUserInteraction(mSession, recorder.didGetUserInteraction());
    }

    private void removeWebContentsDependencies(@Nullable WebContents webContents) {
        if (webContents == null) return;

        if (mGestureStateListener != null) {
            GestureListenerManager.fromWebContents(webContents)
                    .removeListener(mGestureStateListener);
        }
        if (mEngagementSignalWebContentsObserver != null) {
            webContents.removeObserver(mEngagementSignalWebContentsObserver);
        }

        mGestureStateListener = null;
        mEngagementSignalWebContentsObserver = null;
        mScrollState = null;
        mWebContents = null;
    }

    private boolean shouldSendEngagementSignal(Tab tab) {
        return tab != null && tab.getWebContents() != null
                && !tab.isIncognito()
                // Do not report engagement signals if user does not consent to report usage.
                && PrivacyPreferencesManagerImpl.getInstance().isUsageAndCrashReportingPermitted();
    }

    private CustomTabActivityTabProvider.Observer createTabProviderObserver() {
        return new CustomTabActivityTabProvider.Observer() {
            @Override
            public void onInitialTabCreated(@NonNull Tab tab, int mode) {
                maybeStartSendingRealTimeEngagementSignals(tab);
            }

            @Override
            public void onTabSwapped(@NonNull Tab tab) {
                removeWebContentsDependencies(mWebContents);
                maybeStartSendingRealTimeEngagementSignals(tab);
            }

            @Override
            public void onAllTabsClosed() {
                destroy();
            }
        };
    }

    /**
     * Parameter tracking the entire scrolling journey for the associated tab.
     */
    @VisibleForTesting
    static class ScrollState implements UserData {
        private static ScrollState sTestInstance;

        boolean mIsScrollActive;
        boolean mIsDirectionUp;
        int mMaxScrollPercentage;
        int mMaxReportedScrollPercentage;

        void onScrollStarted(boolean isDirectionUp) {
            assert !mIsScrollActive;
            mIsScrollActive = true;
            mIsDirectionUp = isDirectionUp;
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

        /**
         * @return Whether the scrolling direction actually changed during an active scroll.
         */
        boolean onScrollDirectionChanged(boolean isDirectionUp) {
            if (mIsScrollActive && isDirectionUp != mIsDirectionUp) {
                mIsDirectionUp = isDirectionUp;
                return true;
            }
            return false;
        }

        /**
         * @return the MaxReportedScrollPercentage, or SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING if
         *         we don't want to report.
         */
        int onScrollEnded() {
            int reportedPercentage = SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING;
            int maxScrollPercentageFivesMultiple =
                    mMaxScrollPercentage - (mMaxScrollPercentage % 5);
            if (maxScrollPercentageFivesMultiple > mMaxReportedScrollPercentage) {
                mMaxReportedScrollPercentage = maxScrollPercentageFivesMultiple;
                reportedPercentage = mMaxReportedScrollPercentage;
            }
            mIsScrollActive = false;
            return reportedPercentage;
        }

        void resetMaxScrollPercentage() {
            mMaxScrollPercentage = 0;
            mMaxReportedScrollPercentage = 0;
        }

        static @NonNull ScrollState from(Tab tab) {
            if (sTestInstance != null) return sTestInstance;

            ScrollState scrollState = tab.getUserDataHost().getUserData(ScrollState.class);
            if (scrollState == null) {
                scrollState = new ScrollState();
                tab.getUserDataHost().setUserData(ScrollState.class, scrollState);
            }
            return scrollState;
        }

        @VisibleForTesting
        static void setInstanceForTesting(ScrollState instance) {
            sTestInstance = instance;
        }
    }
}
