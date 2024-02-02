// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;

import org.chromium.base.supplier.Supplier;
import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.profiles.Profile;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tasks.tab_management.TabSwitcher;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

import java.util.List;

/**
 * An implementation of the {@link QuickDeleteDelegate} to handle quick delete operations
 * for Chrome.
 */
public class QuickDeleteDelegateImpl extends QuickDeleteDelegate {
    /** {@link SettingsLauncher} used to launch the Clear browsing data settings fragment. */
    private final SettingsLauncher mSettingsLauncher = new SettingsLauncherImpl();

    private final @NonNull Supplier<Profile> mProfileSupplier;
    private final @NonNull Supplier<TabSwitcher> mTabSwitcherSupplier;

    /**
     * @param profileSupplier A supplier for {@link Profile} that owns the data being deleted.
     * @param tabSwitcherSupplier A supplier for {@link TabSwitcher} interface that will be used to
     *     trigger the Quick Delete animation.
     */
    public QuickDeleteDelegateImpl(
            @NonNull Supplier<Profile> profileSupplier,
            @NonNull Supplier<TabSwitcher> tabSwitcherSupplier) {
        mProfileSupplier = profileSupplier;
        mTabSwitcherSupplier = tabSwitcherSupplier;
    }

    @Override
    public void performQuickDelete(@NonNull Runnable onDeleteFinished, @TimePeriod int timePeriod) {
        Profile profile = mProfileSupplier.get().getOriginalProfile();
        BrowsingDataBridge.getForProfile(profile)
                .clearBrowsingData(
                        onDeleteFinished::run,
                        new int[] {
                            BrowsingDataType.HISTORY,
                            BrowsingDataType.COOKIES,
                            BrowsingDataType.CACHE
                        },
                        timePeriod);
    }

    /**
     * @return {@link SettingsLauncher} used to launch the Clear browsing data settings fragment.
     */
    @Override
    SettingsLauncher getSettingsLauncher() {
        return mSettingsLauncher;
    }

    @Override
    void showQuickDeleteAnimation(@NonNull Runnable onAnimationEnd, @NonNull List<Tab> tabs) {
        @Nullable TabSwitcher tabSwitcher = mTabSwitcherSupplier.get();
        if (tabSwitcher == null) {
            onAnimationEnd.run();
            return;
        }
        tabSwitcher.showQuickDeleteAnimation(onAnimationEnd, tabs);
    }
}
