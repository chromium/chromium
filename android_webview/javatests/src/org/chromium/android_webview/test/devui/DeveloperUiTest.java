// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
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
import static androidx.test.espresso.matcher.ViewMatchers.isEnabled;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.not;
import static org.junit.Assert.assertFalse;
import static org.junit.Assert.assertTrue;

import android.app.Activity;
import android.app.Instrumentation.ActivityResult;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.provider.Settings;
import android.view.View;

import androidx.test.InstrumentationRegistry;
import androidx.test.espresso.DataInteraction;
import androidx.test.espresso.intent.Intents;
import androidx.test.espresso.intent.matcher.IntentMatchers;
import androidx.test.filters.MediumTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.test.util.ViewUtils;

/**
 * UI tests for general developer UI functionality. Significant subcomponents (ex. Fragments) may
 * have their own test class.
 */
@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Batching causes flakes.")
public class DeveloperUiTest {
    // The package name of the test shell. This is acting both as the client app and the WebView
    // provider.
    public static final String TEST_WEBVIEW_PACKAGE_NAME = "org.chromium.android_webview.shell";
    public static final String TEST_WEBVIEW_APPLICATION_LABEL = "AwShellApplication";

    @Rule
    public BaseActivityTestRule<MainActivity> mRule =
            new BaseActivityTestRule<>(MainActivity.class);

    private void launchHomeFragment() {
        mRule.launchActivity(null);
        ViewUtils.waitForView(withId(R.id.fragment_home));

        // Only start recording intents after launching the MainActivity.
        Intents.init();

        // Stub all external intents, to avoid launching other apps (ex. system browser), has to be
        // done after launching the activity.
        intending(not(IntentMatchers.isInternal()))
                .respondWith(new ActivityResult(Activity.RESULT_OK, null));
    }

    private void openOptionsMenu() {
        openActionBarOverflowOrOptionsMenu(
                InstrumentationRegistry.getInstrumentation().getTargetContext());
        ViewUtils.waitForView(withId(R.id.nav_menu_group));
    }

    @Before
    public void setUp() {
        Context context = InstrumentationRegistry.getTargetContext();
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
    }

    @After
    public void tearDown() throws Exception {
        // Activity is launched, i.e the test is not skipped.
        if (mRule.getActivity() != null) {
            // Clear the stored preferences
            mRule.getActivity().getPreferences(Context.MODE_PRIVATE).edit().clear().apply();

            // Tests are responsible for verifying every Intent they trigger.
            assertNoUnverifiedIntents();
            Intents.release();
        }
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testOpensHomeFragmentByDefault() throws Throwable {
        launchHomeFragment();

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
        launchHomeFragment();

        // HomeFragment -> CrashesListFragment
        onView(withId(R.id.navigation_crash_ui)).perform(click());
        ViewUtils.waitForView(withId(R.id.fragment_crashes_list));
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
        ViewUtils.waitForView(withId(R.id.fragment_flags));
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
        ViewUtils.waitForView(withId(R.id.fragment_home));
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
        launchHomeFragment();

        openOptionsMenu();
        onView(withText("Change WebView Provider")).check(matches(isDisplayed()));
        onView(withText("Change WebView Provider")).perform(click());
        intended(IntentMatchers.hasAction(Settings.ACTION_WEBVIEW_SETTINGS));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuOptions_reportBug() throws Throwable {
        launchHomeFragment();

        openOptionsMenu();

        ViewUtils.waitForView(withText("Report WebView Bug"));
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
        launchHomeFragment();

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

        openOptionsMenu();
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
        launchHomeFragment();

        openOptionsMenu();

        onView(withText("About WebView DevTools")).check(matches(isDisplayed()));
        onView(withText("About WebView DevTools")).perform(click());
        intended(allOf(IntentMatchers.hasAction(Intent.ACTION_VIEW),
                IntentMatchers.hasData(hasScheme("https")),
                IntentMatchers.hasData(hasHost("chromium.googlesource.com")),
                IntentMatchers.hasData(
                        hasPath("/chromium/src/+/HEAD/android_webview/docs/developer-ui.md"))));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testMenuOptions_components() throws Throwable {
        launchHomeFragment();

        openOptionsMenu();

        onView(withText("Components")).check(matches(isDisplayed()));
        onView(withText("Components")).perform(click());

        onView(withId(R.id.fragment_components_list)).check(matches(isDisplayed()));
    }

    private void switchToFlagsUi() {
        onView(withId(R.id.navigation_flags_ui)).perform(click());
    }

    private void checkFlagSpinnersEnabledState(boolean shouldBeEnabled) {
        // Test assumes that the first element in the list is the text.
        DataInteraction flags = onData(anything()).inAdapterView(withId(R.id.flags_list));
        Matcher<View> criteria = shouldBeEnabled ? isEnabled() : not(isEnabled());
        // Check the first actual flag, and assume the rest have the same state.
        // This avoids a lengthy UI scroll through the flag list.
        flags.atPosition(1).onChildView(withId(R.id.flag_toggle)).check(matches(criteria));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPostNotificationPermissions_preT() throws Throwable {
        launchHomeFragment();
        MainActivity activity = mRule.getActivity();
        activity.setIsAtLeastTBuildForTesting(false);

        assertFalse(activity.needToRequestPostNotificationPermission());

        switchToFlagsUi();

        checkFlagSpinnersEnabledState(true);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPostNotificationPermissions_T_notYetRequested() throws Throwable {
        launchHomeFragment();
        MainActivity activity = mRule.getActivity();
        activity.setIsAtLeastTBuildForTesting(true);

        assertTrue(activity.needToRequestPostNotificationPermission());

        switchToFlagsUi();

        // Check that the popup is visible, and then dismiss it
        onView(withText(MainActivity.NOTIFICATION_PERMISSION_REQUEST_MESSAGE))
                .check(matches(isDisplayed()));
        onView(withText("Cancel")).check(matches(isDisplayed())).perform(click());

        checkFlagSpinnersEnabledState(false);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPostNotificationPermissions_T_alreadyRequested() throws Throwable {
        launchHomeFragment();
        MainActivity activity = mRule.getActivity();

        activity.setIsAtLeastTBuildForTesting(true);
        SharedPreferences preferences = activity.getPreferences(Context.MODE_PRIVATE);
        preferences.edit()
                .putBoolean(MainActivity.POST_NOTIFICATIONS_PERMISSION_REQUESTED_KEY, true)
                .apply();

        assertFalse(activity.needToRequestPostNotificationPermission());

        switchToFlagsUi();
        checkFlagSpinnersEnabledState(true);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testPostNotificationPermissions_T_permissionGranted() throws Throwable {
        launchHomeFragment();
        MainActivity activity = mRule.getActivity();

        activity.setIsAtLeastTBuildForTesting(true);
        activity.runOnUiThread(() -> {
            // Need to run on the UI thread as it directly changes the view
            activity.onRequestPermissionsResult(
                    0, new String[] {"android.permission.POST_NOTIFICATIONS"}, new int[] {0});
        });

        // Getting the permission result should have switched us to fragment_flags
        onView(withId(R.id.fragment_flags)).check(matches(isDisplayed()));
        checkFlagSpinnersEnabledState(true);

        assertFalse("We should no longer need to ask for permission",
                activity.needToRequestPostNotificationPermission());
    }
}
