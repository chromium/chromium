// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.junit.Assert.assertTrue;

import android.content.Intent;
import android.os.Bundle;

import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.Robolectric;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowApplication;

import org.chromium.base.test.BaseRobolectricTestRunner;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.components.webapk.lib.client.WebApkValidator;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.WebApkConstants;
import org.chromium.webapk.test.WebApkTestHelper;

/** JUnit test for WebappLauncherActivity. */
@RunWith(BaseRobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@CommandLineFlags.Add(ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE)
public class WebappLauncherActivityTest {
    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String START_URL = "https://www.google.com/scope/a_is_for_apple";

    @Before
    public void setUp() {
        WebApkValidator.setDisableValidationForTesting(true);
    }

    /**
     * Test that WebappLauncherActivity modifies the passed-in intent so that
     * WebApkIntentDataProviderFactory#create() returns null if the intent does not refer to a valid
     * WebAPK.
     */
    @Test
    public void testTryCreateWebappInfoAltersIntentIfNotValidWebApk() {
        WebApkValidator.setDisableValidationForTesting(false);

        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);
        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);

        assertNotNull(WebApkIntentDataProviderFactory.create(intent));
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();
        assertNull(WebApkIntentDataProviderFactory.create(intent));
    }

    /** Test the launch intent created by {@link WebappLauncherActivity} for old-style WebAPKs. */
    @Test
    public void testOldStyleLaunchIntent() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertEquals(WebappActivity.class.getName(), launchIntent.getComponent().getClassName());
        // WebAPK package name should be part of the intent URI to enable launching multiple
        // WebAPKs.
        assertEquals("webapp://webapk-" + WEBAPK_PACKAGE_NAME, launchIntent.getDataString());
        assertTrue((launchIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK) != 0);
        assertNotNull(WebApkIntentDataProviderFactory.create(launchIntent));
    }

    /** Test the launch intent created by {@link WebappLauncherActivity} for new-style WebAPKs. */
    @Test
    public void testNewStyleLaunchIntent() {
        registerWebApk(WEBAPK_PACKAGE_NAME, START_URL);

        Intent intent = WebApkTestHelper.createMinimalWebApkIntent(WEBAPK_PACKAGE_NAME, START_URL);
        intent.putExtra(WebApkConstants.EXTRA_SPLASH_PROVIDED_BY_WEBAPK, true);
        Robolectric.buildActivity(WebappLauncherActivity.class, intent).create();

        Intent launchIntent = getNextStartedActivity();
        assertEquals(
                SameTaskWebApkActivity.class.getName(), launchIntent.getComponent().getClassName());
        assertEquals(launchIntent.getFlags() & Intent.FLAG_ACTIVITY_NEW_TASK, 0);
        assertNotNull(WebApkIntentDataProviderFactory.create(launchIntent));
    }

    private void registerWebApk(String webApkPackage, String startUrl) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.START_URL, START_URL);
        WebApkTestHelper.registerWebApkWithMetaData(
                webApkPackage, bundle, /* shareTargetMetaData= */ null);
        WebApkTestHelper.addIntentFilterForUrl(webApkPackage, startUrl);
    }

    private Intent getNextStartedActivity() {
        return ShadowApplication.getInstance().getNextStartedActivity();
    }
}
