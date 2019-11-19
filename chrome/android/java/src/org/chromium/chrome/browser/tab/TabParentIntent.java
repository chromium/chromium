// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Intent;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;

/**
 * A holder of {@link Intent} object to be used to bring back the parent {@link Activity}
 * from which the associated tab was opened.
 */
public final class TabParentIntent extends EmptyTabObserver implements UserData {
    private static final Class<TabParentIntent> USER_DATA_KEY = TabParentIntent.class;

    private final Tab mTab;

    /**
     * If the associated tab was opened from another tab in another Activity, this is the Intent
     * that can be fired to bring the parent Activity back.
     */
    private Intent mParentIntent;

    public static TabParentIntent from(Tab tab) {
        UserDataHost host = tab.getUserDataHost();
        TabParentIntent tabParentIntent = host.getUserData(USER_DATA_KEY);
        if (tabParentIntent == null) {
            tabParentIntent = host.setUserData(USER_DATA_KEY, new TabParentIntent(tab));
        }
        return tabParentIntent;
    }

    private TabParentIntent(Tab tab) {
        mTab = tab;
        mTab.addObserver(this);
    }

    @Override
    public void onCloseContents(Tab tab) {
        boolean isSelected = TabModelSelector.from(mTab).getCurrentTab() == tab;

        // If the parent Tab belongs to another Activity, fire the Intent to bring it back.
        if (isSelected && mParentIntent != null && tab.getActivity().getIntent() != mParentIntent) {
            tab.getActivity().startActivity(mParentIntent);
        }
    }

    public void set(Intent intent) {
        mParentIntent = intent;
    }

    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }
}
