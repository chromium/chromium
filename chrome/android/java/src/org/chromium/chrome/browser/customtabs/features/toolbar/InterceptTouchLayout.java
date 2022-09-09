// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs.features.toolbar;

import android.annotation.SuppressLint;
import android.content.Context;
import android.util.AttributeSet;
import android.view.GestureDetector;
import android.view.MotionEvent;
import android.widget.FrameLayout;

import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.metrics.RecordUserAction;
import org.chromium.chrome.browser.omnibox.UrlBar;

/**
 * A simple {@link FrameLayout} that prevents its children from getting touch events. This is
 * especially useful to prevent {@link UrlBar} from running custom touch logic since it is read-only
 * in custom tabs.
 */
class InterceptTouchLayout extends FrameLayout {
    private final GestureDetector mGestureDetector;

    public InterceptTouchLayout(Context context, AttributeSet attrs) {
        super(context, attrs);
        mGestureDetector =
                new GestureDetector(getContext(), new GestureDetector.SimpleOnGestureListener() {
                    @Override
                    public boolean onSingleTapConfirmed(MotionEvent e) {
                        if (LibraryLoader.getInstance().isInitialized()) {
                            RecordUserAction.record("CustomTabs.TapUrlBar");
                        }
                        return super.onSingleTapConfirmed(e);
                    }
                }, ThreadUtils.getUiThreadHandler());
    }

    @Override
    public boolean onInterceptTouchEvent(MotionEvent ev) {
        return true;
    }

    @Override
    @SuppressLint("ClickableViewAccessibility")
    public boolean onTouchEvent(MotionEvent event) {
        mGestureDetector.onTouchEvent(event);
        return super.onTouchEvent(event);
    }
}
