// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.partialcustomtab;

import android.view.MotionEvent;
import android.view.View.OnClickListener;

import org.chromium.base.Callback;
import org.chromium.chrome.browser.customtabs.features.toolbar.CustomTabToolbar;

/**
 * A minimal {@link CustomTabToolbar.HandleStrategy} implementation bridging
 * {@link CustomTabToolbar} and {@link CustomTabHeightStrategy} for the closing operation.
 */
public class SimpleHandleStrategy implements CustomTabToolbar.HandleStrategy {
    private final Callback<Runnable> mCloseAnimation;
    private OnClickListener mOnClickCloseListener;

    public SimpleHandleStrategy(Callback<Runnable> closeAnimation) {
        mCloseAnimation = closeAnimation;
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent event) {
        return false;
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return false;
    }

    @Override
    public void setCloseClickHandler(OnClickListener listener) {
        mOnClickCloseListener = listener;
    }

    @Override
    public void startCloseAnimation() {
        mCloseAnimation.onResult(this::close);
    }

    @Override
    public void close() {
        mOnClickCloseListener.onClick(null);
    }

    public OnClickListener getClickCloseHandlerForTesting() {
        return mOnClickCloseListener;
    }
}
