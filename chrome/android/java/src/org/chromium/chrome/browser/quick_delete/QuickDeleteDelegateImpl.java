// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.quick_delete;

import androidx.annotation.NonNull;

import org.chromium.chrome.browser.browsing_data.BrowsingDataBridge;
import org.chromium.chrome.browser.browsing_data.BrowsingDataType;
import org.chromium.chrome.browser.browsing_data.TimePeriod;
import org.chromium.chrome.browser.settings.SettingsLauncherImpl;
import org.chromium.components.browser_ui.settings.SettingsLauncher;

/**
 * An implementation of the {@link QuickDeleteDelegate} to handle quick delete operations
 * for Chrome.
 */
public class QuickDeleteDelegateImpl extends QuickDeleteDelegate {
    /** {@link SettingsLauncher} used to launch the Clear browsing data settings fragment. */
    private final SettingsLauncher mSettingsLauncher = new SettingsLauncherImpl();

    @Override
    public void performQuickDelete(@NonNull Runnable onDeleteFinished, @TimePeriod int timePeriod) {
        BrowsingDataBridge.getInstance().clearBrowsingData(
                onDeleteFinished::run,
                new int[] {
                        BrowsingDataType.HISTORY, BrowsingDataType.COOKIES, BrowsingDataType.CACHE
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
}
