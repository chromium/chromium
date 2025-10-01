// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;

/**
 * {@link GestureStateListener} implementation for a {@link Tab}. Associated with an active {@link
 * WebContents} via its {@link GestureListenerManager}. The listener is managed as UserData for the
 * Tab, with WebContents updated as the active one changes over time.
 */
@NullMarked
public final class TabGestureStateListener extends TabWebContentsUserData {
    private static final Class<TabGestureStateListener> USER_DATA_KEY =
            TabGestureStateListener.class;

    private final Tab mTab;
    private @Nullable GestureStateListener mGestureListener;

    /**
     * Creates TabGestureStateListener and lets the WebContentsUserData of the Tab manage it.
     *
     * @param tab Tab instance that the active WebContents instance gets loaded in.
     */
    public static TabGestureStateListener from(Tab tab) {
        TabGestureStateListener listener = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (listener == null) {
            listener =
                    tab.getUserDataHost()
                            .setUserData(USER_DATA_KEY, new TabGestureStateListener(tab));
        }
        return listener;
    }

    private TabGestureStateListener(Tab tab) {
        super(tab);
        mTab = tab;
    }

    @Override
    public void initWebContents(WebContents webContents) {
        assert mGestureListener == null;
        GestureListenerManager manager = GestureListenerManager.fromWebContents(webContents);
        assumeNonNull(manager);
        mGestureListener =
                new GestureStateListener() {
                    @Override
                    public void onFlingStartGesture(
                            int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                        onScrollingStateChanged();
                    }

                    @Override
                    public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                        onScrollingStateChanged();
                    }

                    @Override
                    public void onScrollStarted(
                            int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                        onScrollingStateChanged();
                    }

                    @Override
                    public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {
                        onScrollingStateChanged();
                    }

                    @Override
                    public void onGestureBegin() {
                        RewindableIterator<TabObserver> observers =
                                ((TabImpl) mTab).getTabObservers();
                        while (observers.hasNext()) {
                            observers.next().onGestureBegin();
                        }
                    }

                    @Override
                    public void onGestureEnd() {
                        RewindableIterator<TabObserver> observers =
                                ((TabImpl) mTab).getTabObservers();
                        while (observers.hasNext()) {
                            observers.next().onGestureEnd();
                        }
                    }

                    @Override
                    public void onTouchDown() {
                        RewindableIterator<TabObserver> observers =
                                ((TabImpl) mTab).getTabObservers();
                        while (observers.hasNext()) {
                            observers.next().onTouchDown();
                        }
                    }

                    @Override
                    public void onTouchUp() {
                        RewindableIterator<TabObserver> observers =
                                ((TabImpl) mTab).getTabObservers();
                        while (observers.hasNext()) {
                            observers.next().onTouchUp();
                        }
                    }

                    private void onScrollingStateChanged() {
                        boolean scrolling = manager.isScrollInProgress();
                        RewindableIterator<TabObserver> observers =
                                ((TabImpl) mTab).getTabObservers();
                        while (observers.hasNext()) {
                            observers.next().onContentViewScrollingStateChanged(scrolling);
                        }
                    }
                };
        manager.addListener(mGestureListener);
    }

    @Override
    public void cleanupWebContents(@Nullable WebContents webContents) {
        if (webContents != null) {
            GestureListenerManager manager = GestureListenerManager.fromWebContents(webContents);
            if (manager != null && mGestureListener != null) {
                manager.removeListener(mGestureListener);
            }
        }
        mGestureListener = null;
    }
}
