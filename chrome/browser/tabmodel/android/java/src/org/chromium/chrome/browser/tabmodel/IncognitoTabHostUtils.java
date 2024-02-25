// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tabmodel;

/** Utilities concerning all incognito tabs in all {@link IncognitoTabHost}s. */
public class IncognitoTabHostUtils {
    /** Determine whether there are any incognito tabs. */
    public static boolean doIncognitoTabsExist() {
        for (IncognitoTabHost host : IncognitoTabHostRegistry.getInstance().getHosts()) {
            if (host.hasIncognitoTabs()) {
                return true;
            }
        }
        return false;
    }

    /** Determine whether the incognito tab model is active. */
    public static boolean isIncognitoTabModelActive() {
        for (IncognitoTabHost host : IncognitoTabHostRegistry.getInstance().getHosts()) {
            if (host.isActiveModel()) {
                return true;
            }
        }
        return false;
    }

    /** Closes all incognito tabs. */
    public static void closeAllIncognitoTabs() {
        for (IncognitoTabHost host : IncognitoTabHostRegistry.getInstance().getHosts()) {
            host.closeAllIncognitoTabs();
        }
    }
}
