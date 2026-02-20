// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import static org.chromium.build.NullUtil.assertNonNull;

import android.content.Context;
import android.graphics.drawable.Drawable;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.omnibox.OmniboxChipManager;
import org.chromium.chrome.browser.tab.Tab;

/** Entry point for Open in App in tabbed activity. */
@NullMarked
public class TabbedOpenInAppEntryPoint extends OpenInAppEntryPoint {
    private final OmniboxChipManager mOmniboxChipManager;
    private boolean mShowingChip;

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
    }

    @Override
    protected void onOpenInAppInfoChanged(OpenInAppDelegate.@Nullable OpenInAppInfo openInAppInfo) {
        if (openInAppInfo == null && mOmniboxChipManager.isChipShown()) {
            mOmniboxChipManager.dismissChip();
        } else if (openInAppInfo != null) {
            Drawable icon = openInAppInfo.appIcon;
            if (icon == null) {
                icon = assertNonNull(mContext.getDrawable(R.drawable.ic_open_in_new_20dp));
            }

            mOmniboxChipManager.showChip(
                    mContext.getString(R.string.open_in_app),
                    icon,
                    // TODO(crbug.com/450253146): Add a real content description.
                    mContext.getString(R.string.open_in_app),
                    openInAppInfo.action,
                    new OmniboxChipManager.ChipCallback() {
                        @Override
                        public void onChipHidden() {
                            mShowingChip = false;
                        }

                        @Override
                        public void onChipShown() {
                            mShowingChip = true;
                        }
                    });
        }
    }

    @Override
    public OpenInAppDelegate.@Nullable OpenInAppInfo getOpenInAppInfoForMenuItem() {
        if (mShowingChip) return null;

        return super.getOpenInAppInfoForMenuItem();
    }
}
