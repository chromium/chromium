// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.vr;

import static org.chromium.build.NullUtil.assumeNonNull;

import org.chromium.base.supplier.MonotonicObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.compositor.CompositorView;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.components.webxr.VrCompositorDelegate;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.WebContents;

/** Concrete, Chrome-specific implementation of VrCompositorDelegate interface. */
@NullMarked
public class VrCompositorDelegateImpl implements VrCompositorDelegate {
    private final CompositorView mCompositorView;
    private final MonotonicObservableSupplier<TabModelSelector> mTabModelSelectorSupplier;

    VrCompositorDelegateImpl(WebContents webContents) {
        ChromeActivity activity = ChromeActivity.fromWebContents(webContents);
        assumeNonNull(activity);
        mCompositorView =
                assumeNonNull(activity.getCompositorViewHolderSupplier().get()).getCompositorView();
        mTabModelSelectorSupplier = activity.getTabModelSelectorSupplier();
    }

    @Override
    public void setOverlayImmersiveVrMode(boolean enabled) {
        mCompositorView.setOverlayVrMode(enabled);
    }

    @Override
    public void openNewTab(LoadUrlParams url) {
        assumeNonNull(mTabModelSelectorSupplier.get())
                .openNewTab(
                        url,
                        TabLaunchType.FROM_CHROME_UI,
                        /* parent= */ null,
                        /* incognito= */ false);
    }
}
