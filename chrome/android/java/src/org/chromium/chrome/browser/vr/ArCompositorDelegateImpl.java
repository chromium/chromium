// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.view.MotionEvent;

import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.webxr.ArCompositorDelegate;
import org.chromium.content_public.browser.WebContents;

/**
 * Concrete, Chrome-specific implementation of ArCompositorDelegate interface.
 */
public class ArCompositorDelegateImpl implements ArCompositorDelegate {
    private ChromeActivity mActivity;
    private CompositorViewHolder mCompositorViewHolder;
    private CompositorView mCompositorView;

    ArCompositorDelegateImpl(WebContents webContents) {
        mActivity = ChromeActivity.fromWebContents(webContents);
        mCompositorViewHolder = mActivity.getCompositorViewHolder();
        mCompositorView = mCompositorViewHolder.getCompositorView();
    }

    @Override
    public void setOverlayImmersiveArMode(boolean enabled, boolean domSurfaceNeedsConfiguring) {
        mCompositorView.setOverlayImmersiveArMode(enabled, domSurfaceNeedsConfiguring);
    }

    @Override
    public void dispatchTouchEvent(MotionEvent ev) {
        mCompositorViewHolder.dispatchTouchEvent(ev);
    }
}
