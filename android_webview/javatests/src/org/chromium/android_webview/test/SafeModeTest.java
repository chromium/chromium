// Copyright 2021 The Chromium Authors
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
import org.chromium.android_webview.services.SafeModeService.TrustedPackage;
import org.chromium.android_webview.test.VariationsSeedLoaderTest.TestLoader;
import org.chromium.android_webview.test.VariationsSeedLoaderTest.TestLoaderResult;
import org.chromium.android_webview.test.services.ServiceConnectionHelper;
import org.chromium.android_webview.test.util.VariationsTestUtils;
import org.chromium.android_webview.variations.VariationsSeedSafeModeAction;
import org.chromium.base.ContextUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.test.util.Feature;
import org.chromium.build.BuildConfig;

import java.io.File;
import java.util.Arrays;
import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.TimeUnit;
import java.util.concurrent.atomic.AtomicInteger;

/**
 * Test WebView SafeMode.
 */
@RunWith(AwJUnit4ClassRunner.class)
public class SafeModeTest {
    // The package name of the test shell. This is acting both as the client app and the WebView
    // provider.
    public static final String TEST_WEBVIEW_PACKAGE_NAME = "org.chromium.android_webview.shell";

    // This is the actual certificate hash we use to sign webview_instrumentation_apk (Signer #1
    // certificate SHA-256 digest). This can be obtained by running:
    // $ out/Default/bin/webview_instrumentation_apk print-certs
    private static final byte[] TEST_WEBVIEW_CERT_HASH = new byte[] {(byte) 0x32, (byte) 0xa2,
            (byte) 0xfc, (byte) 0x74, (byte) 0xd7, (byte) 0x31, (byte) 0x10, (byte) 0x58,
            (byte) 0x59, (byte) 0xe5, (byte) 0xa8, (byte) 0x5d, (byte) 0xf1, (byte) 0x6d,
            (byte) 0x95, (byte) 0xf1, (byte) 0x02, (byte) 0xd8, (byte) 0x5b, (byte) 0x22,
            (byte) 0x09, (byte) 0x9b, (byte) 0x80, (byte) 0x64, (byte) 0xc5, (byte) 0xd8,
            (byte) 0x91, (byte) 0x5c, (byte) 0x61, (byte) 0xda, (byte) 0xd1, (byte) 0xe0};

    // Arbitrary sha256 digest which does not match TEST_WEBVIEW_PACKAGE_NAME's certificate.
    private static final byte[] FAKE_CERT_HASH = new byte[] {(byte) 0xFF, (byte) 0xFF, (byte) 0xFF,
            (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF,
            (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF,
            (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF,
            (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF,
            (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF, (byte) 0xFF};

    private static final String SAFEMODE_ACTION_NAME = "some_action_name";

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

        SafeModeService.clearSharedPrefsForTesting();
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
    public void testSafeModeState_enableWithMethod() throws Throwable {
        SafeModeService.setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME));
        Assert.assertTrue("SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testSafeModeState_disableWithMethod() throws Throwable {
        SafeModeService.setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME));
        Assert.assertTrue("SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));

        SafeModeService.setSafeMode(Arrays.asList());
        Assert.assertFalse("SafeMode should be re-disabled",
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
            service.setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME));
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
            service.setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME));
        }

        Assert.assertTrue("SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));

        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList());
        }

        Assert.assertFalse("SafeMode should be re-disabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_disabled() throws Throwable {
        Assert.assertEquals(
                "Querying the ContentProvider should yield empty set when SafeMode is disabled",
                asSet(), SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_singleAction() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList(variationsActionId));
        }

        Assert.assertTrue("SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals("Querying the ContentProvider should yield the action we set",
                asSet(variationsActionId),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_multipleActions() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList(SAFEMODE_ACTION_NAME, variationsActionId));
        }

        Assert.assertTrue("SafeMode should be enabled",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals("Querying the ContentProvider should yield the action we set",
                asSet(SAFEMODE_ACTION_NAME, variationsActionId),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_autoDisableAfter30Days() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        final long initialStartTimeMs = 12345L;
        SafeModeService.setClockForTesting(() -> { return initialStartTimeMs; });
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList(variationsActionId));
        }

        final long beforeTimeLimitMs =
                initialStartTimeMs + SafeModeService.SAFE_MODE_ENABLED_TIME_LIMIT_MS - 1L;
        SafeModeService.setClockForTesting(() -> { return beforeTimeLimitMs; });

        Assert.assertTrue("SafeMode should be enabled (before timeout)",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals("Querying the ContentProvider should yield the action we set",
                asSet(variationsActionId),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));

        final long afterTimeLimitMs =
                initialStartTimeMs + SafeModeService.SAFE_MODE_ENABLED_TIME_LIMIT_MS;
        SafeModeService.setClockForTesting(() -> { return afterTimeLimitMs; });

        Assert.assertTrue("SafeMode should be enabled until querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals("ContentProvider should return empty set after timeout", asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertFalse("SafeMode should be disabled after querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_autoDisableIfTimestampInFuture() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        final long initialStartTimeMs = 12345L;
        SafeModeService.setClockForTesting(() -> { return initialStartTimeMs; });
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList(variationsActionId));
        }

        // If the user manually sets their clock backward in time, then the time delta will be
        // negative. This case should also be treated as expired.
        final long queryTime = initialStartTimeMs - 1L;
        SafeModeService.setClockForTesting(() -> { return queryTime; });

        Assert.assertTrue("SafeMode should be enabled until querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals("ContentProvider should return empty set after timeout", asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertFalse("SafeMode should be disabled after querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_extendTimeoutWithDuplicateConfig() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        final long initialStartTimeMs = 12345L;
        SafeModeService.setClockForTesting(() -> { return initialStartTimeMs; });
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList(variationsActionId));
        }

        // Send a duplicate config after 1 day to extend the SafeMode timeout for another 30 days.
        final long duplicateConfigTimeMs = initialStartTimeMs + TimeUnit.DAYS.toMillis(1);
        SafeModeService.setClockForTesting(() -> { return duplicateConfigTimeMs; });
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList(variationsActionId));
        }

        // 30 days after the original timeout
        final long firstTimeLimitMs =
                initialStartTimeMs + SafeModeService.SAFE_MODE_ENABLED_TIME_LIMIT_MS;
        SafeModeService.setClockForTesting(() -> { return firstTimeLimitMs; });

        Assert.assertEquals(
                "Querying the ContentProvider should yield the action we set (timeout extended)",
                asSet(variationsActionId),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));

        final long secondTimeLimitMs =
                duplicateConfigTimeMs + SafeModeService.SAFE_MODE_ENABLED_TIME_LIMIT_MS;
        SafeModeService.setClockForTesting(() -> { return secondTimeLimitMs; });

        Assert.assertEquals("ContentProvider should return empty set after timeout", asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_autoDisableIfMissingTimestamp() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList(variationsActionId));
        }

        // If for some reason LAST_MODIFIED_TIME_KEY is unexpectedly missing, SafeMode should
        // disable itself.
        SafeModeService.removeSharedPrefKeyForTesting(SafeModeService.LAST_MODIFIED_TIME_KEY);

        Assert.assertTrue("SafeMode should be enabled until querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals("ContentProvider should return empty set after timeout", asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertFalse("SafeMode should be disabled after querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    @Feature({"AndroidWebView"})
    public void testQueryActions_autoDisableIfMissingActions() throws Throwable {
        Intent intent = new Intent(ContextUtils.getApplicationContext(), SafeModeService.class);
        final String variationsActionId = new VariationsSeedSafeModeAction().getId();
        try (ServiceConnectionHelper helper =
                        new ServiceConnectionHelper(intent, Context.BIND_AUTO_CREATE)) {
            ISafeModeService service = ISafeModeService.Stub.asInterface(helper.getBinder());
            service.setSafeMode(Arrays.asList(variationsActionId));
        }

        // If for some reason SAFEMODE_ACTIONS_KEY is unexpectedly missing (or the empty set),
        // SafeMode should disable itself.
        SafeModeService.removeSharedPrefKeyForTesting(SafeModeService.SAFEMODE_ACTIONS_KEY);

        Assert.assertTrue("SafeMode should be enabled until querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertEquals("ContentProvider should return empty set after timeout", asSet(),
                SafeModeController.getInstance().queryActions(TEST_WEBVIEW_PACKAGE_NAME));
        Assert.assertFalse("SafeMode should be disabled after querying ContentProvider",
                SafeModeController.getInstance().isSafeModeEnabled(TEST_WEBVIEW_PACKAGE_NAME));
    }

    private class TestSafeModeAction implements SafeModeAction {
        private int mCallCount;
        private int mExecutionOrder;
        private final String mId;
        private final boolean mSuccess;

        TestSafeModeAction(String id) {
            this(id, true);
        }

        TestSafeModeAction(String id, boolean success) {
            mId = id;
            mSuccess = success;
        }

        @Override
        @NonNull
        public String getId() {
            return mId;
        }

        @Override
        public boolean execute() {
            mCallCount++;
            mExecutionOrder = mTestSafeModeActionExecutionCounter.incrementAndGet();
            return mSuccess;
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

        Set<String> actions = asSet(testAction.getId(), "unregistered1", "unregistered2");
        @SafeModeController.SafeModeExecutionResult
        int success = SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals("TestSafeModeAction should have been executed exactly 1 time", 1,
                testAction.getCallCount());
        Assert.assertEquals(
                "Overall status should be unknown if at least one action is unrecognized and no actions failed",
                success, SafeModeController.SafeModeExecutionResult.ACTION_UNKNOWN);
        Assert.assertEquals("Unregistered safemode actions should be logged", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.SafeMode.ExecutionResult",
                        SafeModeController.SafeModeExecutionResult.ACTION_UNKNOWN));
        // If we got this far without crashing, we assume SafeModeController correctly ignored the
        // unregistered actions.
    }

    @Test
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_testStatusHierarchyEnforcedCorrectly() throws Throwable {
        TestSafeModeAction testActionFailed = new TestSafeModeAction("testFail", false);
        TestSafeModeAction testActionSuccess = new TestSafeModeAction("testSuccess");
        SafeModeController.getInstance().registerActions(
                new SafeModeAction[] {testActionSuccess, testActionFailed});

        // The possible execution statuses include SUCCESS, ACTION_FAILED, and ACTION_UNKNOWN.
        // The precedence is ACTION_FAILED, ACTION_UNKNOWN, and then SUCCESS in descending order.
        Set<String> actions = asSet(testActionSuccess.getId(), testActionFailed.getId(),
                "unregistered1", "unregistered2");
        @SafeModeController.SafeModeExecutionResult
        int success = SafeModeController.getInstance().executeActions(actions);
        Assert.assertEquals(testActionFailed.getId() + " should have been executed exactly 1 time",
                1, testActionFailed.getCallCount());
        Assert.assertEquals(testActionSuccess.getId() + " should have been executed exactly 1 time",
                1, testActionSuccess.getCallCount());
        Assert.assertEquals("Overall status should be failure if at least one"
                        + " action is unrecognized and at least one action is a failure",
                success, SafeModeController.SafeModeExecutionResult.ACTION_FAILED);
        Assert.assertEquals("Failed safemode actions should be logged", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.SafeMode.ExecutionResult",
                        SafeModeController.SafeModeExecutionResult.ACTION_FAILED));
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
    @SmallTest
    @Feature({"AndroidWebView"})
    public void testSafeModeAction_overallSuccessStatus() throws Throwable {
        TestSafeModeAction successAction1 = new TestSafeModeAction("successAction1");
        TestSafeModeAction successAction2 = new TestSafeModeAction("successAction2");
        Set<String> allSuccessful = asSet(successAction1.getId(), successAction2.getId());
        SafeModeController.getInstance().registerActions(
                new SafeModeAction[] {successAction1, successAction2});
        @SafeModeController.SafeModeExecutionResult
        int success = SafeModeController.getInstance().executeActions(allSuccessful);
        Assert.assertEquals("Overall status should be successful if all actions are successful",
                success, SafeModeController.SafeModeExecutionResult.SUCCESS);
        Assert.assertEquals("Overall status should be successful if all actions are successful", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.SafeMode.ExecutionResult",
                        SafeModeController.SafeModeExecutionResult.SUCCESS));
        Assert.assertEquals("successAction1 should have been executed exactly 1 time", 1,
                successAction1.getCallCount());
        Assert.assertEquals("successAction2 should have been executed exactly 1 time", 1,
                successAction2.getCallCount());

        // Register a new set of actions where at least one indicates failure.
        SafeModeController.getInstance().unregisterActionsForTesting();
        TestSafeModeAction failAction = new TestSafeModeAction("failAction", false);
        Set<String> oneFailure =
                asSet(successAction1.getId(), failAction.getId(), successAction2.getId());
        SafeModeController.getInstance().registerActions(
                new SafeModeAction[] {successAction1, failAction, successAction2});
        success = SafeModeController.getInstance().executeActions(oneFailure);
        Assert.assertEquals("Overall status should be failure if at least one action fails",
                success, SafeModeController.SafeModeExecutionResult.ACTION_FAILED);
        Assert.assertEquals("Overall status should be failure if at least one action fails", 1,
                RecordHistogram.getHistogramValueCountForTesting(
                        "Android.WebView.SafeMode.ExecutionResult",
                        SafeModeController.SafeModeExecutionResult.ACTION_FAILED));
        // One step failing should not block subsequent steps from executing.
        Assert.assertEquals(
                "successAction1 should have been executed again", 2, successAction1.getCallCount());
        Assert.assertEquals("failAction should have been executed exactly 1 time", 1,
                failAction.getCallCount());
        Assert.assertEquals(
                "successAction2 should have been executed again", 2, successAction2.getCallCount());
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
            boolean success = action.execute();
            Assert.assertTrue("VariationsSeedSafeModeAction should indicate success", success);
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
            boolean success = action.execute();
            Assert.assertTrue("VariationsSeedSafeModeAction should indicate success", success);

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
            boolean success = action.execute();
            Assert.assertTrue("VariationsSeedSafeModeAction should indicate success", success);
            Assert.assertFalse("Old seed should never have existed", oldFile.exists());
            Assert.assertFalse("New seed should never have existed", newFile.exists());
        } finally {
            VariationsTestUtils.deleteSeeds();
        }
    }

    @Test
    @MediumTest
    public void testTrustedPackage_invalidCert() throws Exception {
        TrustedPackage invalidPackage =
                new TrustedPackage(TEST_WEBVIEW_PACKAGE_NAME, FAKE_CERT_HASH, null);
        Assert.assertFalse("wrong certificate should not verify",
                invalidPackage.verify(TEST_WEBVIEW_PACKAGE_NAME));
    }

    private static class TestTrustedPackage extends TrustedPackage {
        private boolean mIsDebug = true;
        TestTrustedPackage(String packageName, byte[] release, byte[] debug) {
            super(packageName, release, debug);
        }
        @Override
        protected boolean isDebugAndroid() {
            return mIsDebug;
        }
        void setDebugBuildForTesting(boolean debug) {
            mIsDebug = debug;
        }
    }

    @Test
    @MediumTest
    public void testTrustedPackage_wrongPackageName() throws Exception {
        TrustedPackage webviewTestShell =
                new TestTrustedPackage(TEST_WEBVIEW_PACKAGE_NAME, TEST_WEBVIEW_CERT_HASH, null);
        Assert.assertFalse("Wrong pacakge name should not verify",
                webviewTestShell.verify("com.fake.package.name"));
    }

    @Test
    @MediumTest
    public void testTrustedPackage_eitherCertCanMatchOnDebugAndroid() throws Exception {
        TrustedPackage webviewTestShell =
                new TrustedPackage(TEST_WEBVIEW_PACKAGE_NAME, TEST_WEBVIEW_CERT_HASH, null);
        Assert.assertTrue("The WebView test shell should match itself",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));
        // Adding a non-matching certificate should not change anything (we should still trust
        // this).
        webviewTestShell = new TestTrustedPackage(
                TEST_WEBVIEW_PACKAGE_NAME, TEST_WEBVIEW_CERT_HASH, FAKE_CERT_HASH);
        Assert.assertTrue("The WebView test shell should match itself",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    public void testTrustedPackage_debugCertsOnlyTrustedOnDebugAndroid() throws Exception {
        // Use a fake release cert and real debug cert so this package can only be trusted on a
        // debug build.
        TestTrustedPackage webviewTestShell = new TestTrustedPackage(
                TEST_WEBVIEW_PACKAGE_NAME, FAKE_CERT_HASH, TEST_WEBVIEW_CERT_HASH);

        webviewTestShell.setDebugBuildForTesting(true);
        Assert.assertTrue("Debug cert should be trusted on debug Android build",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));

        webviewTestShell.setDebugBuildForTesting(false);
        Assert.assertFalse("Debug cert should not be trusted on release Android build",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));
    }

    @Test
    @MediumTest
    public void testTrustedPackage_verifyWebViewTestShell() throws Exception {
        TrustedPackage webviewTestShell =
                new TestTrustedPackage(TEST_WEBVIEW_PACKAGE_NAME, TEST_WEBVIEW_CERT_HASH, null);
        Assert.assertTrue("The WebView test shell should match itself",
                webviewTestShell.verify(TEST_WEBVIEW_PACKAGE_NAME));
    }
}
