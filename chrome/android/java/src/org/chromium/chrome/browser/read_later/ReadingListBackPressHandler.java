// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.read_later;

import org.chromium.base.lifetime.Destroyable;
import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.ObservableSupplierImpl;
import org.chromium.chrome.browser.ActivityTabProvider;
import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabLaunchType;
import org.chromium.components.browser_ui.widget.gesture.BackPressHandler;
import org.chromium.content_public.browser.WebContents;

/**
 * A {@link BackPressHandler} intercepting back press when the current selected tab is launched
 * from reading list.
 */
public class ReadingListBackPressHandler implements BackPressHandler, Destroyable {
    private final ObservableSupplierImpl<Boolean> mBackPressChangedSupplier =
            new ObservableSupplierImpl<>();
    private final ActivityTabProvider mActivityTabProvider;
    private final ActivityTabTabObserver mActivityTabTabObserver;

    public ReadingListBackPressHandler(ActivityTabProvider activityTabProvider) {
        mActivityTabProvider = activityTabProvider;
        mActivityTabTabObserver = new ActivityTabTabObserver(mActivityTabProvider, true) {
            @Override
            protected void onObservingDifferentTab(Tab tab, boolean hint) {
                onBackPressStateChanged();
            }
        };
    }

    @Override
    public void handleBackPress() {
        Tab tab = mActivityTabProvider.get();
        ReadingListUtils.showReadingList(tab.isIncognito());
        WebContents webContents = tab.getWebContents();
        if (webContents != null) webContents.dispatchBeforeUnload(false);
    }

    @Override
    public ObservableSupplier<Boolean> getHandleBackPressChangedSupplier() {
        return mBackPressChangedSupplier;
    }

    @Override
    public void destroy() {
        mActivityTabTabObserver.destroy();
    }

    private void onBackPressStateChanged() {
        Tab tab = mActivityTabProvider.get();
        mBackPressChangedSupplier.set(
                tab != null && tab.getLaunchType() == TabLaunchType.FROM_READING_LIST);
    }
}
