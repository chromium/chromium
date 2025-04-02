// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.share;

import android.app.Activity;

import androidx.annotation.Nullable;

import org.chromium.base.CallbackUtils;
import org.chromium.build.annotations.NullMarked;
import org.chromium.chrome.browser.data_sharing.DataSharingTabManager;
import org.chromium.chrome.browser.share.android_share_sheet.TabGroupSharingController;
import org.chromium.chrome.browser.share.share_sheet.ChromeOptionShareCallback;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.components.collaboration.CollaborationServiceShareOrManageEntryPoint;

/** Implementation of TabGroupSharingController. Mostly a wrapper around DataSharingTabManager. */
@NullMarked
class TabGroupSharingControllerImpl implements TabGroupSharingController {
    private final @Nullable DataSharingTabManager mDataSharingTabManager;

    TabGroupSharingControllerImpl(@Nullable DataSharingTabManager dataSharingTabManager) {
        mDataSharingTabManager = dataSharingTabManager;
    }

    @Override
    public boolean isAvailableForTab(Tab tab) {
        return !tab.isCustomTab()
                && !tab.isOffTheRecord()
                && mDataSharingTabManager != null
                && mDataSharingTabManager.isCreationEnabled();
    }

    @Override
    public void shareAsTabGroup(
            Activity activity, ChromeOptionShareCallback chromeOptionShareCallback, Tab tab) {
        assert mDataSharingTabManager != null;
        mDataSharingTabManager.createTabGroupAndShare(
                activity,
                tab,
                CollaborationServiceShareOrManageEntryPoint.ANDROID_SHARE_SHEET_EXTRA,
                CallbackUtils.emptyCallback());
    }
}
