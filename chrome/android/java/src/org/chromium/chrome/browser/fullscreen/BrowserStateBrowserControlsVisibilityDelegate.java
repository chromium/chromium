// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.fullscreen;

import android.os.Handler;
import android.os.SystemClock;

import androidx.annotation.VisibleForTesting;

import org.chromium.base.CommandLine;
import org.chromium.base.Supplier;
import org.chromium.chrome.browser.ChromeSwitches;
import org.chromium.chrome.browser.tab.BrowserControlsVisibilityDelegate;
import org.chromium.ui.util.TokenHolder;

/**
 * Determines the desired visibility of the browser controls based on the current state of the
 * running activity.
 */
public class BrowserStateBrowserControlsVisibilityDelegate
        implements BrowserControlsVisibilityDelegate {

    /** Minimum duration (in milliseconds) that the controls are shown when requested. */
    @VisibleForTesting
    static final long MINIMUM_SHOW_DURATION_MS = 3000;

    private static boolean sDisableOverridesForTesting;

    private final TokenHolder mTokenHolder;

    private final Handler mHandler = new Handler();

    /** Predicate that tells if we're in persistent fullscreen mode. */
    private final Supplier<Boolean> mPersistentFullscreenMode;

    private long mCurrentShowingStartTime;

    /**
     * Constructs a BrowserControlsVisibilityDelegate designed to deal with overrides driven by
     * the browser UI (as opposed to the state of the tab).
     *
     * @param stateChangedCallback The callback to be triggered when the fullscreen state should be
     *                             updated based on the state of the browser visibility override.
     * @param persistentFullscreenMode Predicate that tells if we're in persistent fullscreen mode.
     */
    public BrowserStateBrowserControlsVisibilityDelegate(
            Runnable stateChangedCallback, Supplier<Boolean> persistentFullscreenMode) {
        mTokenHolder = new TokenHolder(stateChangedCallback);
        mPersistentFullscreenMode = persistentFullscreenMode;
    }

    private void ensureControlsVisibleForMinDuration() {
        // Do not lock the controls as visible. Such as in testing.
        if (CommandLine.getInstance().hasSwitch(ChromeSwitches.DISABLE_MINIMUM_SHOW_DURATION)) {
            return;
        }
        if (mHandler.hasMessages(0)) return; // Messages sent via post/postDelayed have what=0

        long currentShowingTime = SystemClock.uptimeMillis() - mCurrentShowingStartTime;
        if (currentShowingTime >= MINIMUM_SHOW_DURATION_MS) return;

        final int temporaryToken = mTokenHolder.acquireToken();
        mHandler.postDelayed(() -> mTokenHolder.releaseToken(temporaryToken),
                MINIMUM_SHOW_DURATION_MS - currentShowingTime);
    }

    /**
     * Trigger a temporary showing of the browser controls.
     */
    public void showControlsTransient() {
        if (!mTokenHolder.hasTokens()) mCurrentShowingStartTime = SystemClock.uptimeMillis();
        ensureControlsVisibleForMinDuration();
    }

    /**
     * Trigger a permanent showing of the browser controls until requested otherwise.
     *
     * @return The token that determines whether the requester still needs persistent controls to
     *         be present on the screen.
     * @see #releasePersistentShowingToken(int)
     */
    public int showControlsPersistent() {
        if (!mTokenHolder.hasTokens()) mCurrentShowingStartTime = SystemClock.uptimeMillis();
        return mTokenHolder.acquireToken();
    }

    /**
     * Same behavior as {@link #showControlsPersistent()} but also handles removing a previously
     * requested token if necessary.
     *
     * @param oldToken The old fullscreen token to be cleared.
     * @return The fullscreen token as defined in {@link #showControlsPersistent()}.
     */
    public int showControlsPersistentAndClearOldToken(int oldToken) {
        int newToken = showControlsPersistent();
        mTokenHolder.releaseToken(oldToken);
        return newToken;
    }

    /**
     * Notify the manager that the browser controls are no longer required for the given token.
     *
     * @param token The fullscreen token returned from {@link #showControlsPersistent()}.
     */
    public void releasePersistentShowingToken(int token) {
        if (mTokenHolder.containsOnly(token)) {
            ensureControlsVisibleForMinDuration();
        }
        mTokenHolder.releaseToken(token);
    }

    @Override
    public boolean canShowBrowserControls() {
        return !mPersistentFullscreenMode.get();
    }

    @Override
    public boolean canAutoHideBrowserControls() {
        return sDisableOverridesForTesting || !mTokenHolder.hasTokens();
    }

    /**
     * Disable any browser visibility overrides for testing.
     */
    public static void disableForTesting() {
        sDisableOverridesForTesting = true;
    }

    /**
     * Performs clean-up.
     */
    public void destroy() {
        mHandler.removeCallbacksAndMessages(null);
    }
}
