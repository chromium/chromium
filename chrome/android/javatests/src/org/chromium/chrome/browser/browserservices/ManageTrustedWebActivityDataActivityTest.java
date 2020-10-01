// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.createSession;
import static org.chromium.chrome.browser.browserservices.TrustedWebActivityTestUtil.spoofVerification;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.support.test.InstrumentationRegistry;

import androidx.browser.customtabs.CustomTabsIntent;
import androidx.browser.customtabs.CustomTabsSession;
import androidx.test.filters.MediumTest;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.webapk.lib.common.WebApkConstants;

import java.util.concurrent.TimeoutException;

/**
 * Instrumentation tests for launching site settings for WebApks.
 * Site settings are added as a dynamic android shortcut.
 * The shortcut launches a {@link ManageTrustedWebActivityDataActivity}
 * intent that validates the WebApk and launches the chromium SettingsActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ManageTrustedWebActivityDataActivityTest {
    private static final String SETTINGS_ACTIVITY_NAME =
            "org.chromium.chrome.browser.settings.SettingsActivity";
    private static final String WEBAPK_TEST_URL = "https://www.example.com";
    private static final String TEST_PACKAGE_NAME =
            InstrumentationRegistry.getTargetContext().getPackageName();

    @Test
    @MediumTest
    public void launchesWebApkSiteSettings() {
        Intent siteSettingsIntent =
                createWebApkSiteSettingsIntent(TEST_PACKAGE_NAME, Uri.parse(WEBAPK_TEST_URL));

        WebApkValidator.setDisableValidationForTesting(true);
        try {
            launchSiteSettingsIntent(siteSettingsIntent);

            // Check settings activity is running.
            CriteriaHelper.pollUiThread(() -> {
                try {
                    Criteria.checkThat("Site settings activity was not launched",
                            siteSettingsActivityRunning(), Matchers.is(true));
                } catch (PackageManager.NameNotFoundException e) {
                    e.printStackTrace();
                }
            });
        } catch (TimeoutException e) {
            e.printStackTrace();
        }
    }

    private boolean siteSettingsActivityRunning() throws PackageManager.NameNotFoundException {
        for (Activity a : ApplicationStatus.getRunningActivities()) {
            String activityName =
                    a.getPackageManager().getActivityInfo(a.getComponentName(), 0).name;
            if (activityName.equals(SETTINGS_ACTIVITY_NAME)) {
                return true;
            }
        }
        return false;
    }

    private static Intent createWebApkSiteSettingsIntent(String packageName, Uri uri) {
        // CustomTabsIntent builder is used just to put in the session extras.
        CustomTabsIntent.Builder builder =
                new CustomTabsIntent.Builder(CustomTabsSession.createMockSessionForTesting(
                        new ComponentName(InstrumentationRegistry.getTargetContext(),
                                ManageTrustedWebActivityDataActivity.class)));
        Intent intent = builder.build().intent;
        intent.setAction(
                "android.support.customtabs.action.ACTION_MANAGE_TRUSTED_WEB_ACTIVITY_DATA");
        intent.setPackage(packageName);
        intent.setData(uri);
        intent.putExtra(WebApkConstants.EXTRA_IS_WEBAPK, true);
        // The following flag is required because the test starts the intent outside of an activity.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        return intent;
    }

    public void launchSiteSettingsIntent(Intent intent) throws TimeoutException {
        String url = intent.getData().toString();
        spoofVerification(TEST_PACKAGE_NAME, url);
        createSession(intent, TEST_PACKAGE_NAME);

        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }
}
