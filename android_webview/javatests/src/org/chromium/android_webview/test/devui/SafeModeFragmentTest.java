// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isAssignableFrom;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.CoreMatchers.allOf;
import static org.hamcrest.Matchers.anything;
import static org.hamcrest.Matchers.not;

import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.withCount;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.os.RemoteException;
import android.view.View;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.test.espresso.UiController;
import androidx.test.espresso.ViewAction;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.hamcrest.Matcher;
import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.services.ISafeModeService;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.devui.SafeModeFragment;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.android_webview.services.SafeModeService;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.services.ServiceConnectionHelper;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.Feature;
import org.chromium.ui.test.util.ViewUtils;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.Date;
import java.util.List;

/** UI tests for {@link SafeModeFragment}. */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class SafeModeFragmentTest {
    @Rule
    public BaseActivityTestRule mRule = new BaseActivityTestRule<MainActivity>(MainActivity.class);

    private void launchSafeModeFragment() {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), MainActivity.class);
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_SAFEMODE);
        mRule.launchActivity(intent);
        onView(withId(R.id.fragment_safe_mode)).check(matches(isDisplayed()));
    }

    private void checkActionsDisplayed(List<String> actionIds) {
        onView(withId(R.id.safe_mode_actions_list)).check(matches(withCount(actionIds.size())));
        List<String> actionsDisplayed = new ArrayList<>();
        for (int i = 0; i < actionIds.size(); i++) {
            onData(anything())
                    .atPosition(i)
                    .perform(
                            new ViewAction() {
                                @Override
                                public Matcher<View> getConstraints() {
                                    return isAssignableFrom(TextView.class);
                                }

                                @Override
                                public String getDescription() {
                                    return "Get text of a TextView";
                                }

                                @Override
                                public void perform(UiController uiController, View view) {
                                    TextView textView =
                                            (TextView) view; // Save, because of check in
                                    // getConstraints()
                                    actionsDisplayed.add(textView.getText().toString());
                                }
                            });
        }
        // we don't require a specific order of the displayed safemode actions.
        Collections.sort(actionIds);
        Collections.sort(actionsDisplayed);
        Assert.assertEquals(actionIds, actionsDisplayed);
    }

    private static SafeModeAction getNoopAction(String actionId) {
        return new SafeModeAction() {
            @NonNull
            @Override
            public String getId() {
                return actionId;
            }

            @Override
            public boolean execute() {
                return true;
            }
        };
    }

    @Before
    public void setUp() {
        final Context context = ContextUtils.getApplicationContext();
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
    }

    @After
    public void tearDown() throws Throwable {
        // Reset component state back to the default.
        final Context context = ContextUtils.getApplicationContext();
        ComponentName safeModeComponent =
                new ComponentName(context, SafeModeController.SAFE_MODE_STATE_COMPONENT);
        context.getPackageManager()
                .setComponentEnabledSetting(
                        safeModeComponent,
                        PackageManager.COMPONENT_ENABLED_STATE_DEFAULT,
                        PackageManager.DONT_KILL_APP);

        SafeModeService.clearSharedPrefsForTesting();
        SafeModeController.getInstance().unregisterActionsForTesting();
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHasPublicNoArgsConstructor() throws Throwable {
        SafeModeFragment fragment = new SafeModeFragment();
        Assert.assertNotNull(fragment);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testUIShowsSafeModeDisabledByDefault() throws Throwable {
        launchSafeModeFragment();
        onView(withId(R.id.safe_mode_state)).check(matches(withText("Disabled")));
        onView(withId(R.id.safe_mode_actions_container)).check(matches(not(isDisplayed())));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testEnableSafeModeWithOneAction() throws Throwable {
        final long initialStartTimeMs = 12345L;
        final String actionId = "action_id";
        SafeModeService.setClockForTesting(() -> initialStartTimeMs);
        setSafeMode(Arrays.asList(actionId));

        launchSafeModeFragment();

        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.safe_mode_state), not(withText("")), not(withText("Enabled"))));
        onView(withId(R.id.safe_mode_state))
                .check(matches(withText("Enabled on " + new Date(initialStartTimeMs).toString())));
        onView(withId(R.id.safe_mode_actions_list)).check(matches(withCount(1)));
        onData(anything()).atPosition(0).check(matches(withText(actionId)));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testEnableSafeModeWithMultipleActions() throws Throwable {
        final long initialStartTimeMs = 12345L;
        List<String> actionIds = Arrays.asList("action_id1", "action_id2", "action_id3");
        SafeModeService.setClockForTesting(() -> initialStartTimeMs);
        setSafeMode(actionIds);

        launchSafeModeFragment();

        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.safe_mode_state), not(withText("")), not(withText("Enabled"))));
        onView(withId(R.id.safe_mode_state))
                .check(matches(withText("Enabled on " + new Date(initialStartTimeMs).toString())));
        checkActionsDisplayed(actionIds);
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testRelaunchFragmentAfterEnablingSafeMode() throws Throwable {
        launchSafeModeFragment();
        onView(withId(R.id.safe_mode_state)).check(matches(withText("Disabled")));

        final long initialStartTimeMs = 12345L;
        final String actionId = "action_id";
        SafeModeService.setClockForTesting(() -> initialStartTimeMs);
        setSafeMode(Arrays.asList(actionId));
        mRule.recreateActivity();
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.safe_mode_state), not(withText("")), not(withText("Enabled"))));
        onView(withId(R.id.safe_mode_state))
                .check(matches(withText("Enabled on " + new Date(initialStartTimeMs).toString())));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testRelaunchFragmentAfterDisablingSafeMode() throws Throwable {
        final long initialStartTimeMs = 12345L;
        final String actionId = "action_id";
        SafeModeService.setClockForTesting(() -> initialStartTimeMs);
        setSafeMode(Arrays.asList(actionId));
        launchSafeModeFragment();
        ViewUtils.waitForVisibleView(
                allOf(withId(R.id.safe_mode_state), not(withText("")), not(withText("Enabled"))));
        onView(withId(R.id.safe_mode_state))
                .check(matches(withText("Enabled on " + new Date(initialStartTimeMs).toString())));

        SafeModeService.setSafeMode(Arrays.asList());
        mRule.recreateActivity();
        onView(withId(R.id.safe_mode_state)).check(matches(withText("Disabled")));
    }

    private void setSafeMode(List<String> actions) throws RemoteException {
        SafeModeAction[] safeModeActions =
                actions.stream()
                        .map(SafeModeFragmentTest::getNoopAction)
                        .toArray(SafeModeAction[]::new);
        SafeModeController.getInstance().registerActions(safeModeActions);
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        try (ServiceConnectionHelper helper =
                new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(actions);
        }
    }
}
