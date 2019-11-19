// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.send_tab_to_self;

import org.chromium.chrome.browser.ChromeActivity;
import org.chromium.chrome.browser.lifecycle.Destroyable;
import org.chromium.chrome.browser.tab.Tab;

/**
 * This class is responsible for listening for new SendTabToSelfEntries and showing an infobar
 * to the user if the user does not have notifications enabled.
 */
public class SendTabToSelfInfoBarController implements Destroyable {
    private ChromeActivity mActivity;

    /**
     * Creates a SendTabToSelfInfoBarController for the activity passed in.
     *
     * TODO(crbug.com/949233):
     *     - Add observers to listen for SendTabToSelfModel changes. When a new entry is observed,
     *       check to see if notifications are enabled. If not, display an infobar.
     *     - If a new entry comes in and the user is not on a tab, listen for a new tab and show
     *       an infobar then.
     *     - If an infobar is already being displayed and the user pushes another tab, replace
     *       the existing infobar with a new one.
     *
     * @param activity A {@link ChromeActivity} instance the infobars will be
     *            shown in.
     * @return A new instance of {@link SendTabToSelfInfoBarController}.
     */
    public SendTabToSelfInfoBarController(ChromeActivity activity) {
        mActivity = activity;
        mActivity.getLifecycleDispatcher().register(this);
    }

    // Destroyable implementation.
    @Override
    public void destroy() {
        mActivity.getLifecycleDispatcher().unregister(this);
        mActivity = null;
    }

    /**
     * Shows an infobar corresponding to the entry passed in if the user is on a tab.
     * @param entry The entry to display the infobar for.
     */
    public void showInfobarForEntry(SendTabToSelfEntry entry) {
        if (mActivity == null) {
            return;
        }
        Tab tab = mActivity.getActivityTabProvider().get();

        // TODO(crbug.com/949233): Listen for when the user opens a tab next and show
        // an infobar then.
        if (tab == null) {
            return;
        }
        SendTabToSelfAndroidBridge.showInfoBar(entry, tab.getWebContents());
    }
}
