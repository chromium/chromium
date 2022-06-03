// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.content_creation.reactions;

import android.graphics.Bitmap;

import org.chromium.components.content_creation.reactions.ReactionMetadata;

import jp.tomorrowkey.android.gifplayer.BaseGifDrawable;
import jp.tomorrowkey.android.gifplayer.BaseGifImage;

/**
 * An extension of the BaseGifDrawable class that allows stepping through the frames of the GIF
 * without updating the View, to facilitate decoding the GIF for exporting purposes.
 */
public class ReactionGifDrawable extends BaseGifDrawable {
    private final ReactionMetadata mMetadata;
    private boolean mSteppingEnabled;
    private org.chromium.base.Callback<Void> mStepCallback;

    public ReactionGifDrawable(
            ReactionMetadata metadata, BaseGifImage gifImage, Bitmap.Config bitmapConfig) {
        super(gifImage, bitmapConfig);
        mMetadata = metadata;
    }

    /**
     * Enables or disables stepping mode for this drawable. While stepping is enabled, the drawable
     * won't be animated and won't invalidate itself, letting callers control when to advance the
     * animation to the next frame and where to draw it.
     *
     * <p><b>Note:</b> enabling stepping mode immediately stops the animation and resets the
     * drawable back to the first frame.
     *
     * @param steppingEnabled True to enable stepping mode, false to disable it.
     */
    public void setSteppingEnabled(boolean steppingEnabled) {
        if (mSteppingEnabled == steppingEnabled) {
            return;
        }

        mSteppingEnabled = steppingEnabled;
        if (mSteppingEnabled) {
            stop();
            reset();
        }
    }

    /**
     * Decodes the next frame of the animation and invokes the given callback when the bitmap of the
     * next frame is ready to be drawn.
     * @param listener The callback to invoke when the drawable is done decoding the next frame.
     * @return A boolean indicating whether the step was a success or not.
     */
    public boolean step(org.chromium.base.Callback<Void> listener) {
        if (mStepCallback != null) {
            return false;
        }
        mStepCallback = listener;
        setSteppingEnabled(true);
        run();
        return true;
    }

    public ReactionMetadata getMetadata() {
        return mMetadata;
    }

    /**
     * Restarts the animation from the first frame.
     */
    public void resetAnimation() {
        reset();
    }

    @Override
    protected void postProcessFrame(Bitmap bitmap) {
        // A frame has just been decoded. If stepping is enabled, notify the listener that a step
        // has completed.
        if (mSteppingEnabled && mStepCallback != null) {
            org.chromium.base.Callback<Void> cb = mStepCallback;
            mStepCallback = null;
            cb.onResult(null);
        }
    }

    @Override
    public void start() {
        // Only start the animation if stepping isn't enabled.
        if (mSteppingEnabled) {
            return;
        }
        super.start();
    }

    @Override
    public void invalidateSelf() {
        // If stepping is enabled, do not invalidate the drawable, since there is no intention to
        // actually update the view.
        if (mSteppingEnabled) {
            return;
        }

        super.invalidateSelf();
    }
}
