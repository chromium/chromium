// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.net.Uri;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.ApplicationStatus;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Matchers;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Instrumentation tests for launching site settings for WebApks. Site settings are added as a
 * dynamic android shortcut. The shortcut launches a {@link ManageTrustedWebActivityDataActivity}
 * intent that validates the WebApk and launches the chromium SettingsActivity.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ManageTrustedWebActivityDataActivityTest {
    private static final String SETTINGS_ACTIVITY_NAME =
            "org.chromium.chrome.browser.settings.SettingsActivity";
    private static final String WEBAPK_TEST_URL = "https://pwa-directory.appspot.com/";
    private static final String TEST_PACKAGE_NAME = "org.chromium.webapk.test";

    @Test
    @MediumTest
    public void launchesWebApkSiteSettings() throws Exception {
        WebApkValidator.setDisableValidationForTesting(true);
        ManageTrustedWebActivityDataActivity.setCallingPackageForTesting(TEST_PACKAGE_NAME);
        TrustedWebActivityTestUtil.spoofVerification(TEST_PACKAGE_NAME, WEBAPK_TEST_URL);
        launchSettings(TEST_PACKAGE_NAME, Uri.parse(WEBAPK_TEST_URL));

        // Check settings activity is running.
        CriteriaHelper.pollUiThread(
                () -> {
                    try {
                        Criteria.checkThat(
                                "Site settings activity was not launched",
                                siteSettingsActivityRunning(),
                                Matchers.is(true));
                    } catch (PackageManager.NameNotFoundException e) {
                        e.printStackTrace();
                    }
                });
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

    private static void launchSettings(String packageName, Uri uri) {
        Intent intent = new Intent();
        intent.setAction(
                "android.support.customtabs.action.ACTION_MANAGE_TRUSTED_WEB_ACTIVITY_DATA");
        intent.setPackage(ApplicationProvider.getApplicationContext().getPackageName());
        intent.setData(uri);
        intent.putExtra(WebApkConstants.EXTRA_IS_WEBAPK, true);
        // The following flag is required because the test starts the intent outside of an activity.
        intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
        InstrumentationRegistry.getInstrumentation().startActivitySync(intent);
    }
}
