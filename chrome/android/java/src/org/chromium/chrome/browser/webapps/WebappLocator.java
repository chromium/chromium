// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import android.app.Activity;
import android.text.TextUtils;

import org.chromium.base.ApplicationStatus;
import org.chromium.chrome.browser.browserservices.intents.WebappExtras;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.tab.Tab;

import java.lang.ref.WeakReference;

/**
 * Utility class for locating running {@link WebappActivity}.
 */
public class WebappLocator {
    /** Returns the running WebappActivity with the given tab id. Returns null if there is none. */
    public static WeakReference<BaseCustomTabActivity> findWebappActivityWithTabId(int tabId) {
        if (tabId == Tab.INVALID_TAB_ID) return null;

        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!(activity instanceof WebappActivity)) continue;

            BaseCustomTabActivity customTabActivity = (BaseCustomTabActivity) activity;
            Tab tab = customTabActivity.getActivityTab();
            if (tab != null && tab.getId() == tabId) {
                return new WeakReference<>(customTabActivity);
            }
        }
        return null;
    }

    /** Returns the WebappActivity with the given {@link webappId}. */
    public static WeakReference<BaseCustomTabActivity> findRunningWebappActivityWithId(
            String webappId) {
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (!(activity instanceof WebappActivity)) {
                continue;
            }
            BaseCustomTabActivity customTabActivity = (BaseCustomTabActivity) activity;
            WebappExtras webappExtras = customTabActivity.getIntentDataProvider().getWebappExtras();
            if (webappExtras != null && TextUtils.equals(webappId, webappExtras.id)) {
                return new WeakReference<>(customTabActivity);
            }
        }
        return null;
    }
}
