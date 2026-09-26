// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.support_lib_glue;

import android.graphics.Canvas;
import android.view.MotionEvent;

import com.android.webview.chromium.WebSurface;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.support_lib_boundary.WebSurfaceBoundaryInterface;
import org.chromium.support_lib_boundary.util.BoundaryInterfaceReflectionUtil;

import java.lang.reflect.InvocationHandler;

/** Adapter for WebSurfaceBoundaryInterface. */
@NullMarked
class SupportLibWebSurfaceAdapter implements WebSurfaceBoundaryInterface {
    private final WebSurface mWebSurface;

    public SupportLibWebSurfaceAdapter(WebSurface webSurface) {
        mWebSurface = webSurface;
    }

    @Override
    public void setWebContent(
            @Nullable /* WebContentBoundaryInterface */ InvocationHandler webContent) {
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

    @Override
    public void draw(Canvas canvas) {
        mWebSurface.draw(canvas);
    }

    @Override
    public void setSize(int width, int height) {
        mWebSurface.setSize(width, height);
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        return mWebSurface.onTouchEvent(event);
    }

    @Override
    public void destroy() {
        mWebSurface.destroy();
    }
}
