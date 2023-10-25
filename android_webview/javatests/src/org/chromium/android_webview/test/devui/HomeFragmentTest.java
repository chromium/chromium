// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.action.ViewActions.longClick;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.assertNoUnverifiedIntents;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.equalTo;
import static org.hamcrest.Matchers.is;
import static org.hamcrest.Matchers.not;

import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.getClipBoardTextOnUiThread;
import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.withCount;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.os.Build;
import android.provider.Settings;

import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.HomeFragment;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.devui.WebViewPackageError;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.DisableIf;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.test.util.ViewUtils;

import java.util.Locale;

/** UI tests for the developer UI's HomeFragment. */
@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Batching causes test failures")
public class HomeFragmentTest {
    public static final PackageInfo FAKE_WEBVIEW_PACKAGE = new PackageInfo();

    static {
        FAKE_WEBVIEW_PACKAGE.packageName = "org.chromium.fake_webview";
        FAKE_WEBVIEW_PACKAGE.versionCode = 123456789;
        FAKE_WEBVIEW_PACKAGE.versionName = "999.888.777.666";
    }

    @Rule
    public BaseActivityTestRule<MainActivity> mRule =
            new BaseActivityTestRule<>(MainActivity.class);

    @Before
    public void setUp() {
        // Mark popup permission as already requested to suppress the popup
        MainActivity.markPopupPermissionRequestedInPrefsForTesting();
    }

    @After
    public void tearDown() {
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(null);
        // Activity is launched, i.e the test is not skipped.
        if (mRule.getActivity() != null) {
            // Tests are responsible for verifying every Intent they trigger.
            assertNoUnverifiedIntents();
            Intents.release();
        }
    }

    private void launchHomeFragment() {
        mRule.launchActivity(null);
        ViewUtils.waitForVisibleView(withId(R.id.fragment_home));
        // Only start recording intents after launching the MainActivity.
        Intents.init();

        // Stub all external intents, to avoid launching other apps (ex. system browser), has to be
        // done after launching the activity.
        intending(not(IntentMatchers.isInternal()))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHasPublicNoArgsConstructor() throws Throwable {
        HomeFragment fragment = new HomeFragment();
        Assert.assertNotNull(fragment);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    // Test when the system WebView provider is the same package from which the developer UI is
    // launched.
    public void testSameWebViewPackage() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        // Inject test app package as the current WebView package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
        launchHomeFragment();

        // No error messages is displayed.
        onView(withId(R.id.main_error_view)).check(matches(not(isDisplayed())));

        onView(withId(R.id.main_info_list)).check(matches(withCount(2)));

        PackageInfo currentWebViewPackage = WebViewPackageHelper.getCurrentWebViewPackage(context);
        String expectedWebViewPackageInfo =
                String.format(
                        Locale.US,
                        "%s (%s/%s)",
                        currentWebViewPackage.packageName,
                        currentWebViewPackage.versionName,
                        currentWebViewPackage.versionCode);
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
        Context context = ContextUtils.getApplicationContext();
        // Inject a fake PackageInfo as the current WebView package to make sure it will always be
        // different from the test's app package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(FAKE_WEBVIEW_PACKAGE);
        launchHomeFragment();

        onView(withId(R.id.main_info_list)).check(matches(withCount(3)));

        String expectedWebViewPackageInfo =
                String.format(
                        Locale.US,
                        "%s (%s/%s)",
                        FAKE_WEBVIEW_PACKAGE.packageName,
                        FAKE_WEBVIEW_PACKAGE.versionName,
                        FAKE_WEBVIEW_PACKAGE.versionCode);
        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText("WebView package")));
        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText(expectedWebViewPackageInfo)));

        PackageInfo devUiPackage = WebViewPackageHelper.getContextPackageInfo(context);
        String expectedDevUiInfo =
                String.format(
                        Locale.US,
                        "%s (%s/%s)",
                        devUiPackage.packageName,
                        devUiPackage.versionName,
                        devUiPackage.versionCode);
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
    @DisableIf.Build(
            sdk_is_greater_than = Build.VERSION_CODES.R,
            message = "https://crbug.com/1292197")
    public void testLongPressCopy() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        // Inject a fake PackageInfo as the current WebView package to make sure it will always be
        // different from the test's app package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(FAKE_WEBVIEW_PACKAGE);
        launchHomeFragment();

        onView(withText("WebView package")).perform(longClick());
        String expectedWebViewInfo =
                String.format(
                        Locale.US,
                        "%s (%s/%s)",
                        FAKE_WEBVIEW_PACKAGE.packageName,
                        FAKE_WEBVIEW_PACKAGE.versionName,
                        FAKE_WEBVIEW_PACKAGE.versionCode);
        assertThat(getClipBoardTextOnUiThread(context), is(equalTo(expectedWebViewInfo)));

        onView(withText("DevTools package")).perform(longClick());
        PackageInfo devUiPackage = WebViewPackageHelper.getContextPackageInfo(context);
        String expectedDevUiInfo =
                String.format(
                        Locale.US,
                        "%s (%s/%s)",
                        devUiPackage.packageName,
                        devUiPackage.versionName,
                        devUiPackage.versionCode);
        assertThat(getClipBoardTextOnUiThread(context), is(equalTo(expectedDevUiInfo)));

        onView(withText("Device info")).perform(longClick());
        String expectedDeviceInfo =
                String.format(Locale.US, "%s - %s", Build.MODEL, Build.FINGERPRINT);
        assertThat(getClipBoardTextOnUiThread(context), is(equalTo(expectedDeviceInfo)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testDifferentWebViewPackageError_bannerMessage_postNougat() throws Throwable {
        // Inject a fake PackageInfo as the current WebView package to make sure it will always be
        // different from the test's app package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(FAKE_WEBVIEW_PACKAGE);
        launchHomeFragment();

        Context context = ContextUtils.getApplicationContext();
        String expectedErrorMessage =
                String.format(
                        Locale.US,
                        WebViewPackageError.DIFFERENT_WEBVIEW_PROVIDER_ERROR_MESSAGE,
                        WebViewPackageHelper.loadLabel(context));
        ViewUtils.waitForVisibleView(withId(R.id.main_error_view));
        onView(withId(R.id.main_error_view)).check(matches(isDisplayed()));
        onView(withId(R.id.error_text)).check(matches(withText(expectedErrorMessage)));
        // Since the current provider is set to a fake package not an actual installed WebView
        // provider, the UI should only offer to change the system WebView provider and should not
        // offer to open the current WebView provider dev UI.

        onView(withId(R.id.action_button))
                .check(
                        matches(
                                allOf(
                                        isDisplayed(),
                                        withText(
                                                WebViewPackageError
                                                        .CHANGE_WEBVIEW_PROVIDER_BUTTON_TEXT))))
                .perform(click());
        intended(IntentMatchers.hasAction(Settings.ACTION_WEBVIEW_SETTINGS));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    // Test the dialog shown when the WebView package error message is clicked.
    public void testDifferentWebViewPackageError_dialog_postNougat() throws Throwable {
        Context context = ContextUtils.getApplicationContext();
        // Inject a fake PackageInfo as the current WebView package to make sure it will always be
        // different from the test's app package.
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(FAKE_WEBVIEW_PACKAGE);
        launchHomeFragment();

        String dialogExpectedMessage =
                String.format(
                        Locale.US,
                        WebViewPackageError.DIFFERENT_WEBVIEW_PROVIDER_DIALOG_MESSAGE,
                        WebViewPackageHelper.loadLabel(context));
        onView(withId(R.id.main_error_view)).check(matches(isDisplayed())).perform(click());
        onView(withText(dialogExpectedMessage)).check(matches(isDisplayed()));
        // Since the current provider is set to a fake package not an actual installed WebView
        // provider, the UI should only offer to change the system WebView provider and should not
        // offer to open the current WebView provider dev UI.
        onView(withId(android.R.id.button1)).check(matches(not(isDisplayed()))); // positive button
        onView(withId(android.R.id.button2)).check(matches(not(isDisplayed()))); // negative button
        // botton3 is dialog neutral button
        onView(withId(android.R.id.button3))
                .check(matches(withText(WebViewPackageError.CHANGE_WEBVIEW_PROVIDER_BUTTON_TEXT)))
                .perform(click());
        intended(IntentMatchers.hasAction(Settings.ACTION_WEBVIEW_SETTINGS));
    }
}
