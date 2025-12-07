// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import android.os.StrictMode;

import androidx.test.filters.LargeTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.junit.runners.Parameterized;
import org.junit.runners.Parameterized.UseParametersRunnerFactory;

import org.chromium.base.test.util.Feature;

/** Tests ensuring that starting up WebView does not cause any diskRead StrictMode violations. */
@RunWith(Parameterized.class)
@UseParametersRunnerFactory(AwJUnit4ClassRunnerWithParameters.Factory.class)
public class AwStrictModeTest extends AwParameterizedTest {
    @Rule public AwActivityTestRule mActivityTestRule;

    public AwStrictModeTest(AwSettingsMutation param) {
        mActivityTestRule =
                new AwActivityTestRule(param.getMutation()) {
                    @Override
                    public boolean needsNativeInitialized() {
                        // We don't want native to be initialized by default so we can catch the
                        // strict mode violations as failures in tests.
                        return false;
                    }

                    @Override
                    public boolean needsBrowserProcessStarted() {
                        // Don't start the browser process in AwActivityTestRule - we want to start
                        // it ourselves with strictmode policies turned on.
                        return false;
                    }
                };
    }

    private TestAwContentsClient mContentsClient;
    private AwTestContainerView mAwTestContainerView;

    private StrictMode.ThreadPolicy mOldThreadPolicy;
    private StrictMode.VmPolicy mOldVmPolicy;

    @Before
    public void setUp() {
        mContentsClient = new TestAwContentsClient();
        enableStrictModeOnUiThreadSync();
    }

    @After
    public void tearDown() {
        disableStrictModeOnUiThreadSync();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testStartup() {
        startEverythingSync();
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testLoadEmptyData() throws Exception {
        startEverythingSync();
        mActivityTestRule.loadDataSync(
                mAwTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                "",
                "text/html",
                false);
    }

    @Test
    @LargeTest
    @Feature({"AndroidWebView"})
    public void testSetJavaScriptAndLoadData() throws Exception {
        startEverythingSync();
        AwActivityTestRule.enableJavaScriptOnUiThread(mAwTestContainerView.getAwContents());
        mActivityTestRule.loadDataSync(
                mAwTestContainerView.getAwContents(),
                mContentsClient.getOnPageFinishedHelper(),
                "",
                "text/html",
                false);
    }

    private void enableStrictModeOnUiThreadSync() {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            mOldThreadPolicy = StrictMode.getThreadPolicy();
                            mOldVmPolicy = StrictMode.getVmPolicy();
                            StrictMode.setThreadPolicy(
                                    new StrictMode.ThreadPolicy.Builder()
                                            .detectAll()
                                            .penaltyLog()
                                            .penaltyDeath()
                                            .build());
                            StrictMode.setVmPolicy(
                                    new StrictMode.VmPolicy.Builder()
                                            .detectAll()
                                            .penaltyLog()
                                            .penaltyDeath()
                                            .build());
                        });
    }

    private void disableStrictModeOnUiThreadSync() {
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () -> {
                            StrictMode.setThreadPolicy(mOldThreadPolicy);
                            StrictMode.setVmPolicy(mOldVmPolicy);
                        });
    }

    private void startEverythingSync() {
        mActivityTestRule.getActivity();
        mActivityTestRule.startBrowserProcess();
        InstrumentationRegistry.getInstrumentation()
                .runOnMainSync(
                        () ->
                                mAwTestContainerView =
                                        mActivityTestRule.createAwTestContainerView(
                                                mContentsClient));
    }
}
