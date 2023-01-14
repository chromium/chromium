// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.ark.browser.tab;

import androidx.annotation.NonNull;

import com.ark.browser.core.ArkWebContents;

import org.chromium.base.ObserverList.RewindableIterator;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabObserver;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.WebContents;
import org.chromium.ui.base.WindowAndroid;

/**
 * {@link GestureStateListener} implementation for a {@link Tab}. Associated with an active
 * {@link WebContents} via its {@link GestureListenerManager}. The listener is managed as
 * UserData for the Tab, with WebContents updated as the active one changes over time.
 */
public final class ArkTabGestureStateListener extends ArkTabWebContentsUserData {
    private static final Class<ArkTabGestureStateListener> USER_DATA_KEY =
            ArkTabGestureStateListener.class;

    private final ArkTabImpl mTab;
    private GestureStateListener mGestureListener;

    /**
     * Creates TabGestureStateListener and lets the WebContentsUserData of the Tab manage it.
     * @param tab Tab instance that the active WebContents instance gets loaded in.
     */
    public static ArkTabGestureStateListener from(ArkTabImpl tab) {
        ArkTabGestureStateListener listener = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (listener == null) {
            listener = tab.getUserDataHost().setUserData(
                    USER_DATA_KEY, new ArkTabGestureStateListener(tab));
        }
        return listener;
    }

    private ArkTabGestureStateListener(ArkTabImpl tab) {
        super(tab);
        mTab = tab;
    }

    @Override
    public void initWebContents(ArkWebContents arkWeb) {
        if (mTab.getWindowAndroid() == null) {
            return;
        }
        WebContents webContents = arkWeb.getWebContents();
        GestureListenerManager manager = GestureListenerManager.fromWebContents(webContents);
        mGestureListener = new GestureStateListener() {
            private int mLastScrollOffsetY;

            @Override
            public void onFlingStartGesture(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
            }

            @Override
            public void onFlingEndGesture(int scrollOffsetY, int scrollExtentY) {
                mTab.cacheThumbnail();
                onScrollingStateChanged();
            }

            @Override
            public void onScrollStarted(int scrollOffsetY, int scrollExtentY) {
                onScrollingStateChanged();
                mLastScrollOffsetY = scrollOffsetY;
            }

            @Override
            public void onScrollEnded(int scrollOffsetY, int scrollExtentY) {

                mTab.cacheThumbnail();

                onScrollingStateChanged();
                RewindableIterator<TabObserver> observers = mTab.getTabObservers();
                while (observers.hasNext()) {
                    observers.next().onContentViewScrollingEnded(
                            mLastScrollOffsetY - scrollOffsetY);
                }
            }

            private void onScrollingStateChanged() {
                boolean scrolling = manager != null && manager.isScrollInProgress();
                RewindableIterator<TabObserver> observers = mTab.getTabObservers();
                while (observers.hasNext()) {
                    observers.next().onContentViewScrollingStateChanged(scrolling);
                }
            }
        };
        manager.addListener(mGestureListener);
    }

    @Override
    public void onAttachToWindowAndroid(@NonNull WindowAndroid windowAndroid) {
        ArkWebContents arkWeb = mTab.getArkWeb();
        if (arkWeb == null) {
            return;
        }
        initWebContents(arkWeb);
    }

    @Override
    public void onDetachToWindowAndroid() {
        ArkWebContents arkWeb = mTab.getArkWeb();
        if (arkWeb == null) {
            return;
        }
        cleanupWebContents(arkWeb);
    }

    @Override
    public void cleanupWebContents(ArkWebContents arkWeb) {
        if (arkWeb != null) {
            GestureListenerManager manager = GestureListenerManager.fromWebContents(
                    arkWeb.getWebContents());
            if (manager != null) manager.removeListener(mGestureListener);
        }
        mGestureListener = null;
    }
}
