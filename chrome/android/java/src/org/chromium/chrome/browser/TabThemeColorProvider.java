// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import org.chromium.chrome.browser.ActivityTabProvider.ActivityTabTabObserver;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabThemeColorHelper;
import org.chromium.chrome.browser.toolbar.ThemeColorProvider;

/** A ThemeColorProvider for the current tab's theming. */
public class TabThemeColorProvider extends ThemeColorProvider {
    /** The {@link sActivityTabTabObserver} used to know when the active tab color changed. */
    private ActivityTabTabObserver mActivityTabTabObserver;

    public TabThemeColorProvider(Context context) {
        super(context);
    }

    /**
     * @param provider A means of getting the activity's tab.
     */
    public void setActivityTabProvider(ActivityTabProvider provider) {
        mActivityTabTabObserver = new ActivityTabTabObserver(provider) {
            @Override
            public void onObservingDifferentTab(Tab tab, boolean hint) {
                if (tab == null) return;
                updatePrimaryColor(TabThemeColorHelper.getColor(tab), false);
            }

            @Override
            public void onDidChangeThemeColor(Tab tab, int color) {
                updatePrimaryColor(color, true);
            }
        };
    }

    @Override
    public void destroy() {
        super.destroy();
        if (mActivityTabTabObserver != null) {
            mActivityTabTabObserver.destroy();
            mActivityTabTabObserver = null;
        }
    }
}
