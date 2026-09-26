// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.graphics.Canvas;
import android.view.MotionEvent;

import org.chromium.android_webview.AwWebSurface;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/** Represents a presentation surface for WebContent in Compose without intermediate View nodes. */
@NullMarked
public class WebSurface {
    // This interface is expected to be expanded in the future.
    public interface EventListener {
        void onInvalidate();
    }

    @Nullable private WebContent mWebContent;
    private final AwWebSurface mAwWebSurface;
    private final WebContent.SurfaceBindingListener mSurfaceBindingListener;

    public WebSurface(EventListener eventListener) {
        assert eventListener != null : "EventListener cannot be null.";
        mAwWebSurface = new AwWebSurface(eventListener::onInvalidate);
        mSurfaceBindingListener =
                awContents -> {
                    if (awContents == null) {
                        mWebContent = null;
                    }
                    mAwWebSurface.setAwContents(awContents);
                };
    }

    public void setWebContent(@Nullable WebContent webContent) {
        if (mWebContent == webContent) {
            return;
        }
        if (mWebContent != null) {
            mWebContent.bindSurface(null);
        }
        if (webContent != null) {
            webContent.bindSurface(mSurfaceBindingListener);
            mWebContent = webContent;
        }
    }

    public void draw(Canvas canvas) {
        mAwWebSurface.draw(canvas);
    }

    public void setSize(int width, int height) {
        mAwWebSurface.setSize(width, height);
    }

    public boolean onTouchEvent(MotionEvent event) {
        return mAwWebSurface.onTouchEvent(event);
    }

    public void destroy() {
        setWebContent(null);
    }
}
