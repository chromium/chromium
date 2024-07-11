// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test.devui;

import static androidx.test.espresso.Espresso.onData;
import static androidx.test.espresso.Espresso.onView;
import static androidx.test.espresso.assertion.ViewAssertions.matches;
import static androidx.test.espresso.matcher.ViewMatchers.isDisplayed;
import static androidx.test.espresso.matcher.ViewMatchers.withId;
import static androidx.test.espresso.matcher.ViewMatchers.withText;

import static org.hamcrest.Matchers.anything;

import static org.chromium.android_webview.test.devui.DeveloperUiTestUtils.withCount;

import android.content.Context;
import android.content.Intent;

import androidx.test.filters.SmallTest;

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
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.test.BaseActivityTestRule;
import org.chromium.base.test.util.Batch;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.Feature;

import java.io.File;
import java.util.concurrent.ExecutionException;

/** UI tests for the Components UI's fragment. */
@RunWith(AwJUnit4ClassRunner.class)
@Batch(Batch.PER_CLASS)
public class ComponentsListFragmentTest {
    @Rule
    public BaseActivityTestRule mRule = new BaseActivityTestRule<MainActivity>(MainActivity.class);

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
        onView(withId(R.id.fragment_components_list)).check(matches(isDisplayed()));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testHasPublicNoArgsConstructor() throws Throwable {
        ComponentsListFragment fragment = new ComponentsListFragment();
        Assert.assertNotNull(fragment);
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testComponentsDownloadDirectory_isEmpty() throws Throwable {
        sComponentsDownloadDir.mkdirs();
        CallbackHelper helper = getComponentInfoLoadedListener();
        int componentInfoListLoadInitCount = helper.getCallCount();
        launchComponentsFragment();
        helper.waitForCallback(componentInfoListLoadInitCount, 1);

        onView(withId(R.id.components_summary_textview)).check(matches(withText("Components (0)")));
        onView(withId(R.id.components_list)).check(matches(withCount(0)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testComponentsDownloadDirectory_doesNotExist() throws Throwable {
        CallbackHelper helper = getComponentInfoLoadedListener();
        int componentInfoListLoadInitCount = helper.getCallCount();
        launchComponentsFragment();
        helper.waitForCallback(componentInfoListLoadInitCount, 1);

        onView(withId(R.id.components_summary_textview)).check(matches(withText("Components (0)")));
        onView(withId(R.id.components_list)).check(matches(withCount(0)));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testVersionSubdirectory_doesNotExist() throws Throwable {
        new File(sComponentsDownloadDir, "MockComponent A").mkdirs();
        CallbackHelper helper = getComponentInfoLoadedListener();
        int componentInfoListLoadInitCount = helper.getCallCount();
        launchComponentsFragment();
        helper.waitForCallback(componentInfoListLoadInitCount, 1);

        onView(withId(R.id.components_summary_textview)).check(matches(withText("Components (1)")));
        onView(withId(R.id.components_list)).check(matches(withCount(1)));

        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText("No installed versions.")));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testComponentsDownloadDirectory_notMalformed() throws Throwable {
        new File(sComponentsDownloadDir, "MockComponent A/1.0.2.3").mkdirs();
        new File(sComponentsDownloadDir, "MockComponent B/2021.0.2.3").mkdirs();
        CallbackHelper helper = getComponentInfoLoadedListener();
        int componentInfoListLoadInitCount = helper.getCallCount();
        launchComponentsFragment();
        helper.waitForCallback(componentInfoListLoadInitCount, 1);

        onView(withId(R.id.components_summary_textview)).check(matches(withText("Components (2)")));
        onView(withId(R.id.components_list)).check(matches(withCount(2)));

        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText("MockComponent A")));

        onData(anything())
                .atPosition(0)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText("Version: 1.0.2.3")));

        onData(anything())
                .atPosition(1)
                .onChildView(withId(android.R.id.text1))
                .check(matches(withText("MockComponent B")));

        onData(anything())
                .atPosition(1)
                .onChildView(withId(android.R.id.text2))
                .check(matches(withText("Version: 2021.0.2.3")));
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testLexicographicalOrder() throws Throwable {
        final String sortedAlphabet = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
        final String shuffledAlphabet = "JNOAHXWCPITBSDLVZFQRYUGEMK";
        for (int i = 0; i < 26; i++) {
            new File(sComponentsDownloadDir, "MockComponent " + shuffledAlphabet.charAt(i))
                    .mkdirs();
        }
        CallbackHelper helper = getComponentInfoLoadedListener();
        int componentInfoListLoadInitCount = helper.getCallCount();
        launchComponentsFragment();
        helper.waitForCallback(componentInfoListLoadInitCount, 1);

        onView(withId(R.id.components_summary_textview))
                .check(matches(withText("Components (26)")));
        onView(withId(R.id.components_list)).check(matches(withCount(26)));
        for (int i = 0; i < 26; i++) {
            onData(anything())
                    .atPosition(i)
                    .onChildView(withId(android.R.id.text1))
                    .check(matches(withText("MockComponent " + sortedAlphabet.charAt(i))));
        }
    }
}
