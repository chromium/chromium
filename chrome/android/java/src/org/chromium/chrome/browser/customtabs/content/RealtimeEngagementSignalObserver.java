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
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.customtabs.CustomTabsConnection;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.customtabs.features.TabInteractionRecorder;
import org.chromium.chrome.browser.dependency_injection.ActivityScope;
import org.chromium.chrome.browser.flags.ChromeFeatureList;
import org.chromium.chrome.browser.privacy.settings.PrivacyPreferencesManagerImpl;
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
 * engagement signal includes:
 * <ul>
 *    <li>User scrolling direction; </li>
 *    <li>Max scroll percent on a specific tab;</li>
 *    <li>Whether user had interaction with any tab when CCT closes.</li>
 * </ul>
 *
 * The engagement signal will reset in navigation.
 */
// TODO(https://crbug.com/1381619): Reset scroll state during tab switching.
@ActivityScope
class RealtimeEngagementSignalObserver extends CustomTabTabObserver {
    private static final int SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING = -1;
    // Limit the granularity of data the embedder receives.
    private static final int SCROLL_PERCENTAGE_GRANULARITY = 5;

    // Feature param to decide whether to send real values with engagement signals.
    @VisibleForTesting
    protected static final String REAL_VALUES = "real_values";
    private static final int STUB_PERCENT = 0;

    private final CustomTabsConnection mConnection;
    private final TabObserverRegistrar mTabObserverRegistrar;

    @Nullable
    private final CustomTabsSessionToken mSession;

    private final boolean mShouldSendRealValues;

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
            CustomTabsConnection connection, @Nullable CustomTabsSessionToken session) {
        mConnection = connection;
        mSession = session;
        mTabObserverRegistrar = tabObserverRegistrar;

        // Do not register observer via tab#addObserver, so it can change tabs when necessary.
        mTabObserverRegistrar.registerActivityTabObserver(this);

        mShouldSendRealValues = shouldSendRealValues();
        initializeGreatestScrollPercentageSupplier();
    }

    // extends CustomTabTabObserver
    @Override
    protected void onAttachedToInitialTab(@NonNull Tab tab) {
        maybeStartSendingRealTimeEngagementSignals(tab);
    }

    @Override
    protected void onObservingDifferentTab(@NonNull Tab tab) {
        removeWebContentsDependencies(mWebContents);
        maybeStartSendingRealTimeEngagementSignals(tab);
    }

    @Override
    protected void onAllTabsClosed() {
        removeWebContentsDependencies(mWebContents);
    }

    // extends TabObserver
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
    public void onClosingStateChanged(Tab tab, boolean closing) {
        if (!closing) return;
        removeWebContentsDependencies(mWebContents);
    }

    @Override
    public void onDestroyed(Tab tab) {
        collectUserInteraction(tab);
        removeWebContentsDependencies(tab.getWebContents());
    }

    private void initializeGreatestScrollPercentageSupplier() {
        Supplier<Integer> percentageSupplier = () -> {
            if (mScrollState != null) {
                return mShouldSendRealValues ? mScrollState.mMaxReportedScrollPercentage
                                             : STUB_PERCENT;
            }
            return null;
        };
        mConnection.setGreatestScrollPercentageSupplier(percentageSupplier);
    }

    /**
     * Create |mScrollState| and |mGestureStateListener| and start sending real-time engagement
     * signals through {@link androidx.browser.customtabs.CustomTabsCallback}.
     */
    private void maybeStartSendingRealTimeEngagementSignals(Tab tab) {
        if (!shouldSendEngagementSignal(tab)) {
            return;
        }

        if (mWebContents != null) {
            removeWebContentsDependencies(mWebContents);
        }

        assert mGestureStateListener
                == null : "mGestureStateListener should be null when start observing new tab.";
        assert mEngagementSignalWebContentsObserver
                == null
            : "mEngagementSignalWebContentsObserver should be null when start observing new tab.";

        mWebContents = tab.getWebContents();
        mScrollState = ScrollState.from(tab);

        mGestureStateListener = new GestureStateListener() {
            @Override
            public void onScrollStarted(
                    int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                mScrollState.onScrollStarted(isDirectionUp);
                // If we shouldn't send the real values, always send false.
                mConnection.notifyVerticalScrollEvent(
                        mSession, mShouldSendRealValues && isDirectionUp);
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
                    mConnection.notifyVerticalScrollEvent(
                            mSession, mShouldSendRealValues && directionUp);
                }
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                int resultPercentage = mScrollState.onScrollEnded();
                if (resultPercentage != SCROLL_STATE_MAX_PERCENTAGE_NOT_INCREASING) {
                    mConnection.notifyGreatestScrollPercentageIncreased(
                            mSession, mShouldSendRealValues ? resultPercentage : STUB_PERCENT);
                }
            }
        };

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
        if (webContents != null) {
            if (mGestureStateListener != null) {
                GestureListenerManager.fromWebContents(webContents)
                        .removeListener(mGestureStateListener);
            }
            if (mEngagementSignalWebContentsObserver != null) {
                webContents.removeObserver(mEngagementSignalWebContentsObserver);
            }
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
            // We shouldn't get an |onScrollStarted()| call while a scroll is still in progress,
            // but it can happen. Call |onScrollEnded()| to make sure we're in a valid state.
            if (mIsScrollActive) onScrollEnded();
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
                    mMaxScrollPercentage - (mMaxScrollPercentage % SCROLL_PERCENTAGE_GRANULARITY);
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

    private static boolean shouldSendRealValues() {
        boolean enabledWithOverride =
                CustomTabsConnection.getInstance().isDynamicFeatureEnabledWithOverrides(
                        ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS);
        if (enabledWithOverride) return true;

        return ChromeFeatureList.getFieldTrialParamByFeatureAsBoolean(
                ChromeFeatureList.CCT_REAL_TIME_ENGAGEMENT_SIGNALS, REAL_VALUES, true);
    }
}
