// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.test.smoke;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.support.test.InstrumentationRegistry;
import android.support.test.filters.SmallTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.RuleChain;
import org.junit.rules.TestRule;
import org.junit.runner.RunWith;

import org.chromium.base.test.BaseJUnit4ClassRunner;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiApplicationTestRule;
import org.chromium.chrome.test.pagecontroller.rules.ChromeUiAutomatorTestRule;
import org.chromium.chrome.test.pagecontroller.utils.IUi2Locator;
import org.chromium.chrome.test.pagecontroller.utils.Ui2Locators;
import org.chromium.chrome.test.pagecontroller.utils.UiAutomatorUtils;
import org.chromium.chrome.test.pagecontroller.utils.UiLocatorHelper;

/** Smoke Test for Chrome bundles. */
@SmallTest
@RunWith(BaseJUnit4ClassRunner.class)
public class ChromeBundleSmokeTest {
    private static final String TARGET_ACTIVITY =
            "org.chromium.chrome.features.test_dummy.TestDummyActivity";

    public ChromeUiAutomatorTestRule mRule = new ChromeUiAutomatorTestRule();
    public ChromeUiApplicationTestRule mChromeUiRule = new ChromeUiApplicationTestRule();
    @Rule
    public final TestRule mChain = RuleChain.outerRule(mChromeUiRule).around(mRule);

    private String mPackageName;

    @Before
    public void setUp() {
        mPackageName = InstrumentationRegistry.getArguments().getString(
                ChromeUiApplicationTestRule.PACKAGE_NAME_ARG);
        Assert.assertNotNull("Must specify bundle under test", mPackageName);
        mChromeUiRule.launchIntoNewTabPageOnFirstRun();
    }

    private void runTestActivity(int testCase) {
        // This intent will trigger installation of the module if not present.
        Context context = InstrumentationRegistry.getContext();
        Intent intent = new Intent();
        intent.setComponent(new ComponentName(mPackageName, TARGET_ACTIVITY));
        intent.putExtra("test_case", testCase);
        intent.addFlags(Intent.FLAG_ACTIVITY_SINGLE_TOP | Intent.FLAG_ACTIVITY_NEW_TASK);
        context.startActivity(intent);

        final String prefixText = "Test Case " + testCase + ": ";
        IUi2Locator locator = Ui2Locators.withTextContaining(prefixText);

        // Wait for result dialog to show up.
        UiLocatorHelper locatorHelper = UiAutomatorUtils.getInstance().getLocatorHelper();
        Assert.assertTrue(locatorHelper.isOnScreen(locator));

        // Ensure the dialog text indicates a pass.
        final String passText = prefixText + "pass";
        Assert.assertEquals(locatorHelper.getOneTextImmediate(locator, null), passText);
    }

    @Test
    public void testModuleJavaCodeExecution() {
        runTestActivity(0); // Test case EXECUTE_JAVA.
    }

    @Test
    public void testModuleNativeCodeExecution() {
        runTestActivity(1); // Test case EXECUTE_NATIVE.
    }

    @Test
    public void testModuleJavaResourceLoading() {
        runTestActivity(2); // Test case LOAD_JAVA_RESOURCE.
    }

    @Test
    public void testModuleNativeResourceLoading() {
        runTestActivity(3); // Test case LOAD_NATIVE_RESOURCE.
    }
}
