// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import static org.chromium.base.test.util.Restriction.RESTRICTION_TYPE_NON_LOW_END_DEVICE;

import android.content.Context;
import android.view.View;

import androidx.test.core.app.ApplicationProvider;
import androidx.test.filters.MediumTest;
import androidx.test.filters.SmallTest;
import androidx.test.platform.app.InstrumentationRegistry;

import org.junit.After;
import org.junit.Assert;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.BaseSwitches;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.base.test.util.DisabledTest;
import org.chromium.base.test.util.Restriction;
import org.chromium.chrome.browser.download.DownloadTestRule;
import org.chromium.chrome.browser.download.DownloadTestRule.CustomMainActivityStart;
import org.chromium.chrome.browser.firstrun.FirstRunStatus;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.R;
import org.chromium.chrome.test.util.browser.contextmenu.ContextMenuUtils;
import org.chromium.net.test.EmbeddedTestServer;
import org.chromium.ui.widget.Toast;
import org.chromium.ui.widget.ToastManager;

import java.util.concurrent.atomic.AtomicBoolean;

/** Tests that toasts don't trigger HW acceleration. */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ToastHWATest implements CustomMainActivityStart {
    @Rule public DownloadTestRule mDownloadTestRule = new DownloadTestRule(this);

    private EmbeddedTestServer mTestServer;

    private static final String URL_PATH = "/chrome/test/data/android/google.html";
    private static final String IMAGE_NAME = "google.png";
    private static final String IMAGE_ID = "logo";
    private static final String LINK_ID = "aboutLink";

    private static final String[] TEST_FILES = {IMAGE_NAME};

    @Before
    public void setUp() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(true));

        mDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
        mTestServer =
                EmbeddedTestServer.createAndStartServer(
                        ApplicationProvider.getApplicationContext());
    }

    @After
    public void tearDown() {
        ThreadUtils.runOnUiThreadBlocking(() -> FirstRunStatus.setFirstRunFlowComplete(false));

        mDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
        ToastManager.resetForTesting();
    }

    @Override
    public void customMainActivityStart() throws InterruptedException {
        mDownloadTestRule.startMainActivityOnBlankPage();
        mDownloadTestRule.deleteFilesInDownloadDirectory(TEST_FILES);
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @DisabledTest(message = "crbug.com/668217")
    public void testNoRenderThread() {
        Utils.assertNoRenderThread();
    }

    @Test
    @MediumTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @DisabledTest(message = "crbug.com/668217")
    public void testDownloadingToast() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(URL_PATH));
        mDownloadTestRule.assertWaitForPageScaleFactorMatch(0.5f);

        int callCount = mDownloadTestRule.getChromeDownloadCallCount();

        // Download an image (shows 'Downloading...' toast)
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(),
                tab,
                IMAGE_ID,
                R.id.contextmenu_save_image);

        // Wait for UI activity to settle
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        // Wait for download to finish
        Assert.assertTrue(mDownloadTestRule.waitForChromeDownloadToFinish(callCount));

        Utils.assertNoRenderThread();
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @DisabledTest(message = "crbug.com/668217")
    public void testOpenedInBackgroundToast() throws Exception {
        mDownloadTestRule.loadUrl(mTestServer.getURL(URL_PATH));
        mDownloadTestRule.assertWaitForPageScaleFactorMatch(0.5f);

        // Open link in a new tab (shows 'Tab Opened In Background' toast)
        Tab tab = mDownloadTestRule.getActivity().getActivityTab();
        ContextMenuUtils.selectContextMenuItem(
                InstrumentationRegistry.getInstrumentation(),
                mDownloadTestRule.getActivity(),
                tab,
                LINK_ID,
                R.id.contextmenu_open_in_new_tab);

        // Wait for UI activity to settle
        InstrumentationRegistry.getInstrumentation().waitForIdleSync();

        Utils.assertNoRenderThread();
    }

    @Test
    @SmallTest
    @CommandLineFlags.Add(BaseSwitches.ENABLE_LOW_END_DEVICE_MODE)
    @DisabledTest(message = "crbug.com/668217")
    public void testToastNoAcceleration() throws Exception {
        // Toasts created on low-end devices shouldn't be HW accelerated.
        Assert.assertFalse(isToastAcceleratedWithContext(mDownloadTestRule.getActivity()));
        Assert.assertFalse(
                isToastAcceleratedWithContext(
                        mDownloadTestRule.getActivity().getApplicationContext()));
    }

    @Test
    @SmallTest
    @Restriction(RESTRICTION_TYPE_NON_LOW_END_DEVICE)
    public void testToastAcceleration() throws Exception {
        // Toasts created on high-end devices should be HW accelerated.
        Assert.assertTrue(isToastAcceleratedWithContext(mDownloadTestRule.getActivity()));
        Assert.assertTrue(
                isToastAcceleratedWithContext(
                        mDownloadTestRule.getActivity().getApplicationContext()));
    }

    private static boolean isToastAcceleratedWithContext(final Context context) throws Exception {
        final AtomicBoolean accelerated = new AtomicBoolean();
        final CallbackHelper listenerCalled = new CallbackHelper();

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        // We are using Toast.makeText(context, ...) instead of new Toast(context)
                        // because that Toast constructor is unused and is deleted by proguard.
                        Toast toast = Toast.makeText(context, "Test", Toast.LENGTH_SHORT);
                        toast.setView(
                                new View(context) {
                                    @Override
                                    public void onAttachedToWindow() {
                                        super.onAttachedToWindow();
                                        accelerated.set(isHardwareAccelerated());
                                        listenerCalled.notifyCalled();
                                    }
                                });
                        toast.show();
                    }
                });

        listenerCalled.waitForCallback(0);
        ToastManager.resetForTesting();
        return accelerated.get();
    }
}
