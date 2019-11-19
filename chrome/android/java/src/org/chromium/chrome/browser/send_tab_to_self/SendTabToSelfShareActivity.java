// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import androidx.annotation.VisibleForTesting;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.send_tab_to_self.SendTabToSelfMetrics.SendTabToSelfShareClickResult;
import org.chromium.chrome.browser.share.ShareActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.widget.bottomsheet.BottomSheetContent;
import org.chromium.content_public.browser.NavigationEntry;

/**
 * A simple activity that allows Chrome to expose send tab to self as an option in the share menu.
 */
public class SendTabToSelfShareActivity extends ShareActivity {
    @Override
    protected void handleShareAction(ChromeActivity triggeringActivity) {
        Tab tab = triggeringActivity.getActivityTabProvider().get();
        if (tab == null) return;

        NavigationEntry entry = tab.getWebContents().getNavigationController().getVisibleEntry();
        if (entry == null || triggeringActivity.getBottomSheetController() == null) {
            return;
        }

        SendTabToSelfShareClickResult.recordClickResult(
                SendTabToSelfShareClickResult.ClickType.SHOW_DEVICE_LIST);
        triggeringActivity.getBottomSheetController().requestShowContent(
                createBottomSheetContent(triggeringActivity, entry), true);
        // TODO(crbug.com/968246): Remove the need to call this explicitly and instead have it
        // automatically show since PeekStateEnabled is set to false.
        triggeringActivity.getBottomSheetController().expandSheet();
    }

    @VisibleForTesting
    BottomSheetContent createBottomSheetContent(ChromeActivity activity, NavigationEntry entry) {
        return new DevicePickerBottomSheetContent(activity, entry);
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        boolean shouldShow =
                SendTabToSelfAndroidBridge.isFeatureAvailable(currentTab.getWebContents());
        if (shouldShow) {
            SendTabToSelfShareClickResult.recordClickResult(
                    SendTabToSelfShareClickResult.ClickType.SHOW_ITEM);
        }
        return shouldShow;
    }
}
