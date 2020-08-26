// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;

import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.getClipBoardTextOnUiThread;
import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.withCount;

import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.support.test.InstrumentationRegistry;

import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.MediumTest;

import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.devui.util.WebViewPackageHelper;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.test.util.Feature;

import java.util.Locale;

/**
 * UI tests for the developer UI's HomeFragment.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class HomeFragmentTest {
    @Rule
    public IntentsTestRule mRule =
            new IntentsTestRule<MainActivity>(MainActivity.class, false, false);

    private void launchHomeFragment() {
        Intent intent = new Intent();
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_HOME);
        mRule.launchActivity(intent);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    // Test when the system WebView provider is the same package from which the developer UI is
    // launched.
    public void testSameWebViewPackage() throws Throwable {
        Context context = InstrumentationRegistry.getTargetContext();
        // Inject test app package as the current WebView package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
        launchHomeFragment();

        onView(withId(R.id.main_info_list)).check(matches(withCount(2)));

        PackageInfo currentWebViewPackage = WebViewPackageHelper.getCurrentWebViewPackage(context);
        String expectedWebViewPackageInfo =
                String.format(Locale.US, "%s (%s/%s)", currentWebViewPackage.packageName,
                        currentWebViewPackage.versionName, currentWebViewPackage.versionCode);
        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText("WebView package")));
        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText(expectedWebViewPackageInfo)));

        String expectedDeviceInfo =
                String.format(Locale.US, "%s - %s", Build.MODEL, Build.FINGERPRINT);
        onData(anything())
                .atPosition(1)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText("Device info")));
        onData(anything())
                .atPosition(1)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText(expectedDeviceInfo)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    // Test when the system WebView provider is different from the package from which the developer
    // UI is launched.
    public void testDifferentWebViewPackage() throws Throwable {
        Context context = InstrumentationRegistry.getTargetContext();
        // Inject a dummy PackageInfo as the current WebView package to make sure it will always be
        // different from the test's app package.
        PackageInfo dummyTestPackage = new PackageInfo();
        dummyTestPackage.packageName = "org.chromium.dummy_webview";
        dummyTestPackage.versionCode = 123456789;
        dummyTestPackage.versionName = "999.888.777.666";
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(dummyTestPackage);
        launchHomeFragment();

        onView(withId(R.id.main_info_list)).check(matches(withCount(3)));

        String expectedWebViewPackageInfo =
                String.format(Locale.US, "%s (%s/%s)", dummyTestPackage.packageName,
                        dummyTestPackage.versionName, dummyTestPackage.versionCode);
        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText("WebView package")));
        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText(expectedWebViewPackageInfo)));

        PackageInfo devUiPackage = WebViewPackageHelper.getContextPackageInfo(context);
        String expectedDevUiInfo = String.format(Locale.US, "%s (%s/%s)", devUiPackage.packageName,
                devUiPackage.versionName, devUiPackage.versionCode);
        onData(anything())
                .atPosition(1)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText("DevTools package")));
        onData(anything())
                .atPosition(1)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText(expectedDevUiInfo)));

        String expectedDeviceInfo =
                String.format(Locale.US, "%s - %s", Build.MODEL, Build.FINGERPRINT);
        onData(anything())
                .atPosition(2)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText("Device info")));
        onData(anything())
                .atPosition(2)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText(expectedDeviceInfo)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testLongPressCopy() throws Throwable {
        Context context = InstrumentationRegistry.getTargetContext();
        // Inject a dummy PackageInfo as the current WebView package to make sure it will always be
        // different from the test's app package.
        PackageInfo dummyTestPackage = new PackageInfo();
        dummyTestPackage.packageName = "org.chromium.dummy_webview";
        dummyTestPackage.versionCode = 123456789;
        dummyTestPackage.versionName = "999.888.777.666";
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(dummyTestPackage);
        launchHomeFragment();

        onView(withText("WebView package")).perform(longClick());
        String expectedWebViewInfo =
                String.format(Locale.US, "%s (%s/%s)", dummyTestPackage.packageName,
                        dummyTestPackage.versionName, dummyTestPackage.versionCode);
        assertThat(getClipBoardTextOnUiThread(context), is(equalTo(expectedWebViewInfo)));

        onView(withText("DevTools package")).perform(longClick());
        PackageInfo devUiPackage = WebViewPackageHelper.getContextPackageInfo(context);
        String expectedDevUiInfo = String.format(Locale.US, "%s (%s/%s)", devUiPackage.packageName,
                devUiPackage.versionName, devUiPackage.versionCode);
        assertThat(getClipBoardTextOnUiThread(context), is(equalTo(expectedDevUiInfo)));

        onView(withText("Device info")).perform(longClick());
        String expectedDeviceInfo =
                String.format(Locale.US, "%s - %s", Build.MODEL, Build.FINGERPRINT);
        assertThat(getClipBoardTextOnUiThread(context), is(equalTo(expectedDeviceInfo)));
    }
}
