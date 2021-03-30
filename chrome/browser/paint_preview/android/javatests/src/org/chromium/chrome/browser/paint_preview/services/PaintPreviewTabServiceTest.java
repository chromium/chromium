// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.paint_preview.services;

import android.app.Activity;
import android.support.test.InstrumentationRegistry;

import androidx.test.filters.MediumTest;

import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.rules.TemporaryFolder;
import org.junit.runner.RunWith;

import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.base.test.util.Feature;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModel;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.content_public.browser.LoadUrlParams;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.net.test.EmbeddedTestServer;

import java.util.HashSet;
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

    @Rule
    public TemporaryFolder mTemporaryFolder = new TemporaryFolder();

    private TabModelSelector mTabModelSelector;
    private TabModel mTabModel;
    private Tab mTab;
    private PaintPreviewTabService mPaintPreviewTabService;

    @Before
    public void setUp() throws Exception {
        mActivityTestRule.startMainActivityOnBlankPage();
        mTab = mActivityTestRule.getActivity().getActivityTab();
        mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
        mTabModel = mTabModelSelector.getModel(/*incognito*/ false);
    }

    /**
     * Verifies that a Tab's contents are captured when the activity is stopped.
     */
    @Test
    @MediumTest
    @Feature({"PaintPreview"})
    public void testCapturedAndDelete() throws Exception {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
            mPaintPreviewTabService.onRestoreCompleted(mTabModelSelector, true);
            mTab.loadUrl(new LoadUrlParams(url));
        });
        // Give the tab time to complete layout before hiding.
        TimeUnit.SECONDS.sleep(1);
        int tabId = mTab.getId();

        // Simulate closing the app.
        Activity activity = mActivityTestRule.getActivity();
        activity.getWindow().setLocalFocus(false, false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstrumentationRegistry.getInstrumentation().callActivityOnPause(activity);
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstrumentationRegistry.getInstrumentation().callActivityOnStop(activity);
        });

        // Allow time to capture.
        CriteriaHelper.pollUiThread(() -> {
            mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
            return mPaintPreviewTabService.hasCaptureForTab(tabId);
        }, "Paint Preview didn't get captured.", TIMEOUT_MS, POLLING_INTERVAL_MS);

        // Simulate unpausing the app (for cleanup).
        activity.getWindow().setLocalFocus(true, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstrumentationRegistry.getInstrumentation().callActivityOnRestart(activity);
            InstrumentationRegistry.getInstrumentation().callActivityOnStart(activity);
            InstrumentationRegistry.getInstrumentation().callActivityOnResume(activity);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mTabModelSelector = mActivityTestRule.getActivity().getTabModelSelector();
            mTab = mTabModelSelector.getTabById(tabId);
            mTabModel = mTabModelSelector.getModel(/*incognito*/ false);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> { mTabModel.closeTab(mTab); });

        CriteriaHelper.pollUiThread(() -> {
            return !mPaintPreviewTabService.hasCaptureForTab(tabId);
        }, "Paint Preview didn't get deleted.", TIMEOUT_MS, POLLING_INTERVAL_MS);
    }

    /**
     * Tests that capturing and deleting via an audit works as expected.
     */
    @Test
    @MediumTest
    @Feature({"PaintPreview"})
    public void testCapturedAndDeleteViaAudit() throws Exception {
        EmbeddedTestServer testServer = mActivityTestRule.getTestServer();
        final String url = testServer.getURL("/chrome/test/data/android/about.html");

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
            mPaintPreviewTabService.onRestoreCompleted(mTabModelSelector, true);
            mTab.loadUrl(new LoadUrlParams(url));
        });
        // Give the tab time to complete layout before hiding.
        TimeUnit.SECONDS.sleep(1);
        int tabId = mTab.getId();

        // Simulate closing the app.
        Activity activity = mActivityTestRule.getActivity();
        activity.getWindow().setLocalFocus(false, false);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstrumentationRegistry.getInstrumentation().callActivityOnPause(activity);
        });
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstrumentationRegistry.getInstrumentation().callActivityOnStop(activity);
        });

        // Allow time to capture.
        CriteriaHelper.pollUiThread(() -> {
            mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
            return mPaintPreviewTabService.hasCaptureForTab(tabId);
        }, "Paint Preview didn't get captured.", TIMEOUT_MS, POLLING_INTERVAL_MS);

        // Simulate unpausing the app (for cleanup).
        activity.getWindow().setLocalFocus(true, true);
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            InstrumentationRegistry.getInstrumentation().callActivityOnRestart(activity);
            InstrumentationRegistry.getInstrumentation().callActivityOnStart(activity);
            InstrumentationRegistry.getInstrumentation().callActivityOnResume(activity);
        });

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Use the incognito tab model as the normal tab model will still have the tab ids
            // active.
            mPaintPreviewTabService.auditOnStart(mTabModelSelector.getModel(/*incognito=*/true));
        });

        CriteriaHelper.pollUiThread(() -> {
            return !mPaintPreviewTabService.hasCaptureForTab(tabId);
        }, "Paint Preview didn't get deleted.", TIMEOUT_MS, POLLING_INTERVAL_MS);
    }

    /**
     * Verifies the early cache is created correctly.
     */
    @Test
    @MediumTest
    @Feature({"PaintPreview"})
    public void testEarlyCache() throws Exception {
        mTemporaryFolder.newFolder("1");
        mTemporaryFolder.newFile("2.zip");
        mTemporaryFolder.newFile("6.zip");
        mTemporaryFolder.newFolder("10");

        HashSet<Integer> expectedFiles = new HashSet<>();
        expectedFiles.add(1);
        expectedFiles.add(2);
        expectedFiles.add(6);
        expectedFiles.add(10);

        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mPaintPreviewTabService = PaintPreviewTabServiceFactory.getServiceInstance();
            mPaintPreviewTabService.createPreNativeCache(mTemporaryFolder.getRoot().getPath());
        });

        Assert.assertEquals(expectedFiles, mPaintPreviewTabService.mPreNativeCache);
    }
}
