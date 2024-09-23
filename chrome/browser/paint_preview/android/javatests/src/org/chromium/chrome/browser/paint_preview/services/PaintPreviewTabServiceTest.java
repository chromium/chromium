// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import android.app.Activity;

import androidx.test.filters.MediumTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabClosureParams;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.concurrent.TimeUnit;

/** Tests for the Paint Preview Tab Manager. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class PaintPreviewTabServiceTest {
    private static final long TIMEOUT_MS = 5000;
    private static final long POLLING_INTERVAL_MS = 500;

    @Rule
    public final ChromeTabbedActivityTestRule mActivityTestRule =
            new ChromeTabbedActivityTestRule();

    @Rule public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private TabModelSelector mTabModelSelector;
    private TabModel mTabModel;
    private Tab mTab;
    private PaintPreviewTabService mPaintPreviewTabService;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        mTabModel = mTabModelSelector.getModel(/* incognito= */ false);
    }

    /** Verifies that a Tab's contents are captured when the activity is stopped. */
    @Test
    @MediumTest
    @Feature({"PaintPreview"})
    public void testCapturedAndDelete() throws Exception {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
                    mTab.loadUrl(new LoadUrlParams(url));
                });
        // Give the tab time to complete layout before hiding.
        TimeUnit.SECONDS.sleep(1);
        int tabId = mTab.getId();

        // Simulate closing the app.
        Activity activity = mActivityTestRule.getActivity();
        activity.getWindow().setLocalFocus(false, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstrumentationRegistry.getInstrumentation().callActivityOnPause(activity);
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstrumentationRegistry.getInstrumentation().callActivityOnStop(activity);
                });

        // Allow time to capture.
        CriteriaHelper.pollUiThread(
                () -> {
                    mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
                    return mPaintPreviewTabService.hasCaptureForTab(tabId);
                },
                "Paint Preview didn't get captured.",
                TIMEOUT_MS,
                POLLING_INTERVAL_MS);

        // Simulate unpausing the app (for cleanup).
        activity.getWindow().setLocalFocus(true, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstrumentationRegistry.getInstrumentation().callActivityOnRestart(activity);
                    InstrumentationRegistry.getInstrumentation().callActivityOnStart(activity);
                    InstrumentationRegistry.getInstrumentation().callActivityOnResume(activity);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
                    mTab = mTabModelSelector.getTabById(tabId);
                    mTabModel = mTabModelSelector.getModel(/* incognito= */ false);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mTabModel.closeTabs(TabClosureParams.closeTab(mTab).allowUndo(false).build());
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mPaintPreviewTabService.hasCaptureForTab(tabId);
                },
                "Paint Preview didn't get deleted.",
                TIMEOUT_MS,
                POLLING_INTERVAL_MS);
    }

    /** Tests that capturing and deleting via an audit works as expected. */
    @Test
    @MediumTest
    @Feature({"PaintPreview"})
    public void testCapturedAndDeleteViaAudit() throws Exception {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
                    mTab.loadUrl(new LoadUrlParams(url));
                });
        // Give the tab time to complete layout before hiding.
        TimeUnit.SECONDS.sleep(1);
        int tabId = mTab.getId();

        // Simulate closing the app.
        Activity activity = mActivityTestRule.getActivity();
        activity.getWindow().setLocalFocus(false, false);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstrumentationRegistry.getInstrumentation().callActivityOnPause(activity);
                });
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstrumentationRegistry.getInstrumentation().callActivityOnStop(activity);
                });

        // Allow time to capture.
        CriteriaHelper.pollUiThread(
                () -> {
                    mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
                    return mPaintPreviewTabService.hasCaptureForTab(tabId);
                },
                "Paint Preview didn't get captured.",
                TIMEOUT_MS,
                POLLING_INTERVAL_MS);

        // Simulate unpausing the app (for cleanup).
        activity.getWindow().setLocalFocus(true, true);
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    InstrumentationRegistry.getInstrumentation().callActivityOnRestart(activity);
                    InstrumentationRegistry.getInstrumentation().callActivityOnStart(activity);
                    InstrumentationRegistry.getInstrumentation().callActivityOnResume(activity);
                });

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaintPreviewTabService.auditArtifacts(new int[0]);
                });

        CriteriaHelper.pollUiThread(
                () -> {
                    return !mPaintPreviewTabService.hasCaptureForTab(tabId);
                },
                "Paint Preview didn't get deleted.",
                TIMEOUT_MS,
                POLLING_INTERVAL_MS);
    }

    /** Verifies the pre-native preview exists check works. */
    @Test
    @MediumTest
    @Feature({"PaintPreview"})
    public void testPreNativePreviewExists() throws Exception {
        mTemporaryFolder.newFile("2.zip");
        mTemporaryFolder.newFile("3.zip");
        mTemporaryFolder.newFile("6");
        mTemporaryFolder.newFolder("10");

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
                    Assert.assertTrue(
                            mPaintPreviewTabService.previewExistsPreNative(
                                    mTemporaryFolder.getRoot().getPath(), 2));
                    Assert.assertTrue(
                            mPaintPreviewTabService.previewExistsPreNative(
                                    mTemporaryFolder.getRoot().getPath(), 3));
                    Assert.assertFalse(
                            mPaintPreviewTabService.previewExistsPreNative(
                                    mTemporaryFolder.getRoot().getPath(), 6));
                    Assert.assertFalse(
                            mPaintPreviewTabService.previewExistsPreNative(
                                    mTemporaryFolder.getRoot().getPath(), 10));
                });
    }
}
