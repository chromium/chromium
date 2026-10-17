// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.browserservices;

import android.app.Activity;
import android.content.Intent;
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
import org.chromium.chrome.browser.settings.SettingsActivity;
import org.chromium.chrome.browser.settings.SettingsHostFragment;
import org.chromium.chrome.browser.settings.SettingsInTab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.components.browser_ui.site_settings.SingleWebsiteSettings;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.webapk.lib.common.WebApkConstants;

/**
 * Instrumentation tests for launching site settings for WebApks. Site settings are added as a
 * dynamic android shortcut. The shortcut launches a {@link ManageTrustedWebActivityDataActivity}
 * intent that validates the WebApk and launches site settings, either in a {@link SettingsActivity}
 * or in a settings tab.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@Batch(Batch.PER_CLASS)
public class ManageTrustedWebActivityDataActivityTest {
    private static final String WEBAPK_TEST_URL = "https://pwa-directory.appspot.com/";
    private static final String TEST_PACKAGE_NAME = "org.chromium.webapk.test";

    @Test
    @MediumTest
    public void launchesWebApkSiteSettings() throws Exception {
        WebApkValidator.setDisableValidationForTesting(true);
        ManageTrustedWebActivityDataActivity.setCallingPackageForTesting(TEST_PACKAGE_NAME);
        TrustedWebActivityTestUtil.spoofVerification(TEST_PACKAGE_NAME, WEBAPK_TEST_URL);
        launchSettings(TEST_PACKAGE_NAME, Uri.parse(WEBAPK_TEST_URL));

        // Check site settings are showing.
        CriteriaHelper.pollUiThread(
                () -> {
                    Criteria.checkThat(
                            "Site settings were not launched",
                            siteSettingsRunning(),
                            Matchers.is(true));
                });
    }

    private static boolean siteSettingsRunning() {
        for (Activity activity : ApplicationStatus.getRunningActivities()) {
            if (SettingsInTab.shouldOpenSettingsInTab()) {
                // Settings open in a browser tab, hosted by a SettingsHostFragment.
                SettingsHostFragment hostFragment = SettingsHostFragment.get(activity);
                if (hostFragment != null
                        && hostFragment.getMainFragment() instanceof SingleWebsiteSettings) {
                    return true;
                }
            } else if (activity instanceof SettingsActivity settingsActivity
                    && settingsActivity.getMainFragment() instanceof SingleWebsiteSettings) {
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
