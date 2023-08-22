// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.components.webxr.VrCompositorDelegate;
import org.chromium.content_public.browser.WebContents;

/**
 * Concrete, Chrome-specific implementation of VrCompositorDelegate interface.
 */
public class VrCompositorDelegateImpl implements VrCompositorDelegate {
    private CompositorView mCompositorView;

    VrCompositorDelegateImpl(WebContents webContents) {
        Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                ChromeActivity.fromWebContents(webContents).getCompositorViewHolderSupplier();
        mCompositorView = compositorViewHolderSupplier.get().getCompositorView();
    }

    @Override
    public void setOverlayImmersiveVrMode(boolean enabled) {
        mCompositorView.setOverlayVrMode(enabled);
    }
}
