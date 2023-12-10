// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.compositor.CompositorViewHolder;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.webxr.VrCompositorDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/** Concrete, Chrome-specific implementation of VrCompositorDelegate interface. */
public class VrCompositorDelegateImpl implements VrCompositorDelegate {
    private CompositorView mCompositorView;
    private ObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    VrCompositorDelegateImpl(WebContents webContents) {
        ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
        Supplier<CompositorViewHolder> compositorViewHolderSupplier =
                activity.getCompositorViewHolderSupplier();
        mCompositorView = compositorViewHolderSupplier.get().getCompositorView();
        mTabModelSelectorSupplier = activity.getTabModelSelectorSupplier();
    }

    @Override
    public void setOverlayImmersiveVrMode(boolean enabled) {
        mCompositorView.setOverlayVrMode(enabled);
    }

    @Override
    public void openNewTab(LoadUrlParams url) {
        mTabModelSelectorSupplier
                .get()
                .openNewTab(
                        url,
                        TabLaunchType.FROM_CHROME_UI,
                        /* parent= */ null,
                        /* incognito= */ false);
    }
}
