// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.gesturenav;

import org.chromium.base.UserData;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.content_public.browser.BackForwardTransitionAnimationManager;
import org.chromium.content_public.browser.WebContents;
import org.chromium.content_public.browser.back_forward_transition.AnimationStage;
import org.chromium.ui.base.BackGestureEventSwipeEdge;

/**
 * A handler to trigger seamless navigation / predictive back animation when a back gesture is
 * performed on a navigable tab page.
 */
@NullMarked
public class TabOnBackGestureHandler implements UserData {
    private static final Class<TabOnBackGestureHandler> USER_DATA_KEY =
            TabOnBackGestureHandler.class;

    /**
     * @param tab The tab in which page is displayed and back gesture is performed.
     * @return A {@link TabOnBackGestureHandler} to trigger animation on the given tab.
     */
    public static TabOnBackGestureHandler from(Tab tab) {
        var tabOnBackGestureHandler = tab.getUserDataHost().getUserData(USER_DATA_KEY);
        if (tabOnBackGestureHandler != null) return tabOnBackGestureHandler;
        return tab.getUserDataHost().setUserData(USER_DATA_KEY, new TabOnBackGestureHandler(tab));
    }

    private final Tab mTab;
    private boolean mDestroyed;
    private boolean mIsGestureMode;
    private @BackGestureEventSwipeEdge int mStartedEdge;
    private @Nullable WebContents mGesturedWebContents;
    private @Nullable BackForwardTransitionAnimationManager mGestureManager;

    private TabOnBackGestureHandler(Tab tab) {
        mTab = tab;
    }

    public void onBackStarted(
            float progress,
            @BackGestureEventSwipeEdge int edge,
            boolean forward,
            boolean isGestureMode) {
        if (mDestroyed) return;
        if (mGestureManager != null) onBackCancelled(isGestureMode);

        WebContents webContents = mTab.getWebContents();
        assert webContents != null && !webContents.isDestroyed();
        if (webContents == null || webContents.isDestroyed()) return;
        var manager = webContents.getBackForwardTransitionAnimationManager();
        assert manager != null;
        if (manager == null) return;

        mIsGestureMode = isGestureMode;
        mStartedEdge = edge;
        mGesturedWebContents = webContents;
        mGestureManager = manager;
        manager.onGestureStarted(progress, edge, forward, isGestureMode);
    }

    /**
     * Returns whether this handler is still driving the caller's gesture. When it returns false the
     * caller owns the gesture again: it must drop its reference to this handler and do its own back
     * handling, otherwise the navigation is lost. The handler is not necessarily idle then: on an
     * edge mismatch it may still be driving a newer gesture for another owner.
     */
    public boolean onBackProgressed(
            float progress,
            @BackGestureEventSwipeEdge int edge,
            boolean forward,
            boolean isGestureMode) {
        var manager = mGestureManager;
        if (mDestroyed || manager == null || mStartedEdge != edge || !isCurrentGesture()) {
            // Do not cancel another owner's gesture or restart from an unchecked direction.
            // Return control to the caller so that it can handle navigation. crbug.com/530682179.
            return false;
        }
        manager.onGestureProgressed(progress, isGestureMode);
        return true;
    }

    public void onBackCancelled(boolean isGestureMode) {
        finishGesture(/* invoke= */ false, isGestureMode);
    }

    public void onBackInvoked(boolean isGestureMode) {
        finishGesture(/* invoke= */ true, isGestureMode);
    }

    private boolean isCurrentGesture() {
        var webContents = mGesturedWebContents;
        return webContents != null
                && !webContents.isDestroyed()
                && mTab.getWebContents() == webContents
                && webContents.getBackForwardTransitionAnimationManager() == mGestureManager;
    }

    private void finishGesture(boolean invoke, boolean isGestureMode) {
        var manager = mGestureManager;
        boolean isCurrentGesture = isCurrentGesture();
        mGestureManager = null;
        mGesturedWebContents = null;
        if (mDestroyed || manager == null || !isCurrentGesture) return;
        // Never send a terminal event to a WebContents that did not receive the start event.
        if (invoke) {
            manager.onGestureInvoked(isGestureMode);
        } else {
            manager.onGestureCancelled(isGestureMode);
        }
    }

    /**
     * Releases this Tab's gesture when its UserDataHost is destroyed. The Tab may release its
     * WebContents for reuse instead of destroying it, so native manager destruction cannot replace
     * this cleanup. Callers may also retain this handler after the Tab is destroyed.
     */
    @Override
    public void destroy() {
        if (mDestroyed) return;
        var manager = mGestureManager;
        // A detached compositor has already aborted the animation. Avoid cancelling it then.
        if (manager != null
                && isCurrentGesture()
                && manager.getCurrentAnimationStage() != AnimationStage.NONE) {
            onBackCancelled(mIsGestureMode);
        }
        mGestureManager = null;
        mGesturedWebContents = null;
        mDestroyed = true;
    }
}
