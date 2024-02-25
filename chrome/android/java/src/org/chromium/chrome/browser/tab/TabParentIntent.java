// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.content.Intent;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.Supplier;

/**
 * A holder of {@link Intent} object to be used to bring back the parent {@link Activity}
 * from which the associated tab was opened.
 */
public final class TabParentIntent extends EmptyTabObserver implements UserData {
    private static final Class<TabParentIntent> USER_DATA_KEY = TabParentIntent.class;

    private final Tab mTab;

    private Supplier<Tab> mCurrentTab;

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
        boolean isSelected = mCurrentTab.get() == tab;

        // If the parent Tab belongs to another Activity, fire the Intent to bring it back.
        if (isSelected
                && mParentIntent != null
                && TabUtils.getActivity(tab).getIntent() != mParentIntent) {
            TabUtils.getActivity(tab).startActivity(mParentIntent);
        }
    }

    public TabParentIntent set(Intent intent) {
        mParentIntent = intent;
        return this;
    }

    /** Set the supplier of the current Tab. */
    public void setCurrentTab(Supplier<Tab> currentTab) {
        mCurrentTab = currentTab;
    }

    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }
}
