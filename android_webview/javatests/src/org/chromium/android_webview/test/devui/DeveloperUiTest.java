// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.Espresso.openActionBarOverflowOrOptionsMenu;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.doesNotExist;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.intent.Intents.assertNoUnverifiedIntents;
import static androidx.test.espresso.intent.Intents.intended;
import static androidx.test.espresso.intent.Intents.intending;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasHost;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasParamWithValue;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasPath;
import static androidx.test.espresso.intent.matcher.UriMatchers.hasScheme;
import static androidx.test.espresso.matcher.ViewMatchers.hasTextColor;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.not;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Intent;
import android.os.Build;
import android.provider.Settings;
import android.support.test.InstrumentationRegistry;

import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.espresso.intent.rule.IntentsTestRule;
import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;

/**
 * UI tests for general developer UI functionality. Significant subcomponents (ex. Fragments) may
 * have their own test class.
 */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class DeveloperUiTest {
    // The package name of the test shell. This is acting both as the client app and the WebView
    // provider.
    public static final String TEST_WEBVIEW_PACKAGE_NAME = "org.chromium.android_webview.shell";
    public static final String TEST_WEBVIEW_APPLICATION_LABEL = "AwShellApplication";

    @Rule
    public IntentsTestRule mRule = new IntentsTestRule<MainActivity>(MainActivity.class);

    @Before
    public void setUp() throws Exception {
        // Stub all external intents, to avoid launching other apps (ex. system browser).
        intending(not(IntentMatchers.isInternal()))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));
    }

    @After
    public void tearDown() throws Exception {
        // Tests are responsible for verifyhing every Intent they trigger.
        assertNoUnverifiedIntents();
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOpensHomeFragmentByDefault() throws Throwable {
        onView(withId(R.id.fragment_home)).check(matches(isDisplayed()));
        onView(withId(R.id.navigation_home))
                .check(matches(hasTextColor(R.color.navigation_selected)));
        onView(withId(R.id.navigation_crash_ui))
                .check(matches(hasTextColor(R.color.navigation_unselected)));
        onView(withId(R.id.navigation_flags_ui))
                .check(matches(hasTextColor(R.color.navigation_unselected)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testNavigateBetweenFragments() throws Throwable {
        // HomeFragment -> CrashesListFragment
        onView(withId(R.id.navigation_crash_ui)).perform(click());
        onView(withId(R.id.fragment_home)).check(doesNotExist());
        onView(withId(R.id.fragment_crashes_list)).check(matches(isDisplayed()));
        onView(withId(R.id.navigation_home))
                .check(matches(hasTextColor(R.color.navigation_unselected)));
        onView(withId(R.id.navigation_crash_ui))
                .check(matches(hasTextColor(R.color.navigation_selected)));
        onView(withId(R.id.navigation_flags_ui))
                .check(matches(hasTextColor(R.color.navigation_unselected)));

        // CrashesListFragment -> FlagsFragment
        onView(withId(R.id.navigation_flags_ui)).perform(click());
        onView(withId(R.id.fragment_crashes_list)).check(doesNotExist());
        onView(withId(R.id.fragment_flags)).check(matches(isDisplayed()));
        onView(withId(R.id.navigation_home))
                .check(matches(hasTextColor(R.color.navigation_unselected)));
        onView(withId(R.id.navigation_crash_ui))
                .check(matches(hasTextColor(R.color.navigation_unselected)));
        onView(withId(R.id.navigation_flags_ui))
                .check(matches(hasTextColor(R.color.navigation_selected)));

        // FlagsFragment -> HomeFragment
        onView(withId(R.id.navigation_home)).perform(click());
        onView(withId(R.id.fragment_flags)).check(doesNotExist());
        onView(withId(R.id.fragment_home)).check(matches(isDisplayed()));
        onView(withId(R.id.navigation_home))
                .check(matches(hasTextColor(R.color.navigation_selected)));
        onView(withId(R.id.navigation_crash_ui))
                .check(matches(hasTextColor(R.color.navigation_unselected)));
        onView(withId(R.id.navigation_flags_ui))
                .check(matches(hasTextColor(R.color.navigation_unselected)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuOptions_switchProvider_shownOnNougat() throws Throwable {
        Assume.assumeTrue("This test verifies behavior introduced in Nougat and above",
                Build.VERSION.SDK_INT >= Build.VERSION_CODES.N);

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        onView(withText("Change WebView Provider")).check(matches(isDisplayed()));
        onView(withText("Change WebView Provider")).perform(click());
        intended(IntentMatchers.hasAction(Settings.ACTION_WEBVIEW_SETTINGS));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuOptions_switchProvider_notShown() throws Throwable {
        Assume.assumeTrue("This test verifies pre-Nougat behavior",
                Build.VERSION.SDK_INT < Build.VERSION_CODES.N);
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        onView(withId(R.id.options_menu_switch_provider)).check(doesNotExist());
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuOptions_reportBug() throws Throwable {
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        onView(withText("Report WebView Bug")).check(matches(isDisplayed()));
        onView(withText("Report WebView Bug")).perform(click());
        intended(allOf(IntentMatchers.hasAction(Intent.ACTION_VIEW),
                IntentMatchers.hasData(hasScheme("https")),
                IntentMatchers.hasData(hasHost("bugs.chromium.org")),
                IntentMatchers.hasData(hasPath("/p/chromium/issues/entry")),
                IntentMatchers.hasData(hasParamWithValue("template", "Webview+Bugs")),
                IntentMatchers.hasData(hasParamWithValue(
                        "labels", "Via-WebView-DevTools,Pri-3,Type-Bug,OS-Android"))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuOptions_checkUpdates_withPlayStore() throws Throwable {
        // Stub out the Intent to the Play Store, to verify the case where the Play Store Intent
        // resolves.
        // TODO(ntfschr): figure out how to stub startActivity to throw an exception, to verify the
        // case when Play is not installed.
        intending(
                allOf(IntentMatchers.hasAction(Intent.ACTION_VIEW),
                        IntentMatchers.hasData(hasScheme("market")),
                        IntentMatchers.hasData(hasHost("details")),
                        IntentMatchers.hasData(hasParamWithValue("id", TEST_WEBVIEW_PACKAGE_NAME))))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));

        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        onView(withText("Check for WebView updates")).check(matches(isDisplayed()));
        onView(withText("Check for WebView updates")).perform(click());

        intended(allOf(IntentMatchers.hasAction(Intent.ACTION_VIEW),
                IntentMatchers.hasData(hasScheme("market")),
                IntentMatchers.hasData(hasHost("details")),
                IntentMatchers.hasData(hasParamWithValue("id", TEST_WEBVIEW_PACKAGE_NAME))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuOptions_aboutDevTools() throws Throwable {
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());

        onView(withText("About WebView DevTools")).check(matches(isDisplayed()));
        onView(withText("About WebView DevTools")).perform(click());
        intended(allOf(IntentMatchers.hasAction(Intent.ACTION_VIEW),
                IntentMatchers.hasData(hasScheme("https")),
                IntentMatchers.hasData(hasHost("chromium.googlesource.com")),
                IntentMatchers.hasData(
                        hasPath("/chromium/src/+/HEAD/android_webview/docs/developer-ui.md"))));
    }
}
