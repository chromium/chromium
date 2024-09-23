// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.layouts.LayoutStateProvider;
import org.chromium.chrome.browser.layouts.LayoutType;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/** Decides to show a next tab by location if overview is open, or by hierarchy otherwise. */
public class ChromeNextTabPolicySupplier implements NextTabPolicySupplier {
    private LayoutStateProvider mLayoutStateProvider;

    public ChromeNextTabPolicySupplier(
            OneshotSupplier<LayoutStateProvider> layoutStateProviderSupplier) {
        layoutStateProviderSupplier.onAvailable(this::setLayoutStateProvider);
    }

    private void setLayoutStateProvider(@NonNull LayoutStateProvider layoutStateProvider) {
        mLayoutStateProvider = layoutStateProvider;
    }

    @Override
    public @NextTabPolicy Integer get() {
        if (mLayoutStateProvider != null
                && mLayoutStateProvider.isLayoutVisible(LayoutType.TAB_SWITCHER)) {
            return NextTabPolicy.LOCATIONAL;
        } else {
            return NextTabPolicy.HIERARCHICAL;
        }
    }

    @VisibleForTesting
    LayoutStateProvider getLayoutStateProvider() {
        return mLayoutStateProvider;
    }
}
