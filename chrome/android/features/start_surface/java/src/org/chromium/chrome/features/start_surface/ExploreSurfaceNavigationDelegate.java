// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.features.start_surface;

import androidx.annotation.Nullable;

import org.chromium.base.metrics.RecordUserAction;
import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.native_page.NativePageNavigationDelegate;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.ReturnToChromeUtil;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.ui.mojom.WindowOpenDisposition;

/** Implementation of the {@link NativePageNavigationDelegate} for the explore surface. */
class ExploreSurfaceNavigationDelegate implements NativePageNavigationDelegate {
    private static final String NEW_TAB_URL_HELP = "https://support.google.com/chrome/?p=new_tab";

    private final Supplier<Tab> mParentTabSupplier;

    ExploreSurfaceNavigationDelegate(Supplier<Tab> parentTabSupplier) {
        mParentTabSupplier = parentTabSupplier;
    }

    @Override
    public boolean isOpenInNewWindowEnabled() {
        return false;
    }

    @Override
    @Nullable
    public Tab openUrl(int windowOpenDisposition, LoadUrlParams loadUrlParams) {
        Tab newTab = ReturnToChromeUtil.handleLoadUrlFromStartSurface(loadUrlParams,
                windowOpenDisposition == WindowOpenDisposition.NEW_BACKGROUND_TAB,
                windowOpenDisposition == WindowOpenDisposition.OFF_THE_RECORD,
                mParentTabSupplier.get());
        assert newTab != null;
        RecordUserAction.record("ContentSuggestions.Feed.CardAction.Open.StartSurface");
        return newTab;
    }

    @Override
    @Nullable
    public Tab openUrlInGroup(int windowOpenDisposition, LoadUrlParams loadUrlParams) {
        // 'open in group' has been disabled in crrev.com/c/2885469. We should never reach this
        // method.
        assert false; // NOTREACHED.
        return null;
    }
}
