// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.incognito;

import org.chromium.chrome.browser.tabmodel.IncognitoTabHostUtils;
import org.chromium.chrome.browser.tabmodel.IncognitoTabModelObserver;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.ui.permissions.ContextualNotificationPermissionRequester;

/**
 * Controls the presence incognito notification through {@link IncognitoNotificationManager}.
 *
 * Reacts to the presence or absence of incognito tabs.
 */
public class IncognitoNotificationPresenceController implements IncognitoTabModelObserver {
    /**
     * Creates an {@link IncognitoNotificationPresenceController} that reacts to incognito tabs in a
     * given |tabModelSelector|.
     * @param tabModelSelector The {@link TabModelSelector} to observe
     */
    public static void observeTabModelSelector(TabModelSelector tabModelSelector) {
        tabModelSelector.addIncognitoTabModelObserver(
                new IncognitoNotificationPresenceController());
    }

    IncognitoNotificationPresenceController() {}

    @Override
    public void wasFirstTabCreated() {
        IncognitoNotificationManager.showIncognitoNotification();
        ContextualNotificationPermissionRequester.getInstance().requestPermissionIfNeeded();
    }

    @Override
    public void didBecomeEmpty() {
        if (!IncognitoTabHostUtils.doIncognitoTabsExist()) {
            IncognitoNotificationManager.dismissIncognitoNotification();
        }
    }
}
