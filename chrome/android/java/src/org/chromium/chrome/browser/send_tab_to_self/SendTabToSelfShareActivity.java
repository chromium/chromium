// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import org.chromium.chrome.browser.ChromeAccessorActivity;
import org.chromium.chrome.browser.share.send_tab_to_self.SendTabToSelfAndroidBridge;
import org.chromium.chrome.browser.tab.Tab;

/**
 * A simple activity that allows Chrome to expose send tab to self as an option in the share menu.
 */
public class SendTabToSelfShareActivity extends ChromeAccessorActivity {
    public static final String SHARE_ACTION = "SendTabToSelfShareActivityShareAction";

    @Override
    protected String getShareAction() {
        return SHARE_ACTION;
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        return SendTabToSelfAndroidBridge.isFeatureAvailable(currentTab.getWebContents());
    }
}
