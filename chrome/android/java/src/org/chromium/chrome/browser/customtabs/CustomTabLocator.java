// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.app.Activity;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.webapps.WebappActivity;

import java.lang.ref.WeakReference;

/**
 * Utility class for locating running {@link BaseCustomTabActivity} and {@link WebappActivity} as
 * well.
 */
public class CustomTabLocator {
    /**
     * Returns the running BaseCustomTabActivity with the given tab id. Returns null if there is
     * none.
     *
     * @param tabId the tabId to search with.
     * @return The matched BaseCustomTabActivity if there's any, otherwise returns null.
     */
    public static WeakReference<BaseCustomTabActivity> findCustomTabActivityWithTabId(int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return null;

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!(activity instanceof BaseCustomTabActivity customTabActivity)) continue;

            Tab tab = customTabActivity.getActivityTab();
            if (tab != null && tab.getId() == tabId) {
                return new WeakReference<>(customTabActivity);
            }
        }
        return null;
    }

    /**
     * Returns the BaseCustomTabActivity represents the running WebAppActivity with the given id.
     *
     * @param webappId The webapp id to search with.
     * @return The matched WepappActivity if there's any, otherwise returns null.
     */
    public static WeakReference<BaseCustomTabActivity> findRunningWebappActivityWithId(
            String webappId) {
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!(activity instanceof WebappActivity customTabActivity)) {
                continue;
            }
            WebappExtras webappExtras = customTabActivity.getIntentDataProvider().getWebappExtras();
            if (webappExtras != null && TextUtils.equals(webappId, webappExtras.id)) {
                return new WeakReference<>(customTabActivity);
            }
        }
        return null;
    }
}
