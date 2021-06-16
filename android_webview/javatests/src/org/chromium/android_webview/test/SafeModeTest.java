// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.services.ISafeModeService;
import org.chromium.android_webview.services.SafeModeService;
import org.chromium.android_webview.test.services.ServiceConnectionHelper;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Feature;

import java.util.Arrays;

/**
 * Test WebView SafeMode.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class SafeModeTest {
    // The package name of the test shell. This is acting both as the client app and the WebView
    // provider.
    public static final String TEST_WEBVIEW_PACKAGE_NAME = "org.chromium.android_webview.shell";

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @After
    public void tearDown() throws Throwable {
        // Reset component state back to the default.
        final Context context = ContextUtils.getApplicationContext();
        ComponentName safeModeComponent = new ComponentName(
                TEST_WEBVIEW_PACKAGE_NAME, SafeModeController.SAFE_MODE_STATE_COMPONENT);
        context.getPackageManager().setComponentEnabledSetting(safeModeComponent,
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT, PackageManager.DONT_KILL_APP);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_disabledByDefault() throws Throwable {
        Assert.assertFalse("SafeMode should be off by default",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_readComponentState() throws Throwable {
        // Enable the component directly.
        final Context context = ContextUtils.getApplicationContext();
        ComponentName safeModeComponent = new ComponentName(
                TEST_WEBVIEW_PACKAGE_NAME, SafeModeController.SAFE_MODE_STATE_COMPONENT);
        context.getPackageManager().setComponentEnabledSetting(safeModeComponent,
                PackageManager.COMPONENT_ENABLED_STATE_ENABLED, PackageManager.DONT_KILL_APP);

        Assert.assertTrue("SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_enableWithService() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(TEST_WEBVIEW_PACKAGE_NAME, Arrays.asList("some_action_name"));
        }

        Assert.assertTrue("SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_disableWithService() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(TEST_WEBVIEW_PACKAGE_NAME, Arrays.asList("some_action_name"));
        }

        Assert.assertTrue("SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));

        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(TEST_WEBVIEW_PACKAGE_NAME, Arrays.asList());
        }

        Assert.assertFalse("SafeMode should be re-disabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_mustBeTrustedApp() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            try {
                service.setSafeMode("fake.package.name", Arrays.asList("some_action_name"));
                Assert.fail(
                        "SafeModeService should have thrown an exception for wrong package name");
            } catch (SecurityException e) {
                // Expected
            }
        }

        Assert.assertFalse("SafeMode should stay disabled because package name is wrong",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }
}
