// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.content;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.cc.mojom.RootScrollOffsetUpdateFrequency;
import org.chromium.chrome.browser.customtabs.content.TabObserverRegistrar.CustomTabTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabHidingType;
import org.chromium.content_public.browser.GestureListenerManager;
import org.chromium.content_public.browser.GestureStateListener;
import org.chromium.content_public.browser.LoadCommittedDetails;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.WebContentsObserver;
import org.chromium.ui.base.WindowAndroid;

/**
 * Class that observes the Custom Tab for an initial scroll down gesture before
 * {@link RealtimeEngagementSignalObserver} is created.
 */
public class EngagementSignalsInitialScrollObserver extends CustomTabTabObserver {
    private final TabObserverRegistrar mTabObserverRegistrar;

    private WebContents mWebContents;
    private GestureStateListener mGestureStateListener;
    private WebContentsObserver mWebContentsObserver;
    private boolean mHadScrollDown;

    public EngagementSignalsInitialScrollObserver(TabObserverRegistrar tabObserverRegistrar) {
        mTabObserverRegistrar = tabObserverRegistrar;
        mTabObserverRegistrar.registerActivityTabObserver(this);
    }

    public void destroy() {
        mTabObserverRegistrar.unregisterActivityTabObserver(this);
        cleanUpListeners();
    }

    // extends CustomTabTabObserver
    @Override
    protected void onAttachedToInitialTab(@NonNull Tab tab) {
        startTrackingScrolls(tab);
    }

    @Override
    protected void onObservingDifferentTab(@NonNull Tab tab) {
        cleanUpListeners();
        startTrackingScrolls(tab);
    }

    @Override
    protected void onAllTabsClosed() {
        cleanUpListeners();
    }

    // extends TabObserver
    @Override
    public void onContentChanged(Tab tab) {
        startTrackingScrolls(tab);
    }

    @Override
    public void onActivityAttachmentChanged(Tab tab, @Nullable WindowAndroid window) {
        cleanUpListeners();
        super.onActivityAttachmentChanged(tab, window);
    }

    @Override
    public void webContentsWillSwap(Tab tab) {
        cleanUpListeners();
    }

    @Override
    public void onHidden(Tab tab, @TabHidingType int reason) {
        if (reason == TabHidingType.CHANGED_TABS) {
            mHadScrollDown = false;
        }
    }

    @Override
    public void onClosingStateChanged(Tab tab, boolean closing) {
        if (!closing) return;
        cleanUpListeners();
    }

    @Override
    public void onDestroyed(Tab tab) {
        cleanUpListeners();
    }

    private void startTrackingScrolls(Tab tab) {
        mHadScrollDown = false;

        if (mWebContents != null) {
            cleanUpListeners();
        }
        mWebContents = tab.getWebContents();

        mGestureStateListener =
                new GestureStateListener() {
                    @Override
                    public void onScrollStarted(
                            int scrollOffsetY, int scrollExtentY, boolean isDirectionUp) {
                        if (!isDirectionUp) {
                            mHadScrollDown = true;
                        }
                    }

                    @Override
                    public void onVerticalScrollDirectionChanged(
                            boolean directionUp, float currentScrollRatio) {
                        // If the scroll direction changed, either the previous direction was or the
                        // new direction is a down scroll.
                        mHadScrollDown = true;
                    }
                };

        mWebContentsObserver =
                new WebContentsObserver() {
                    @Override
                    public void navigationEntryCommitted(LoadCommittedDetails details) {
                        if (details.isMainFrame() && !details.isSameDocument()) {
                            mHadScrollDown = false;
                        }
                    }
                };

        GestureListenerManager gestureListenerManager =
                GestureListenerManager.fromWebContents(mWebContents);
        if (!gestureListenerManager.hasListener(mGestureStateListener)) {
            gestureListenerManager.addListener(
                    mGestureStateListener, RootScrollOffsetUpdateFrequency.NONE);
        }
        mWebContents.addObserver(mWebContentsObserver);
    }

    private void cleanUpListeners() {
        mHadScrollDown = false;
        if (mWebContents != null) {
            if (mGestureStateListener != null) {
                GestureListenerManager.fromWebContents(mWebContents)
                        .removeListener(mGestureStateListener);
            }
            if (mWebContentsObserver != null) {
                mWebContents.removeObserver(mWebContentsObserver);
            }
        }
        mWebContents = null;
        mGestureStateListener = null;
        mWebContentsObserver = null;
    }

    /** Returns whether the current page has had a scroll down gesture. */
    public boolean hasCurrentPageHadScrollDown() {
        return mHadScrollDown;
    }
}
