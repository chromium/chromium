// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
package org.chromium.chrome.browser.webapps;

import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.pm.ResolveInfo;
import android.net.Uri;
import android.provider.Browser;

import androidx.browser.customtabs.CustomTabsIntent;

import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageManagerUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.chrome.browser.IntentHandler;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider;
import org.chromium.chrome.browser.customtabs.CustomTabIntentDataProvider.LaunchSourceType;
import org.chromium.chrome.browser.externalnav.ExternalNavigationDelegateImpl;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tab.TabIdManager;
import org.chromium.chrome.browser.tabmodel.AsyncTabParamsManager;
import org.chromium.chrome.browser.tabmodel.TabLaunchType;
import org.chromium.chrome.browser.tabmodel.document.AsyncTabCreationParams;
import org.chromium.chrome.browser.tabmodel.document.TabDelegate;

import java.net.URISyntaxException;
import java.util.List;

/**
 * Asynchronously creates Tabs for navigation originating from an installed PWA.
 *
 * This is the same as the parent class with exception of checking for a specialized native handlers
 * first, and if none are found opening a Custom Tab instead of creating a new tab in Chrome.
 */
public class WebappTabDelegate extends TabDelegate {
    private static final String TAG = "WebappTabDelegate";
    private String mApkPackageName;
    private @LaunchSourceType int mLaunchSourceType;

    public WebappTabDelegate(boolean incognito, WebappInfo webappInfo) {
        super(incognito);
        mApkPackageName = webappInfo.webApkPackageName();
        mLaunchSourceType =
                webappInfo.isForWebApk() ? LaunchSourceType.WEBAPK : LaunchSourceType.WEBAPP;
    }

    @Override
    public void createNewTab(
            AsyncTabCreationParams asyncParams, @TabLaunchType int type, int parentId) {
        String url = asyncParams.getLoadUrlParams().getUrl();
        if (maybeStartExternalActivity(url)) return;

        int assignedTabId = TabIdManager.getInstance().generateValidId(Tab.INVALID_TAB_ID);
        AsyncTabParamsManager.add(assignedTabId, asyncParams);

        Intent intent = new CustomTabsIntent.Builder().setShowTitle(true).build().intent;
        intent.setData(Uri.parse(url));
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_SEND_TO_EXTERNAL_DEFAULT_HANDLER, true);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_CHROME, true);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_IS_OPENED_BY_WEBAPK, true);
        intent.putExtra(CustomTabIntentDataProvider.EXTRA_BROWSER_LAUNCH_SOURCE, mLaunchSourceType);
        intent.putExtra(Browser.EXTRA_APPLICATION_ID, mApkPackageName);
        addAsyncTabExtras(asyncParams, parentId, false /* isChromeUI */, assignedTabId, intent);

        IntentHandler.startActivityForTrustedIntent(intent);
    }

    private boolean maybeStartExternalActivity(String url) {
        Intent intent;
        try {
            intent = Intent.parseUri(url, Intent.URI_INTENT_SCHEME);
        } catch (URISyntaxException ex) {
            Log.w(TAG, "Bad URI %s", url, ex);
            return false;
        }

        List<ResolveInfo> handlers = PackageManagerUtils.queryIntentActivities(
                intent, PackageManager.GET_RESOLVED_FILTER);

        boolean foundSpecializedHandler = false;

        for (String result :
                ExternalNavigationDelegateImpl.getSpecializedHandlersWithFilter(handlers, null)) {
            if (result.equals(mApkPackageName)) {
                // Current WebAPK matches and this is a HTTP(s) link. Don't intercept so that we
                // can launch a CCT. See http://crbug.com/831806 for more context.
                return false;
            } else {
                foundSpecializedHandler = true;
            }
        }

        // Launch a native app iff there is a specialized handler for a given URL.
        if (foundSpecializedHandler) {
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            // StartActivity may cause StrictMode violations on Android K.
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                ContextUtils.getApplicationContext().startActivity(intent);
            }
            return true;
        }

        return false;
    }
}
