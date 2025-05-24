// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import android.app.Activity;
import android.content.Intent;

import org.chromium.base.UserData;
import org.chromium.base.UserDataHost;
import org.chromium.base.supplier.Supplier;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;

/**
 * A holder of {@link Intent} object to be used to bring back the parent {@link Activity} from which
 * the associated tab was opened.
 */
@NullMarked
public final class TabParentIntent extends EmptyTabObserver implements UserData {
    private static final Class<TabParentIntent> USER_DATA_KEY = TabParentIntent.class;

    private final Tab mTab;

    private @Nullable Supplier<@Nullable Tab> mCurrentTab;

    /**
     * If the associated tab was opened from another tab in another Activity, this is the Intent
     * that can be fired to bring the parent Activity back.
     */
    private @Nullable Intent mParentIntent;

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
        boolean isSelected = mCurrentTab != null && mCurrentTab.get() == tab;

        // If the parent Tab belongs to another Activity, fire the Intent to bring it back.
        if (isSelected && mParentIntent != null) {
            Activity activity = TabUtils.getActivity(tab);
            if (activity != null && activity.getIntent() != mParentIntent) {
                activity.startActivity(mParentIntent);
            }
        }
    }

    public TabParentIntent set(Intent intent) {
        mParentIntent = intent;
        return this;
    }

    /** Set the supplier of the current tab. */
    public void setCurrentTab(Supplier<@Nullable Tab> currentTab) {
        mCurrentTab = currentTab;
    }

    @Override
    public void destroy() {
        mTab.removeObserver(this);
    }
}
