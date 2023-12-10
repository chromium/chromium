// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share.qrcode;

import com.google.android.material.tabs.TabLayout;

import org.chromium.ui.base.WindowAndroid;

import java.util.ArrayList;

/**
 * Listener that tracks which tab the user is currently viewing. This is used to turn
 * the camera on and off.
 */
public class TabLayoutPageListener extends TabLayout.TabLayoutOnPageChangeListener {
    private ArrayList<QrCodeDialogTab> mTabs;
    private int mVisibleTab;

    /**
     * @param tabLayout The tabLayout displayed to the user.
     * @param tabs The set of corresponding tabs.
     */
    public TabLayoutPageListener(TabLayout tabLayout, ArrayList<QrCodeDialogTab> tabs) {
        super(tabLayout);
        mTabs = tabs;
        // By default the first tab should be visible
        mVisibleTab = 0;
    }

    /**
     * Update to perform based on the user switched tabs. Pauses all other tabs and
     * resumes the selected tab.
     */
    @Override
    public void onPageSelected(int position) {
        mVisibleTab = position;
        super.onPageSelected(mVisibleTab);

        for (int i = 0; i < mTabs.size(); i++) {
            if (mVisibleTab == i) {
                mTabs.get(i).onResume();
            } else {
                // Let the other tabs know that they are no longer in the foreground and pause
                // them.
                mTabs.get(i).onPause();
            }
        }
    }

    /**
     * Called to resume the selected tab. Called when the user navigates away from the
     * entire dialog and comes back.
     */
    public void resumeSelectedTab() {
        mTabs.get(mVisibleTab).onResume();
    }

    /**
     * Called when the fragment's underlying AndroidPermissionDelegate is updated.
     * Propagates the given AndroidPermissionDelegate to all of the tabs.
     * @param windowAndroid The updated WindowAndroid.
     */
    public void updatePermissions(WindowAndroid windowAndroid) {
        for (QrCodeDialogTab tab : mTabs) {
            tab.updatePermissions(windowAndroid);
        }
    }

    /**
     * Pause all the tabs. Note that we don't update the visible tab since
     * the dialog will continue to display that when the resumes the dialog.
     */
    public void pauseAllTabs() {
        for (QrCodeDialogTab tab : mTabs) {
            tab.onPause();
        }
    }
}
