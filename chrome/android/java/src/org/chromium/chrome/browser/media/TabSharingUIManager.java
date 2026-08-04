// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media;

import org.chromium.base.ObserverList;
import org.chromium.base.ResettersForTesting;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.WebContents;

import java.util.ArrayList;
import java.util.List;

/**
 * Global manager for tab sharing toolbars. This class is the entry point for showing the toolbar
 * UI. It tracks all active sharing sessions by the respective Bridge instances, and broadcasts the
 * events to create and destroy the UI.
 */
@NullMarked
public class TabSharingUIManager {
    private static @Nullable TabSharingUIManager sInstance;

    public interface Observer {
        /** Called when a new tab sharing session starts. */
        void onSharingSessionStarted(TabSharingUIBridge bridge);

        /** Called when an existing tab sharing session stops. */
        void onSharingSessionStopped(TabSharingUIBridge bridge);
    }

    private final List<TabSharingUIBridge> mActiveBridges = new ArrayList<>();
    private final ObserverList<Observer> mObservers = new ObserverList<>();

    /** Returns the singleton instance of TabSharingUIManager. */
    public static TabSharingUIManager getInstance() {
        TabSharingUIManager instance = sInstance;
        if (instance == null) {
            instance = new TabSharingUIManager();
            sInstance = instance;
        }
        return instance;
    }

    /**
     * Sets the singleton instance for testing.
     *
     * @param instance The test instance to use, or null to reset.
     */
    public static void setInstanceForTesting(@Nullable TabSharingUIManager instance) {
        var oldInstance = sInstance;
        sInstance = instance;
        ResettersForTesting.register(() -> sInstance = oldInstance);
    }

    TabSharingUIManager() {}

    /**
     * Adds an observer to be notified of tab sharing session events.
     *
     * @param observer The observer to add.
     */
    public void addObserver(Observer observer) {
        mObservers.addObserver(observer);
        // Notify the new observer of all currently active sessions.
        for (TabSharingUIBridge bridge : mActiveBridges) {
            observer.onSharingSessionStarted(bridge);
        }
    }

    /**
     * Removes an observer.
     *
     * @param observer The observer to remove.
     */
    public void removeObserver(Observer observer) {
        mObservers.removeObserver(observer);
    }

    /**
     * Adds a bridge and notifies observers that sharing started.
     *
     * @param bridge The bridge to add.
     */
    public void addBridge(TabSharingUIBridge bridge) {
        mActiveBridges.add(bridge);
        for (Observer observer : mObservers) {
            observer.onSharingSessionStarted(bridge);
        }
    }

    /**
     * Removes a bridge and notifies observers that sharing stopped.
     *
     * @param bridge The bridge to remove.
     */
    public void removeBridge(TabSharingUIBridge bridge) {
        boolean removed = mActiveBridges.remove(bridge);
        assert removed : "Bridge not found in active bridges: " + bridge;
        if (removed) {
            for (Observer observer : mObservers) {
                observer.onSharingSessionStopped(bridge);
            }
        }
    }

    /**
     * Stops any active sharing session initiated by the specified capturer WebContents.
     *
     * @param capturer The {@link WebContents} performing tab sharing.
     */
    public void stopSharingByCapturerTab(WebContents capturer) {
        for (TabSharingUIBridge bridge : mActiveBridges) {
            if (bridge.getCapturer() == capturer) {
                bridge.stopSharing();
                // A capturer WebContents can have at most one active tab sharing session at a time
                // (a 1:1 relationship between bridge and capturer), so returning on first match is
                // safe.
                return;
            }
        }
    }

    /** Returns whether there are any active tab sharing sessions. */
    public boolean isSharing() {
        return !mActiveBridges.isEmpty();
    }
}
