// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.media.immersive_playback.components;

import android.os.Handler;
import android.os.Looper;

import org.chromium.build.annotations.NullMarked;

/** Helper class that manages auto-hide inactivity timer for immersive video controls. */
@NullMarked
public class ImmersiveVideoControlAutoHideManager {
    /** The default auto-hide inactivity delay in milliseconds. */
    public static final long AUTO_HIDE_DELAY_MS = 3000L;

    private final Handler mHandler = new Handler(Looper.getMainLooper());
    private final Runnable mAutoHideRunnable;
    private final long mDelayMs;

    private boolean mControlPanelHovered;
    private boolean mFormatPanelHovered;
    private boolean mControlPanelMoving;

    /**
     * Creates a new {@link ImmersiveVideoControlAutoHideManager} with default delay.
     *
     * @param autoHideRunnable The runnable to execute when the inactivity timer expires.
     */
    public ImmersiveVideoControlAutoHideManager(Runnable autoHideRunnable) {
        this(autoHideRunnable, AUTO_HIDE_DELAY_MS);
    }

    /**
     * Creates a new {@link ImmersiveVideoControlAutoHideManager}.
     *
     * @param autoHideRunnable The runnable to execute when the inactivity timer expires.
     * @param delayMs The inactivity delay in milliseconds.
     */
    public ImmersiveVideoControlAutoHideManager(Runnable autoHideRunnable, long delayMs) {
        mAutoHideRunnable = autoHideRunnable;
        mDelayMs = delayMs;
    }

    /** Called when movement state of the control panel changes. */
    public void onControlPanelMoveChanged(boolean isMoving) {
        mControlPanelMoving = isMoving;
        updateTimer();
    }

    /** Called when hover state of the control panel changes. */
    public void onControlPanelHoverChanged(boolean hovered) {
        mControlPanelHovered = hovered;
        updateTimer();
    }

    /** Called when hover state of the format selection panel changes. */
    public void onFormatPanelHoverChanged(boolean hovered) {
        mFormatPanelHovered = hovered;
        updateTimer();
    }

    /** Starts or restarts the inactivity timer. */
    public void startTimer() {
        mControlPanelHovered = false;
        mFormatPanelHovered = false;
        mControlPanelMoving = false;
        updateTimer();
    }

    /** Stops the inactivity timer. */
    public void stopTimer() {
        mHandler.removeCallbacks(mAutoHideRunnable);
    }

    private void updateTimer() {
        if (mControlPanelHovered || mFormatPanelHovered || mControlPanelMoving) {
            stopTimer();
        } else {
            mHandler.removeCallbacks(mAutoHideRunnable);
            mHandler.postDelayed(mAutoHideRunnable, mDelayMs);
        }
    }
}
