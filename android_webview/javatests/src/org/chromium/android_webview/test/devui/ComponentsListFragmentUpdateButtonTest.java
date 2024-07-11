// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.action.ViewActions.click;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.anything;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.MediumTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.devui.ComponentsListFragment;
import org.chromium.android_webview.devui.MainActivity;
import org.chromium.android_webview.devui.R;
import org.chromium.android_webview.nonembedded_util.WebViewPackageHelper;
import org.chromium.android_webview.services.ComponentsProviderPathUtil;
import org.chromium.android_webview.test.AwJUnit4ClassRunner;
import org.chromium.android_webview.test.services.MockAwComponentUpdateService;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.DoNotBatch;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.util.concurrent.ExecutionException;

/** UI tests for the Components UI's Update Button. */
@RunWith(AwJUnit4ClassRunner.class)
@DoNotBatch(reason = "Batching causes test failures.")
public class ComponentsListFragmentUpdateButtonTest {
    @Rule
    public BaseActivityTestRule<MainActivity> mRule =
            new BaseActivityTestRule<>(MainActivity.class);

    private static File sComponentsDownloadDir =
            new File(ComponentsProviderPathUtil.getComponentUpdateServiceDirectoryPath());

    @Before
    public void setUp() {
        Context context = ContextUtils.getApplicationContext();
        WebViewPackageHelper.setCurrentWebViewPackageForTesting(
                WebViewPackageHelper.getContextPackageInfo(context));
    }

    @After
    public void tearDown() {
        if (sComponentsDownloadDir.exists()) {
            Assert.assertTrue(FileUtils.recursivelyDeleteFile(sComponentsDownloadDir, null));
        }
    }

    private CallbackHelper getComponentInfoLoadedListener() throws ExecutionException {
        return ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final CallbackHelper helper = new CallbackHelper();
                    ComponentsListFragment.setComponentInfoLoadedListenerForTesting(
                            () -> {
                                helper.notifyCalled();
                            });
                    return helper;
                });
    }

    private void launchComponentsFragment() {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), MainActivity.class);
        intent.putExtra(MainActivity.FRAGMENT_ID_INTENT_EXTRA, MainActivity.FRAGMENT_ID_COMPONENTS);
        mRule.launchActivity(intent);

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    ComponentsListFragment.setComponentUpdateServiceNameForTesting(
                            "org.chromium.android_webview.test.services.MockAwComponentUpdateService");
                });
        onView(withId(R.id.fragment_components_list)).check(matches(isDisplayed()));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testUiPopulated() throws Throwable {
        CallbackHelper componentInfoLoadedHelper = getComponentInfoLoadedListener();
        int componentInfoListLoadInitCount = componentInfoLoadedHelper.getCallCount();
        launchComponentsFragment();
        componentInfoLoadedHelper.waitForCallback(componentInfoListLoadInitCount, 1);
        componentInfoListLoadInitCount = componentInfoLoadedHelper.getCallCount();

        onView(withId(R.id.components_summary_textview)).check(matches(withText("Components (0)")));
        CallbackHelper serviceStoppedHelper =
                MockAwComponentUpdateService.getServiceFinishedCallbackHelper();
        int serviceStoppedCount = serviceStoppedHelper.getCallCount();
        onView(withId(R.id.options_menu_update)).perform(click());

        serviceStoppedHelper.waitForCallback(serviceStoppedCount, 1);
        MockAwComponentUpdateService.sendResultReceiverCallback();
        componentInfoLoadedHelper.waitForCallback(componentInfoListLoadInitCount, 1);

        onView(withId(R.id.components_summary_textview)).check(matches(withText("Components (2)")));

        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText(MockAwComponentUpdateService.MOCK_COMPONENT_A_NAME)));

        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text2))
                .check(
                        matches(
                                withText(
                                        "Version: "
                                                + MockAwComponentUpdateService
                                                        .MOCK_COMPONENT_A_VERSION)));

        onData(anything())
                .atPosition(1)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText(MockAwComponentUpdateService.MOCK_COMPONENT_B_NAME)));

        onData(anything())
                .atPosition(1)
                .onChildView(withId(android.R.id.text2))
                .check(
                        matches(
                                withText(
                                        "Version: "
                                                + MockAwComponentUpdateService
                                                        .MOCK_COMPONENT_B_VERSION)));
    }

    /**
     * Test that no crash happens if {@code MockAwComponentUpdateService.sFinishCallback}
     * is received when {@code ComponentsListFragment} is not visible to the user.
     */
    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testUpdateCallbackReceived_fragmentNotVisible() throws Throwable {
        CallbackHelper componentInfoLoadedHelper = getComponentInfoLoadedListener();
        int componentInfoListLoadInitCount = componentInfoLoadedHelper.getCallCount();
        launchComponentsFragment();
        componentInfoLoadedHelper.waitForCallback(componentInfoListLoadInitCount, 1);

        onView(withId(R.id.components_summary_textview)).check(matches(withText("Components (0)")));
        CallbackHelper serviceStoppedHelper =
                MockAwComponentUpdateService.getServiceFinishedCallbackHelper();
        int serviceStoppedCount = serviceStoppedHelper.getCallCount();

        onView(withId(R.id.options_menu_update)).perform(click());
        onView(withId(R.id.navigation_home)).perform(click());
        serviceStoppedHelper.waitForCallback(serviceStoppedCount, 1);
        MockAwComponentUpdateService.sendResultReceiverCallback();
        onView(withId(R.id.fragment_home)).check(matches(isDisplayed()));
    }
}
