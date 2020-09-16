// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.app.tabmodel;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.supplier.OneshotSupplier;
import org.chromium.chrome.browser.compositor.layouts.OverviewModeBehavior;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy;
import org.chromium.chrome.browser.tabmodel.NextTabPolicy.NextTabPolicySupplier;

/**
 * Decides to show a next tab by location if overview is open, or by hierarchy otherwise.
 */
public class ChromeNextTabPolicySupplier implements NextTabPolicySupplier {
    private OverviewModeBehavior mOverviewModeBehavior;

    public ChromeNextTabPolicySupplier(
            OneshotSupplier<OverviewModeBehavior> overviewModeControllerObservableSupplier) {
        overviewModeControllerObservableSupplier.onAvailable(this::setOverviewModeBehavior);
    }

    private void setOverviewModeBehavior(@NonNull OverviewModeBehavior overviewModeBehavior) {
        mOverviewModeBehavior = overviewModeBehavior;
    }

    @Override
    public @NextTabPolicy Integer get() {
        if (mOverviewModeBehavior != null && mOverviewModeBehavior.overviewVisible()) {
            return NextTabPolicy.LOCATIONAL;
        } else {
            return NextTabPolicy.HIERARCHICAL;
        }
    }

    @VisibleForTesting
    OverviewModeBehavior getOverviewModeBehavior() {
        return mOverviewModeBehavior;
    }
}
