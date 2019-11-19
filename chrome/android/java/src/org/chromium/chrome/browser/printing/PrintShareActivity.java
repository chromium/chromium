// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.printing;

import org.chromium.chrome.R;
import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.preferences.Pref;
import org.chromium.chrome.browser.preferences.PrefServiceBridge;
import org.chromium.chrome.browser.share.ShareActivity;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.printing.PrintingController;
import org.chromium.printing.PrintingControllerImpl;

/**
 * A simple activity that allows Chrome to expose print as an option in the share menu.
 */
public class PrintShareActivity extends ShareActivity {
    @Override
    protected void handleShareAction(ChromeActivity triggeringActivity) {
        triggeringActivity.onMenuOrKeyboardAction(R.id.print_id, true);
    }

    public static boolean featureIsAvailable(Tab currentTab) {
        PrintingController printingController = PrintingControllerImpl.getInstance();
        return (printingController != null && !currentTab.isNativePage()
                && !currentTab.isShowingInterstitialPage() && !printingController.isBusy()
                && PrefServiceBridge.getInstance().getBoolean(Pref.PRINTING_ENABLED));
    }
}
