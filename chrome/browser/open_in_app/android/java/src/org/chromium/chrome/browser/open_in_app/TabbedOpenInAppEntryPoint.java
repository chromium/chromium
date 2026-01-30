// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import android.content.Context;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.OmniboxChipManager;
import org.chromium.chrome.browser.tab.Tab;

/** Entry point for Open in App in tabbed activity. */
@NullMarked
public class TabbedOpenInAppEntryPoint extends OpenInAppEntryPoint {
    private final OmniboxChipManager mOmniboxChipManager;

    /**
     * Constructor for this class.
     *
     * @param tabSupplier Supplier of the current tab.
     * @param omniboxChipManager The {@link OmniboxChipManager} to manage the open in app chip.
     * @param context The {@link Context} to get resources from.
     */
    public TabbedOpenInAppEntryPoint(
            NullableObservableSupplier<Tab> tabSupplier,
            OmniboxChipManager omniboxChipManager,
            Context context) {
        super(tabSupplier, context);

        mOmniboxChipManager = omniboxChipManager;

        onOpenInAppInfoChanged(getOpenInAppInfoForMenuItem());
    }

    @Override
    public void onOpenInAppInfoChanged(OpenInAppDelegate.@Nullable OpenInAppInfo openInAppInfo) {
        super.onOpenInAppInfoChanged(openInAppInfo);

        if (openInAppInfo == null) {
            mOmniboxChipManager.dismissChip();
        } else {
            mOmniboxChipManager.showChip(
                    mContext.getString(R.string.open_in_app),
                    openInAppInfo.appIcon,
                    openInAppInfo.action);
        }
    }
}
