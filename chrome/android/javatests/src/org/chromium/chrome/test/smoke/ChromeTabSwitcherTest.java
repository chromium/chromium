// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.smoke;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.net.Uri;

import androidx.test.InstrumentationRegistry;
import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.LargeTest;

import org.junit.Before;
import org.junit.ClassRule;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.Log;
import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.R;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.smoke.utilities.FirstRunNavigator;
import org.chromium.net.test.EmbeddedTestServerRule;

/** Basic Test for Chrome Android to switch Tabs. */
@LargeTest
@RunWith(BaseJUnit4ClassRunner.class)
public class ChromeTabSwitcherTest {
    private static final String TAG = "SmokeTest";
    private static final String ACTIVITY_NAME = "com.google.android.apps.chrome.IntentDispatcher";
    private static final String TEST_PAGE =
            "/chrome/android/javatests/src/org/chromium/chrome/test/smoke/test.html";

    private IUi2Locator mTabSwitcherButton = Ui2Locators.withAnyResEntry(R.id.tab_switcher_button);

    private IUi2Locator mHubToolbar = Ui2Locators.withAnyResEntry(R.id.hub_toolbar);

    private IUi2Locator mTabList = Ui2Locators.withAnyResEntry(R.id.tab_list_recycler_view);

    private FirstRunNavigator mFirstRunNavigator = new FirstRunNavigator();

    public static final long TIMEOUT_MS = 20000L;
    public static final long UI_CHECK_INTERVAL = 1000L;
    private String mPackageName;
    public ChromeUiAutomatorTestRule mRule = new ChromeUiAutomatorTestRule();
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();
    @Rule public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mRule);

    @ClassRule
    public static EmbeddedTestServerRule sEmbeddedTestServerRule = new EmbeddedTestServerRule();

    @Before
    public void setUp() throws Exception {
        mPackageName =
                InstrumentationRegistry.getArguments()
                        .getString(
                                ChromeUiApplicationTestRule.PACKAGE_NAME_ARG,
                                "org.chromium.chrome");
    }

    @Test
    public void testTabSwitcher() throws Exception {
        Context context = ApplicationProvider.getApplicationContext();
        String url = sEmbeddedTestServerRule.getServer().getURL(TEST_PAGE);
        final Intent intent = new Intent(Intent.ACTION_VIEW, Uri.parse(url));
        intent.addCategory(Intent.CATEGORY_BROWSABLE);
        intent.addCategory(Intent.CATEGORY_DEFAULT);
        intent.addFlags(Intent.FLAG_ACTIVITY_CLEAR_TASK | Intent.FLAG_ACTIVITY_NEW_TASK);
        intent.setComponent(new ComponentName(mPackageName, ACTIVITY_NAME));
        context.startActivity(intent);

        // Looks for the any view/layout with the chrome package name.
        IUi2Locator locatorChrome = Ui2Locators.withPackageName(mPackageName);
        // Wait until chrome shows up
        Log.i(TAG, "Attempting to navigate through FRE");
        UiAutomatorUtils.getInstance().waitUntilAnyVisible(locatorChrome);

        // Go through the FRE until you see ChromeTabbedActivity urlbar.
        Log.i(TAG, "Waiting for omnibox to show URL");
        mFirstRunNavigator.navigateThroughFRE();

        Log.i(TAG, "Waiting for omnibox to show URL");
        assert url.startsWith("http://");
        String urlWithoutScheme = url.substring(7);
        IUi2Locator dataUrlText = Ui2Locators.withText(urlWithoutScheme);
        UiAutomatorUtils.getInstance().getLocatorHelper().verifyOnScreen(dataUrlText);

        Log.i(TAG, "Waiting 5 seconds to ensure background logic does not crash");
        Thread.sleep(5000);

        Log.i(TAG, "Activating tab switcher.");
        UiAutomatorUtils.getInstance().click(mTabSwitcherButton);
        UiAutomatorUtils.getInstance().waitUntilAnyVisible(mHubToolbar);
        UiAutomatorUtils.getInstance().getLocatorHelper().verifyOnScreen(mTabList);

        Log.i(TAG, "Test complete.");
    }
}
