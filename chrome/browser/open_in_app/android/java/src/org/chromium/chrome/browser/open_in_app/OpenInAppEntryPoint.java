// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.open_in_app;

import org.chromium.base.supplier.NullableObservableSupplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabSupplierObserver;

/** Abstract base entry point class for Open in App. */
@NullMarked
public abstract class OpenInAppEntryPoint
        implements OpenInAppMenuItemProvider, OpenInAppDelegate.Observer {
    private final TabSupplierObserver mTabSupplierObserver;
    private @Nullable Tab mCurrentTab;
    private @Nullable OpenInAppDelegate mOpenInAppDelegate;
    private OpenInAppDelegate.@Nullable OpenInAppInfo mOpenInAppInfo;

    /**
     * Constructor for this class.
     *
     * @param tabSupplier A supplier that notifies of tab changes.
     */
    public OpenInAppEntryPoint(NullableObservableSupplier<Tab> tabSupplier) {
        mTabSupplierObserver =
                new TabSupplierObserver(tabSupplier, /* shouldTrigger= */ true) {
                    @Override
                    protected void onObservingDifferentTab(@Nullable Tab tab) {
                        mCurrentTab = tab;

                        if (mCurrentTab != null) {
                            mOpenInAppDelegate = OpenInAppDelegate.from(mCurrentTab);
                        }
                        if (mOpenInAppDelegate != null) {
                            mOpenInAppDelegate.addOpenInAppInfoObserver(OpenInAppEntryPoint.this);
                        }

                        onOpenInAppInfoChanged(
                                mOpenInAppDelegate != null
                                        ? mOpenInAppDelegate.getCurrentOpenInAppInfo()
                                        : null);
                    }
                };
    }

    public void destroy() {
        mTabSupplierObserver.destroy();

        if (mOpenInAppDelegate != null) {
            mOpenInAppDelegate.removeOpenInAppInfoObserver(this);
        }

        mOpenInAppInfo = null;
    }

    @Override
    public void onOpenInAppInfoChanged(OpenInAppDelegate.@Nullable OpenInAppInfo openInAppInfo) {
        mOpenInAppInfo = openInAppInfo;
    }

    @Override
    public OpenInAppDelegate.@Nullable OpenInAppInfo getOpenInAppInfo() {
        return mOpenInAppInfo;
    }
}
