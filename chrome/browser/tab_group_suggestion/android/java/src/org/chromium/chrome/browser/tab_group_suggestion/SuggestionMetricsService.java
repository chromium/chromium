// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab_group_suggestion;

import static org.chromium.base.ApplicationStatus.registerStateListenerForActivity;
import static org.chromium.base.ApplicationStatus.unregisterActivityStateListener;

import android.app.Activity;

import androidx.annotation.IntDef;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus.ActivityStateListener;
import org.chromium.base.Token;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.browser.lifecycle.StartStopWithNativeObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.tabwindow.WindowId;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;
import java.util.HashMap;
import java.util.Map;

/**
 * A metrics service for tab group suggestions. This service manages {@link
 * SuggestionMetricsTracker} instances for each window.
 */
@NullMarked
public class SuggestionMetricsService {
    /** Defines the sources for tab group creation. */
    @IntDef({
        GroupCreationSource.GTS_SUGGESTION,
        GroupCreationSource.CPA_SUGGESTION,
        GroupCreationSource.UNKNOWN
    })
    @Retention(RetentionPolicy.SOURCE)
    @Target({ElementType.TYPE_USE})
    public @interface GroupCreationSource {
        /** Tab group created from a suggestion in the Grid Tab Switcher. */
        int GTS_SUGGESTION = 0;

        /** Tab group created from a contextual page action suggestion in the toolbar. */
        int CPA_SUGGESTION = 1;

        /** Tab group created from an unknown source. */
        int UNKNOWN = 2;
    }

    private final Map<@WindowId Integer, SuggestionMetricsLifecycleManager> mLifecycleObservers =
            new HashMap<>();

    /**
     * Manage activity lifecycles for a single {@link SuggestionMetricsTracker}.
     *
     * <p>The lifecycle of each {@link SuggestionMetricsTracker} is managed by a corresponding
     * {@link SuggestionMetricsLifecycleManager} and is tied to the activity lifecycle:
     *
     * <ul>
     *   <li>Initialization: A tracker is created via {@link #initializeTracker(int,
     *       TabModelSelector, ActivityLifecycleDispatcher)}.
     *   <li>Activity Start: The tracker is reset.
     *   <li>Activity Stop: The tracker records the collected metrics for the session.
     *   <li>Activity Destroy: The tracker is cleaned up.
     * </ul>
     */
    private class SuggestionMetricsLifecycleManager
            implements StartStopWithNativeObserver, ActivityStateListener {
        private final @WindowId int mWindowId;
        private final SuggestionMetricsTracker mTracker;
        private final ActivityLifecycleDispatcher mLifecycleDispatcher;

        /**
         * @param windowId The ID of the window associated with this lifecycle observer.
         * @param activity The activity associated with this lifecycle observer.
         * @param tracker The {@link SuggestionMetricsTracker} to manage.
         * @param lifecycleDispatcher The {@link ActivityLifecycleDispatcher} for observing activity
         *     lifecycle events.
         */
        SuggestionMetricsLifecycleManager(
                @WindowId int windowId,
                Activity activity,
                SuggestionMetricsTracker tracker,
                ActivityLifecycleDispatcher lifecycleDispatcher) {
            mWindowId = windowId;
            mTracker = tracker;
            mLifecycleDispatcher = lifecycleDispatcher;

            lifecycleDispatcher.register(this);
            registerStateListenerForActivity(this, activity);
        }

        @Override
        public void onActivityStateChange(Activity activity, @ActivityState int newState) {
            if (newState != ActivityState.DESTROYED) return;

            mLifecycleObservers.remove(mWindowId);
            mTracker.destroy();
            unregisterActivityStateListener(this);
            mLifecycleDispatcher.unregister(this);
        }

        @Override
        public void onStartWithNative() {
            mTracker.reset();
        }

        @Override
        public void onStopWithNative() {
            mTracker.recordMetrics();
        }

        /** Gets the suggestion metrics tracker. */
        SuggestionMetricsTracker getTracker() {
            return mTracker;
        }
    }

    /**
     * Initializes the metrics tracker for a given window.
     *
     * @param windowId The ID of the window to track.
     * @param activity The activity to track state changes for.
     * @param tabModelSelector The {@link TabModelSelector} to observe for tab changes.
     * @param lifecycleDispatcher Used to subscribe to activity lifecycle events.
     */
    public void initializeTracker(
            @WindowId int windowId,
            Activity activity,
            TabModelSelector tabModelSelector,
            ActivityLifecycleDispatcher lifecycleDispatcher) {
        SuggestionMetricsTracker tracker = new SuggestionMetricsTracker(tabModelSelector);
        SuggestionMetricsLifecycleManager observer =
                new SuggestionMetricsLifecycleManager(
                        windowId, activity, tracker, lifecycleDispatcher);
        mLifecycleObservers.put(windowId, observer);
    }

    /**
     * Called when a tab group suggestion is accepted.
     *
     * @param windowId The ID of the window where the suggestion was accepted.
     * @param suggestionType The type of suggestion that was accepted.
     * @param groupId The tab group ID of the suggested tab group.
     */
    public void onSuggestionAccepted(
            @WindowId int windowId, @GroupCreationSource Integer suggestionType, Token groupId) {
        SuggestionMetricsLifecycleManager observer = mLifecycleObservers.get(windowId);
        assert observer != null;

        observer.getTracker().onSuggestionAccepted(suggestionType, groupId);
    }
}
