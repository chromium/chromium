// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.view.View;
import android.view.ViewTreeObserver;

import java.util.ArrayList;

/** Used to register listeners that can be notified of changes to the position of a view. */
public class ViewPositionObserver {
    /** Called during predraw if the position of the underlying view has changed. */
    public interface Listener {
        void onPositionChanged(int positionX, int positionY);
    }

    private View mView;
    // Absolute position of the container view relative to its parent window.
    private final int[] mPosition = new int[2];

    private final ArrayList<Listener> mListeners;
    private ViewTreeObserver.OnPreDrawListener mPreDrawListener;

    /** @param view The view to observe. */
    public ViewPositionObserver(View view) {
        mView = view;
        mListeners = new ArrayList<Listener>();
        updatePosition();
        mPreDrawListener =
                new ViewTreeObserver.OnPreDrawListener() {
                    @Override
                    public boolean onPreDraw() {
                        updatePosition();
                        return true;
                    }
                };
    }

    /** @return The current x position of the observed view. */
    public int getPositionX() {
        // The stored position may be out-of-date. Get the real current position.
        updatePosition();
        return mPosition[0];
    }

    /** @return The current y position of the observed view. */
    public int getPositionY() {
        // The stored position may be out-of-date. Get the real current position.
        updatePosition();
        return mPosition[1];
    }

    /** Register a listener to be called when the position of the underlying view changes. */
    public void addListener(Listener listener) {
        if (mListeners.contains(listener)) return;

        if (mListeners.isEmpty()) {
            mView.getViewTreeObserver().addOnPreDrawListener(mPreDrawListener);
            updatePosition();
        }

        mListeners.add(listener);
    }

    /** Remove a previously installed listener. */
    public void removeListener(Listener listener) {
        if (!mListeners.contains(listener)) return;

        mListeners.remove(listener);

        if (mListeners.isEmpty()) {
            mView.getViewTreeObserver().removeOnPreDrawListener(mPreDrawListener);
        }
    }

    private void notifyListeners() {
        for (int i = 0; i < mListeners.size(); i++) {
            mListeners.get(i).onPositionChanged(mPosition[0], mPosition[1]);
        }
    }

    private void updatePosition() {
        int previousPositionX = mPosition[0];
        int previousPositionY = mPosition[1];
        mView.getLocationInWindow(mPosition);
        if (mPosition[0] != previousPositionX || mPosition[1] != previousPositionY) {
            notifyListeners();
        }
    }
}
