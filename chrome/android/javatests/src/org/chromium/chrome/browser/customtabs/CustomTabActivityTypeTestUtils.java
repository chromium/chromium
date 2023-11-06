// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.customtabs;

import android.content.Intent;

import androidx.test.core.app.ApplicationProvider;

import org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil;
import org.chromium.chrome.browser.browserservices.intents.BrowserServicesIntentDataProvider;
import org.chromium.chrome.browser.browserservices.intents.WebappConstants;
import org.chromium.chrome.browser.flags.ActivityType;
import org.chromium.chrome.browser.webapps.WebApkActivityTestRule;
import org.chromium.chrome.browser.webapps.WebappActivityTestRule;
import org.chromium.chrome.test.ChromeActivityTestRule;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;

import java.util.concurrent.TimeoutException;

/**
 * Helper methods for running a test against multiple activity types. Supported activity types:
 * webapp, WebAPK, CCT, and TWA.
 */
public class CustomTabActivityTypeTestUtils {
    public static ChromeActivityTestRule<? extends BaseCustomTabActivity> createActivityTestRule(
            @ActivityType int activityType) {
        return switch (activityType) {
            case ActivityType.WEBAPP -> new WebappActivityTestRule();
            case ActivityType.WEB_APK -> new WebApkActivityTestRule();
            default -> new CustomTabActivityTestRule();
        };
    }

    public static void launchActivity(
            @ActivityType int activityType, ChromeActivityTestRule<?> activityTestRule, String url)
            throws TimeoutException, UnsupportedOperationException {
        switch (activityType) {
            case ActivityType.WEBAPP:
                launchWebapp((WebappActivityTestRule) activityTestRule, url);
                return;
            case ActivityType.WEB_APK:
                launchWebApk((WebApkActivityTestRule) activityTestRule, url);
                return;
            case ActivityType.CUSTOM_TAB:
                launchCct((CustomTabActivityTestRule) activityTestRule, url);
                return;
            case ActivityType.TRUSTED_WEB_ACTIVITY:
                launchTwa((CustomTabActivityTestRule) activityTestRule, url);
                return;
            default:
                throw new UnsupportedOperationException();
        }
    }

    private static void launchWebapp(WebappActivityTestRule activityTestRule, String url) {
        Intent launchIntent = activityTestRule.createIntent();
        launchIntent.putExtra(WebappConstants.EXTRA_URL, url);
        activityTestRule.startWebappActivity(launchIntent);
    }

    private static void launchWebApk(WebApkActivityTestRule activityTestRule, String url) {
        BrowserServicesIntentDataProvider intentDataProvider =
                new WebApkIntentDataProviderBuilder("org.chromium.webapk.random", url).build();
        activityTestRule.startWebApkActivity(intentDataProvider);
    }

    private static void launchCct(CustomTabActivityTestRule activityTestRule, String url) {
        activityTestRule.startCustomTabActivityWithIntent(
                CustomTabsIntentTestUtils.createMinimalCustomTabIntent(
                        ApplicationProvider.getApplicationContext(), url));
    }

    private static void launchTwa(CustomTabActivityTestRule activityTestRule, String url)
            throws TimeoutException {
        String packageName = ApplicationProvider.getApplicationContext().getPackageName();
        Intent intent = TrustedWebActivityTestUtil.createTrustedWebActivityIntent(url);
        TrustedWebActivityTestUtil.spoofVerification(packageName, url);
        TrustedWebActivityTestUtil.createSession(intent, packageName);
        activityTestRule.startCustomTabActivityWithIntent(intent);
    }
}
