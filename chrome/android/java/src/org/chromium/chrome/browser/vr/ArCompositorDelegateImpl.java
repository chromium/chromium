// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import android.view.MotionEvent;
import android.view.ViewGroup;

import androidx.annotation.NonNull;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.R;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.webxr.ArCompositorDelegate;
import org.chromium.content_public.browser.WebContents;

/** Concrete, Chrome-specific implementation of ArCompositorDelegate interface. */
public class ArCompositorDelegateImpl implements ArCompositorDelegate {
    private ChromeActivity mActivity;
    private CompositorViewHolder mCompositorViewHolder;
    private CompositorView mCompositorView;

    ArCompositorDelegateImpl(WebContents webContents) {
        mActivity = ChromeActivity.fromWebContents(webContents);

        Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                mActivity.getCompositorViewHolderSupplier();
        mCompositorViewHolder = compositorViewHolderSupplier.get();
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

    @Override
    public @NonNull ViewGroup getArSurfaceParent() {
        // the ar_view_holder is a FrameLayout, up-cast to a ViewGroup.
        return mActivity.findViewById(R.id.ar_view_holder);
    }

    @Override
    public boolean shouldToggleArSurfaceParentVisibility() {
        return true;
    }
}
