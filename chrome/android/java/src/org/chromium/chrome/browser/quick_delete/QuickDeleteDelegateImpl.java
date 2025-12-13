// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.multiwindow.MultiInstanceManager.PersistedInstanceType;
import org.chromium.chrome.browser.multiwindow.MultiWindowUtils;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab_ui.TabSwitcher;

import java.util.List;
import java.util.function.Supplier;

/**
 * An implementation of the {@link QuickDeleteDelegate} to handle quick delete operations for
 * Chrome.
 */
@NullMarked
public class QuickDeleteDelegateImpl extends QuickDeleteDelegate {
    private final Supplier<Profile> mProfileSupplier;
    private final Supplier<TabSwitcher> mTabSwitcherSupplier;

    /**
     * @param profileSupplier A supplier for {@link Profile} that owns the data being deleted.
     * @param tabSwitcherSupplier A supplier for {@link TabSwitcher} interface that will be used to
     *     trigger the Quick Delete animation.
     */
    public QuickDeleteDelegateImpl(
            Supplier<Profile> profileSupplier, Supplier<TabSwitcher> tabSwitcherSupplier) {
        mProfileSupplier = profileSupplier;
        mTabSwitcherSupplier = tabSwitcherSupplier;
    }

    @Override
    public void performQuickDelete(Runnable onDeleteFinished, @TimePeriod int timePeriod) {
        Profile profile = mProfileSupplier.get().getOriginalProfile();
        BrowsingDataBridge.getForProfile(profile)
                .clearBrowsingData(
                        onDeleteFinished::run,
                        new int[] {
                            BrowsingDataType.HISTORY,
                            BrowsingDataType.SITE_DATA,
                            BrowsingDataType.CACHE
                        },
                        timePeriod);
    }

    @Override
    void showQuickDeleteAnimation(Runnable onAnimationEnd, List<Tab> tabs) {
        @Nullable TabSwitcher tabSwitcher = mTabSwitcherSupplier.get();
        if (tabSwitcher == null) {
            onAnimationEnd.run();
            return;
        }
        tabSwitcher.showQuickDeleteAnimation(onAnimationEnd, tabs);
    }

    @Override
    boolean isInMultiWindowMode() {
        return MultiWindowUtils.getInstanceCountWithFallback(PersistedInstanceType.ANY) > 1;
    }
}
