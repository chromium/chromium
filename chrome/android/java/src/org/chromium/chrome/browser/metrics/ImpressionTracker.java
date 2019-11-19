// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.metrics;

import android.graphics.Rect;
import android.support.v4.view.ViewCompat;
import android.view.View;
import android.view.ViewParent;
import android.view.ViewTreeObserver;

import androidx.annotation.Nullable;

/**
 * A class that helps with tracking impressions.
 */
public class ImpressionTracker
        implements ViewTreeObserver.OnPreDrawListener, View.OnAttachStateChangeListener {
    /**
     * The Listener will be called back on an impression, which is defined as a given part of the
     * view's height being visible (defaults to 2/3 of the view's height, can be configured by
     * {@code setImpressionThreshold()}).
     *
     * @see #setListener
     */
    public interface Listener {
        /**
         * The tracked view is being shown (a given part of its height is visible).
         */
        void onImpression();
    }

    /** The currently tracked View. */
    private final View mView;
    private @Nullable Listener mListener;
    private int mImpressionThresholdPx;

    /**
     * Creates a new instance tracking the given {@code view} as soon as and while a listener is
     * attached. Note that the view is final but the listener can be set and reset during the
     * lifetime of this object.
     * @param view The View to track.
     */
    public ImpressionTracker(View view) {
        mView = view;
    }

    /**
     * Sets the listener and starts tracking the view, or stops tracking by passing null.
     * Changing the listener while this object is tracking is not allowed; tracking has to be
     * stopped first.
     * @param listener The impression listener, or null to stop tracking.
     */
    public void setListener(@Nullable Listener listener) {
        assert listener == null || mListener == null;
        if (mListener != null) detach();
        mListener = listener;
        if (mListener != null) attach();
    }

    /**
     * Sets a custom threshold that defines "impression".
     * @param impressionThresholdPx Number of pixels of height of the view that need to be visible.
     */
    public void setImpressionThreshold(int impressionThresholdPx) {
        assert impressionThresholdPx > 0;
        mImpressionThresholdPx = impressionThresholdPx;
    }

    /**
     * Registers listeners for the current view.
     */
    private void attach() {
        // Listen to onPreDraw() only if the view is potentially visible (attached to the window).
        mView.addOnAttachStateChangeListener(this);
        if (ViewCompat.isAttachedToWindow(mView)) {
            mView.getViewTreeObserver().addOnPreDrawListener(this);
        }
    }

    /**
     * Unregisters the listeners for the current view.
     */
    private void detach() {
        mView.removeOnAttachStateChangeListener(this);
        if (ViewCompat.isAttachedToWindow(mView)) {
            mView.getViewTreeObserver().removeOnPreDrawListener(this);
        }
    }

    @Override
    public void onViewAttachedToWindow(View v) {
        mView.getViewTreeObserver().addOnPreDrawListener(this);
    }

    @Override
    public void onViewDetachedFromWindow(View v) {
        mView.getViewTreeObserver().removeOnPreDrawListener(this);
    }

    @Override
    public boolean onPreDraw() {
        ViewParent parent = mView.getParent();
        if (parent != null) {
            Rect rect = new Rect(0, 0, mView.getWidth(), mView.getHeight());

            int impressionThresholdPx = mImpressionThresholdPx;
            // If no threshold is specified, track impression if at least 2/3 of the view is
            // visible.
            if (impressionThresholdPx == 0) impressionThresholdPx = 2 * mView.getHeight() / 3;

            // |getChildVisibleRect| returns false when the view is empty, which may happen when
            // dismissing or reassigning a View. In this case |rect| appears to be invalid.
            if (parent.getChildVisibleRect(mView, rect, null)
                    && rect.height() >= impressionThresholdPx) {
                mListener.onImpression();
            }
        }
        // Proceed with the current drawing pass.
        return true;
    }
}
