// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.webapk.lib.client;

import android.content.ComponentName;
import android.os.Bundle;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.RobolectricTestRunner;
import org.robolectric.RuntimeEnvironment;
import org.robolectric.Shadows;
import org.robolectric.android.util.concurrent.RoboExecutorService;
import org.robolectric.annotation.Config;
import org.robolectric.annotation.LooperMode;
import org.robolectric.shadows.ShadowApplication;
import org.robolectric.shadows.ShadowLooper;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.components.webapk.lib.common.WebApkMetaDataKeys;
import org.chromium.webapk.lib.common.identity_service.IIdentityService;
import org.chromium.webapk.test.WebApkTestHelper;

/** Unit tests for {@link org.chromium.webapk.lib.client.WebApkIdentityServiceClient}. */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
@LooperMode(LooperMode.Mode.LEGACY)
public class WebApkIdentityServiceClientTest {
    static final String BROWSER_PACKAGE_NAME = "org.chromium.test";

    private static final String WEBAPK_PACKAGE_NAME = "org.chromium.webapk.test_package";
    private static final String ANOTHER_BROWSER_PACKAGE_NAME = "another.browser";
    private ShadowApplication mShadowApplication;

    /** Mocks the {@link WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback}. */
    private static class TestCheckBacksWebApkCallback
            implements WebApkIdentityServiceClient.CheckBrowserBacksWebApkCallback {
        private boolean mResult;
        private boolean mIsCalled;

        @Override
        public void onChecked(boolean doesBrowserBackWebApk, String browserPackageName) {
            mResult = doesBrowserBackWebApk;
            mIsCalled = true;
        }
    }

    /** Mocks the Identity service of a WebAPK. */
    private static class TestIdentityService extends IIdentityService.Stub {
        private String mRuntimeHost;

        public TestIdentityService(String runtimeHost) {
            mRuntimeHost = runtimeHost;
        }

        @Override
        public String getRuntimeHostBrowserPackageName() {
            return mRuntimeHost;
        }
    }

    @Before
    public void setUp() {
        mShadowApplication = Shadows.shadowOf(RuntimeEnvironment.application);
        PostTask.setPrenativeThreadPoolExecutorForTesting(new RoboExecutorService());
    }

    @After
    public void tearDown() {
        WebApkIdentityServiceClient.disconnectAll(RuntimeEnvironment.application);
    }

    /**
     * Tests that for WebAPKs with shell APK version lower than the {@link
     * WebApkIdentityServiceClient#SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST}, the
     * backs-WebAPK-check returns false if the browser does NOT match the WebAPK's runtime host
     * specified in the metaData.
     */
    @Test
    public void testReturnsFalseWhenNotMatchRuntimeHostBeforeIntroduceHostBrowserSwitchLogic() {
        registerWebApk(
                /* runtimeHost= */ ANOTHER_BROWSER_PACKAGE_NAME,
                WebApkIdentityServiceClient.SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST - 1
                /* shellApkVersion= */ );
        mShadowApplication.declareActionUnbindable(
                WebApkIdentityServiceClient.ACTION_WEBAPK_IDENTITY_SERVICE);

        Assert.assertFalse(doesBrowserBackWebApk());
    }

    /**
     * Tests that for WebAPKs with shell APK version lower than the {@link
     * WebApkIdentityServiceClient#SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST}, the
     * backs-WebAPK-check returns true if the browser matches the WebAPK's runtime host specified in
     * the metaData.
     */
    @Test
    public void testReturnsTrueWhenMatchesRuntimeHostBeforeIntroduceHostBrowserSwitchLogic() {
        registerWebApk(
                /* runtimeHost= */ BROWSER_PACKAGE_NAME,
                WebApkIdentityServiceClient.SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST - 1
                /* shellApkVersion= */ );
        mShadowApplication.declareActionUnbindable(
                WebApkIdentityServiceClient.ACTION_WEBAPK_IDENTITY_SERVICE);

        Assert.assertTrue(doesBrowserBackWebApk());
    }

    /**
     * Tests that for WebAPKs with shell APK version equal or higher than the {@link
     * WebApkIdentityServiceClient#SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST} but doesn't
     * have Identity Service, the backs-WebAPK-check returns false.
     */
    @Test
    public void testBacksWebApkCheckForWebApkWithHostBrowserSwitchLogicButWithoutIdentityService() {
        registerWebApk(
                /* runtimeHost= */ BROWSER_PACKAGE_NAME,
                WebApkIdentityServiceClient.SHELL_APK_VERSION_SUPPORTING_SWITCH_RUNTIME_HOST
                /* shellApkVersion= */ );
        mShadowApplication.declareActionUnbindable(
                WebApkIdentityServiceClient.ACTION_WEBAPK_IDENTITY_SERVICE);

        Assert.assertFalse(doesBrowserBackWebApk());
    }

    /**
     * Tests that for WebAPKs with Identity service, the backs-WebAPK-check returns false if the
     * package name of the browser does NOT match the one provided by the Identity service.
     */
    @Test
    public void testReturnsFalseWhenDoesNotMatchRuntimeHostProvidedByIdentityService() {
        // The shell APK version doesn't matter as long as the WebAPK has an Identity service.
        registerWebApk(/* runtimeHost= */ BROWSER_PACKAGE_NAME, /* shellApkVersion= */ 0);
        mShadowApplication.setComponentNameAndServiceForBindService(
                new ComponentName(WEBAPK_PACKAGE_NAME, ""),
                new TestIdentityService(ANOTHER_BROWSER_PACKAGE_NAME));

        Assert.assertFalse(doesBrowserBackWebApk());
    }

    /**
     * Tests that for WebAPKs with Identity service, the backs-WebAPK-check returns true if the
     * package name of the browser matches the one provided by the Identity service.
     */
    @Test
    public void testReturnsTrueWhenMatchesRuntimeHostProvidedByIdentityService() {
        // The shell APK version doesn't matter as long as the WebAPK has an Identity service.
        registerWebApk(/* runtimeHost= */ ANOTHER_BROWSER_PACKAGE_NAME, /* shellApkVersion= */ 0);
        mShadowApplication.setComponentNameAndServiceForBindService(
                new ComponentName(WEBAPK_PACKAGE_NAME, ""),
                new TestIdentityService(/* runtimeHost= */ BROWSER_PACKAGE_NAME));

        Assert.assertTrue(doesBrowserBackWebApk());
    }

    /** Registers a WebAPK with the runtime host and the shell APK version in its metadata. */
    private void registerWebApk(String runtimeHost, int shellApkVersion) {
        Bundle bundle = new Bundle();
        bundle.putString(WebApkMetaDataKeys.RUNTIME_HOST, runtimeHost);
        bundle.putInt(WebApkMetaDataKeys.SHELL_APK_VERSION, shellApkVersion);
        WebApkTestHelper.registerWebApkWithMetaData(
                WEBAPK_PACKAGE_NAME, bundle, /* shareTargetMetaData= */ null);
    }

    /** Checks whether the browser backs the WebAPK. */
    private boolean doesBrowserBackWebApk() {
        TestCheckBacksWebApkCallback callback = new TestCheckBacksWebApkCallback();
        WebApkIdentityServiceClient.getInstance(TaskTraits.BEST_EFFORT_MAY_BLOCK)
                .checkBrowserBacksWebApkAsync(
                        RuntimeEnvironment.application, WEBAPK_PACKAGE_NAME, callback);
        ShadowLooper.runUiThreadTasksIncludingDelayedTasks();

        Assert.assertTrue(callback.mIsCalled);
        return callback.mResult;
    }
}
