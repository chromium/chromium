// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;

import androidx.annotation.NonNull;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;

import org.junit.After;
import org.junit.Assert;
import org.junit.Assume;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.android_webview.BrowserSafeModeActionList;
import org.chromium.android_webview.common.SafeModeAction;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.services.ISafeModeService;
import org.chromium.android_webview.common.variations.VariationsUtils;
import org.chromium.android_webview.services.SafeModeService;
import org.chromium.android_webview.test.VariationsSeedLoaderTest.TestLoader;
import org.chromium.android_webview.test.VariationsSeedLoaderTest.TestLoaderResult;
import org.chromium.android_webview.test.services.ServiceConnectionHelper;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.android_webview.variations.VariationsSeedSafeModeAction;
import org.chromium.base.ContextUtils;
import org.chromium.base.test.util.Feature;
import org.chromium.build.BuildConfig;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test WebView SafeMode.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class SafeModeTest {
    // The package name of the test shell. This is acting both as the client app and the WebView
    // provider.
    public static final String TEST_WEBVIEW_PACKAGE_NAME = "org.chromium.android_webview.shell";

    private AtomicInteger mTestSafeModeActionExecutionCounter;

    @Rule
    public AwActivityTestRule mActivityTestRule = new AwActivityTestRule();

    @Before
    public void setUp() throws Throwable {
        mTestSafeModeActionExecutionCounter = new AtomicInteger(0);
    }

    @After
    public void tearDown() throws Throwable {
        // Reset component state back to the default.
        final Context context = ContextUtils.getApplicationContext();
        ComponentName safeModeComponent = new ComponentName(
                TEST_WEBVIEW_PACKAGE_NAME, SafeModeController.SAFE_MODE_STATE_COMPONENT);
        context.getPackageManager().setComponentEnabledSetting(safeModeComponent,
                PackageManager.COMPONENT_ENABLED_STATE_DEFAULT, PackageManager.DONT_KILL_APP);

        SafeModeController.getInstance().unregisterActionsForTesting();
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

    private class TestSafeModeAction implements SafeModeAction {
        private int mCallCount;
        private int mExecutionOrder;
        private final String mId;

        TestSafeModeAction(String id) {
            mId = id;
        }

        @Override
        @NonNull
        public String getId() {
            return mId;
        }

        @Override
        public void execute() {
            mCallCount++;
            mExecutionOrder = mTestSafeModeActionExecutionCounter.incrementAndGet();
        }

        public int getCallCount() {
            return mCallCount;
        }

        public int getExecutionOrder() {
            return mExecutionOrder;
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_cannotRegisterActionsTwice() throws Throwable {
        TestSafeModeAction testAction1 = new TestSafeModeAction("test1");
        TestSafeModeAction testAction2 = new TestSafeModeAction("test2");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction1});
        try {
            SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction2});
            Assert.fail("SafeModeController should have thrown an exception when "
                    + "re-registering actions");
        } catch (IllegalStateException e) {
            // Expected
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_cannotRegisterDuplicateActionId() throws Throwable {
        Assume.assumeTrue("This behavior is only in debug builds for performance reasons",
                BuildConfig.ENABLE_ASSERTS);
        TestSafeModeAction testAction1 = new TestSafeModeAction("test1");
        TestSafeModeAction testAction2 = new TestSafeModeAction("test1");
        try {
            SafeModeController.getInstance().registerActions(
                    new SafeModeAction[] {testAction1, testAction2});
            Assert.fail("SafeModeController should have thrown an exception for "
                    + "a duplicate ID");
        } catch (IllegalArgumentException e) {
            // Expected
        }
    }

    private static <T> Set<T> asSet(T... values) {
        Set<T> set = new HashSet<>();
        for (T value : values) {
            set.add(value);
        }
        return set;
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_mustRegisterBeforeExecuting() throws Throwable {
        try {
            Set<String> actions = asSet("test");
            SafeModeController.getInstance().executeActions(actions);
            Assert.fail("SafeModeController should have thrown an exception when "
                    + "executing without registering");
        } catch (IllegalStateException e) {
            // Expected
        }
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_executesRegisteredAction() throws Throwable {
        TestSafeModeAction testAction = new TestSafeModeAction("test");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});

        Set<String> actions = asSet("test");
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals("TestSafeModeAction should have been executed exactly 1 time", 1,
                testAction.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_doesNotExecuteUnregisteredActions() throws Throwable {
        TestSafeModeAction testAction = new TestSafeModeAction("test");
        SafeModeController.getInstance().registerActions(new SafeModeAction[] {testAction});

        Set<String> actions = asSet("test", "unregistered1", "unregistered2");
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals("TestSafeModeAction should have been executed exactly 1 time", 1,
                testAction.getCallCount());
        // If we got this far without crashing, we assume SafeModeController correctly ignored the
        // unregistered actions.
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_onlyExecutesSpecifiedActions() throws Throwable {
        TestSafeModeAction testAction1 = new TestSafeModeAction("test1");
        TestSafeModeAction testAction2 = new TestSafeModeAction("test2");
        SafeModeController.getInstance().registerActions(
                new SafeModeAction[] {testAction1, testAction2});

        Set<String> actions = asSet("test1");
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals("testAction1 should have been executed exactly 1 time", 1,
                testAction1.getCallCount());
        Assert.assertEquals(
                "testAction2 should not have been executed", 0, testAction2.getCallCount());
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_executesActionsInOrder() throws Throwable {
        TestSafeModeAction testAction1 = new TestSafeModeAction("test1");
        TestSafeModeAction testAction2 = new TestSafeModeAction("test2");
        TestSafeModeAction testAction3 = new TestSafeModeAction("test3");

        Set<String> actions = asSet(testAction1.getId(), testAction2.getId(), testAction3.getId());

        SafeModeController.getInstance().registerActions(
                new SafeModeAction[] {testAction1, testAction2, testAction3});
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals(
                "testAction1 should be executed first", 1, testAction1.getExecutionOrder());
        Assert.assertEquals(
                "testAction2 should be executed second", 2, testAction2.getExecutionOrder());
        Assert.assertEquals(
                "testAction3 should be executed third", 3, testAction3.getExecutionOrder());

        // Unregister and re-register in the opposite order. Verify that they're executed in the new
        // registration order.
        SafeModeController.getInstance().unregisterActionsForTesting();
        SafeModeController.getInstance().registerActions(
                new SafeModeAction[] {testAction3, testAction2, testAction1});
        SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals("testAction3 should be executed first the next time", 4,
                testAction3.getExecutionOrder());
        Assert.assertEquals("testAction2 should be executed second the next time", 5,
                testAction2.getExecutionOrder());
        Assert.assertEquals("testAction1 should be executed third the next time", 6,
                testAction1.getExecutionOrder());
    }

    @Test
    @MediumTest
    public void testSafeModeAction_canRegisterBrowserActions() throws Exception {
        // Validity check: verify we can register the production SafeModeAction list. As long as
        // this finishes without throwing, assume the list is in good shape (e.g., no duplicate
        // SafeModeAction IDs).
        SafeModeController.getInstance().registerActions(BrowserSafeModeActionList.sList);
    }

    @Test
    @MediumTest
    public void testVariations_deletesSeedFiles() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("Seed file already exists", oldFile.createNewFile());
            Assert.assertTrue("New seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);
            VariationsTestUtils.writeMockSeed(newFile);
            VariationsSeedSafeModeAction action = new VariationsSeedSafeModeAction();
            action.execute();
            Assert.assertFalse(
                    "Old seed should have been deleted but it still exists", oldFile.exists());
            Assert.assertFalse(
                    "New seed should have been deleted but it still exists", newFile.exists());
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    public void testVariations_doesNotLoadExperiments() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            Assert.assertTrue("Seed file already exists", oldFile.createNewFile());
            Assert.assertTrue("New seed file already exists", newFile.createNewFile());
            VariationsTestUtils.writeMockSeed(oldFile);
            VariationsTestUtils.writeMockSeed(newFile);
            VariationsSeedSafeModeAction action = new VariationsSeedSafeModeAction();
            action.execute();

            TestLoader loader = new TestLoader(new TestLoaderResult());
            loader.startVariationsInit();
            boolean loadedSeed = loader.finishVariationsInit();
            Assert.assertFalse(
                    "Loaded a variations seed even though it should have been deleted by SafeMode",
                    loadedSeed);
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    public void testVariations_doesNothingIfSeedDoesNotExist() throws Exception {
        try {
            File oldFile = VariationsUtils.getSeedFile();
            File newFile = VariationsUtils.getNewSeedFile();
            VariationsSeedSafeModeAction action = new VariationsSeedSafeModeAction();
            action.execute();
            Assert.assertFalse("Old seed should never have existed", oldFile.exists());
            Assert.assertFalse("New seed should never have existed", newFile.exists());
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }
}
