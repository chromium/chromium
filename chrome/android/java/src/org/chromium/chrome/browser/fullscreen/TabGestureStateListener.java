// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabWebContentsUserData;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;

/**
 * {@link GestureStateListener} implementation for a {@link Tab}. Associated with an active
 * {@link WebContents} via its {@link GestureListenerManager}. The listener is managed as
 * UserData for the Tab, with WebContents updated as the active one changes over time.
 */
public final class TabGestureStateListener extends TabWebContentsUserData {
    private static final Class<TabGestureStateListener> USER_DATA_KEY =
            TabGestureStateListener.class;

    private final Tab mTab;
    private GestureStateListener mGestureListener;
    private FullscreenManager mFullscreenManager;

    /**
     * Creates TabGestureStateListener and lets the WebContentsUserData of the Tab manage it.
     * @param tab Tab instance that the active WebContents instance gets loaded in.
     */
    public static TabGestureStateListener from(Tab tab) {
        TabGestureStateListener listener = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (listener == null) {
            listener = tab.getUserDataHost().setUserData(
                    USER_DATA_KEY, new TabGestureStateListener(tab));
        }
        return listener;
    }

    private TabGestureStateListener(Tab tab) {
        super(tab);
        mTab = tab;
    }

    /**
     * Set {@link FullscreenManager} instance. This is non-null for active tab.
     * @param manager FullscreenManager instance.
     */
    void setFullscreenManager(FullscreenManager manager) {
        mFullscreenManager = manager;
    }

    @Override
    public void initWebContents(WebContents webContents) {
        GestureListenerManager manager = GestureListenerManager.fromWebContents(webContents);
        mGestureListener = new GestureStateListener() {
            @Override
            public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            private void onScrollingStateChanged() {
                if (mFullscreenManager == null) return;
                boolean scrolling = manager != null ? manager.isScrollInProgress() : false;
                mFullscreenManager.onContentViewScrollingStateChanged(scrolling);
            }
        };
        manager.addListener(mGestureListener);
    }

    @Override
    public void cleanupWebContents(WebContents webContents) {
        if (webContents != null) {
            GestureListenerManager manager = GestureListenerManager.fromWebContents(webContents);
            if (manager != null) manager.removeListener(mGestureListener);
        }
        mGestureListener = null;
    }
}
