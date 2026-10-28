// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.graphics.Canvas;
import android.view.MotionEvent;

import org.chromium.android_webview.AwWebSurface;
import org.chromium.base.TraceEvent;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;
import org.chromium.support_lib_boundary.web.WebSurfaceBoundaryInterface;
import org.chromium.support_lib_boundary.web.WebSurfaceEvent;
import org.chromium.support_lib_boundary.web.util.WebSurfaceEventPacker;
import org.chromium.support_lib_glue.WebApiCallLogger.WebComposeApiCall;

import java.lang.reflect.InvocationHandler;
import java.util.function.BiConsumer;

/** Adapter for WebSurfaceBoundaryInterface. */
@NullMarked
class SupportLibWebSurfaceAdapter implements WebSurfaceBoundaryInterface {
    private final AwWebSurface mWebSurface;

    public SupportLibWebSurfaceAdapter(
            BiConsumer<@WebSurfaceEvent Integer, @Nullable Object> eventListener) {
        mWebSurface = new AwWebSurface(new EventListenerImpl(eventListener));
    }

    @Override
    public void setWebContent(
            @Nullable /* WebContentBoundaryInterface */ InvocationHandler webContent) {
        try (TraceEvent event =
                TraceEvent.scoped(
                        "WebView.APICall.AndroidX.WebCompose.WEB_SURFACE_SET_WEB_CONTENT")) {
            WebApiCallLogger.recordWebComposeApiCall(WebComposeApiCall.WEB_SURFACE_SET_WEB_CONTENT);
            if (webContent == null) {
                mWebSurface.setWebContent(null);
            } else {
                SupportLibWebContentAdapter contentAdapter =
                        (SupportLibWebContentAdapter)
                                BoundaryInterfaceReflectionUtil.getDelegateFromInvocationHandler(
                                        webContent);
                mWebSurface.setWebContent(contentAdapter.getWebContent());
            }
        }
    }

    @Override
    public void draw(Canvas canvas) {
        // Metrics logging is intentionally omitted on this high-frequency rendering path.
        mWebSurface.draw(canvas);
    }

    @Override
    public void setSize(int width, int height) {
        // Metrics logging is intentionally omitted on this high-frequency layout path.
        mWebSurface.setSize(width, height);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        // Metrics logging is intentionally omitted on this high-frequency input path.
        return mWebSurface.onTouchEvent(event);
    }

    @Override
    public void destroy() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.AndroidX.WebCompose.WEB_SURFACE_DESTROY")) {
            WebApiCallLogger.recordWebComposeApiCall(WebComposeApiCall.WEB_SURFACE_DESTROY);
            mWebSurface.destroy();
        }
    }

    private static class EventListenerImpl implements AwWebSurface.EventListener {
        private final BiConsumer<@WebSurfaceEvent Integer, @Nullable Object> mEventListener;

        EventListenerImpl(BiConsumer<@WebSurfaceEvent Integer, @Nullable Object> eventListener) {
            mEventListener = eventListener;
        }

        @Override
        public void onInvalidate() {
            mEventListener.accept(WebSurfaceEvent.INVALIDATE, null);
        }

        @Override
        public void onOverScroll(float deltaX, float deltaY, float velocityX, float velocityY) {
            mEventListener.accept(
                    WebSurfaceEvent.OVER_SCROLL,
                    WebSurfaceEventPacker.packOverScroll(deltaX, deltaY, velocityX, velocityY));
        }
    }
}
