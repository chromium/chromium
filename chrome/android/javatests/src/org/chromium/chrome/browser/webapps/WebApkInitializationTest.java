// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.webapps;

import static org.mockito.ArgumentMatchers.isA;
import static org.mockito.Mockito.verify;

import android.app.Activity;

import androidx.test.filters.LargeTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.mockito.Mockito;

import org.chromium.base.ActivityState;
import org.chromium.base.ApplicationStatus;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.browserservices.ui.SharedActivityCoordinator;
import org.chromium.chrome.browser.browserservices.ui.controller.webapps.WebappDisclosureController;
import org.chromium.chrome.browser.customtabs.BaseCustomTabActivity;
import org.chromium.chrome.browser.customtabs.CustomTabOrientationController;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.lifecycle.ActivityLifecycleDispatcher;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.util.browser.webapps.WebApkIntentDataProviderBuilder;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeoutException;

/** Tests that the expected classes are constructed when a WebAPK Activity is launched. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
@DoNotBatch(reason = "Activity initialzation test")
public class WebApkInitializationTest {
    @Rule public final WebApkActivityTestRule mActivityRule = new WebApkActivityTestRule();

    public ActivityLifecycleDispatcher mActivityLifecycleDispatcher;

    class CreationObserver implements ApplicationStatus.ActivityStateListener {
        public CreationObserver() {}

        @Override
        public void onActivityStateChange(Activity activity, @ActivityState int newState) {
            if (newState == ActivityState.CREATED && activity instanceof BaseCustomTabActivity) {
                mActivityLifecycleDispatcher =
                        Mockito.spy(((BaseCustomTabActivity) activity).getLifecycleDispatcher());
                ((BaseCustomTabActivity) activity)
                        .setLifecycleDispatcherForTesting(mActivityLifecycleDispatcher);
            }
        }
    }

    /**
     * Test that {@link WebappActionsNotificationManager}, {@link
     * WebappDisclosureSnackbarController}, {@link WebApkActivityLifecycleUmaTracker} and {@link
     * CustomTabOrientationController} are constructed when a {@link WebApkActivity} is launched.
     */
    @Test
    @LargeTest
    @Feature({"WebApk"})
    public void testInitialization() throws TimeoutException {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ApplicationStatus.registerStateListenerForAllActivities(new CreationObserver());
                });
        EmbeddedTestServer embeddedTestServer =
                mActivityRule.getEmbeddedTestServerRule().getServer();
        WebApkIntentDataProviderBuilder intentDataProviderBuilder =
                new WebApkIntentDataProviderBuilder(
                        "org.chromium.webapk.for.testing",
                        embeddedTestServer.getURL(
                                "/chrome/test/data/banners/manifest_test_page.html"));
        mActivityRule.startWebApkActivity(intentDataProviderBuilder.build());

        verify(mActivityLifecycleDispatcher).register(isA(WebappActionsNotificationManager.class));
        verify(mActivityLifecycleDispatcher).register(isA(WebappDisclosureController.class));
        verify(mActivityLifecycleDispatcher).register(isA(WebApkActivityLifecycleUmaTracker.class));
        verify(mActivityLifecycleDispatcher).register(isA(SharedActivityCoordinator.class));
    }
}
