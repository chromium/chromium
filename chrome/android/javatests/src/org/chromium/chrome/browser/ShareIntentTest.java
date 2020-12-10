// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.app.Activity;
import android.content.ComponentName;
import android.content.Context;
import android.content.Intent;
import android.content.pm.PackageManager;
import android.content.res.Resources;
import android.net.Uri;
import android.os.ParcelFileDescriptor;
import android.view.Window;

import androidx.test.filters.LargeTest;

import org.junit.After;
import org.junit.Before;
import org.junit.Rule;
import org.junit.Test;
import org.junit.runner.RunWith;

import org.chromium.base.supplier.ObservableSupplier;
import org.chromium.base.supplier.OneshotSupplierImpl;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CommandLineFlags;
import org.chromium.chrome.browser.app.ChromeActivity;
import org.chromium.chrome.browser.flags.ChromeSwitches;
import org.chromium.chrome.browser.share.ShareDelegate;
import org.chromium.chrome.browser.share.ShareDelegateImpl;
import org.chromium.chrome.browser.share.ShareHelper;
import org.chromium.chrome.browser.tab.Tab;
import org.chromium.chrome.browser.tabmodel.TabModelSelector;
import org.chromium.chrome.browser.ui.RootUiCoordinator;
import org.chromium.chrome.browser.util.ChromeFileProvider;
import org.chromium.chrome.test.ChromeJUnit4ClassRunner;
import org.chromium.chrome.test.ChromeTabbedActivityTestRule;
import org.chromium.chrome.test.util.browser.tabmodel.MockTabModelSelector;
import org.chromium.content_public.browser.test.util.TestThreadUtils;
import org.chromium.ui.base.WindowAndroid;

import java.io.IOException;
import java.lang.ref.WeakReference;
import java.util.concurrent.ExecutionException;

/**
 * Instrumentation tests for Share intents.
 */
@RunWith(ChromeJUnit4ClassRunner.class)
@CommandLineFlags.Add({ChromeSwitches.DISABLE_FIRST_RUN_EXPERIENCE})
public class ShareIntentTest {
    @Rule
    public ChromeTabbedActivityTestRule mActivityTestRule = new ChromeTabbedActivityTestRule();

    private static final String TAG = "ShareIntentTest";

    /**
     * Mock activity class that overrides the startActivity and checks if the file passed in the
     * intent can be opened.
     *
     * This class is a wrapper around the actual activity of the test, while it also inherits from
     * activity and redirects the calls to the methods to the actual activity.
     */
    private static class MockChromeActivity extends ChromeTabbedActivity {
        private final Object mLock = new Object();
        private boolean mCheckCompleted;
        private ChromeActivity mActivity;

        public MockChromeActivity(ChromeActivity activity) {
            mActivity = activity;
            mCheckCompleted = false;
        }

        /**
         * Overrides startActivity and notifies check completed when the file from the uri of the
         * intent is opened.
         */
        @Override
        public void startActivity(Intent intent) {
            processStartActivityIntent(intent);
        }

        @Override
        public void startActivityForResult(Intent intent, int requestCode) {
            processStartActivityIntent(intent);
        }

        private void processStartActivityIntent(Intent intent) {
            final Uri uri = intent.getClipData().getItemAt(0).getUri();
            PostTask.postTask(TaskTraits.BEST_EFFORT_MAY_BLOCK, () -> {
                ChromeFileProvider provider = new ChromeFileProvider();
                ParcelFileDescriptor file = null;
                try {
                    file = provider.openFile(uri, "r");
                    if (file != null) file.close();
                } catch (IOException e) {
                    assert false : "Error while opening the file";
                }
                synchronized (mLock) {
                    mCheckCompleted = true;
                    mLock.notify();
                }
            });
        }

        /**
         * Waits till the check for file opening is completed.
         */
        public void waitForFileCheck() throws InterruptedException {
            synchronized (mLock) {
                while (!mCheckCompleted) {
                    mLock.wait();
                }
            }
        }

        @Override
        public String getPackageName() {
            return mActivity.getPackageName();
        }

        @Override
        public Tab getActivityTab() {
            return mActivity.getActivityTab();
        }

        @Override
        public PackageManager getPackageManager() {
            return mActivity.getPackageManager();
        }

        @Override
        public Window getWindow() {
            return mActivity.getWindow();
        }

        @Override
        public ActivityTabProvider getActivityTabProvider() {
            return mActivity.getActivityTabProvider();
        }

        @Override
        public TabModelSelector getTabModelSelector() {
            // TabModelSelector remains uninitialized for this test. Return a mock instead.
            return new MockTabModelSelector(1, 0, null);
        }

        @Override
        public ObservableSupplier<ShareDelegate> getShareDelegateSupplier() {
            return mActivity.getShareDelegateSupplier();
        }

        @Override
        public Object getSystemService(String name) {
            // Prevents a scenario where InputMethodManager#hideSoftInput()
            // gets called before Activity#onCreate() gets called in this test.
            return name.equals(Context.INPUT_SERVICE) ? null : mActivity.getSystemService(name);
        }

        @Override
        public String getSystemServiceName(Class<?> serviceClass) {
            return mActivity.getSystemServiceName(serviceClass);
        }

        @Override
        public Resources getResources() {
            return mActivity.getResources();
        }

        @Override
        public Resources.Theme getTheme() {
            return mActivity.getTheme();
        }
    }

    @Test
    @LargeTest
    public void testShareIntent() throws ExecutionException, InterruptedException {
        MockChromeActivity mockActivity = TestThreadUtils.runOnUiThreadBlocking(() -> {
            // Sets a test component as last shared and "shareDirectly" option is set so that
            // the share selector menu is not opened. The start activity is overridden, so the
            // package and class names do not matter.
            return new MockChromeActivity(mActivityTestRule.getActivity());
        });
        RootUiCoordinator rootUiCoordinator = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return new RootUiCoordinator(mockActivity, null,
                    mockActivity.getShareDelegateSupplier(), mockActivity.getActivityTabProvider(),
                    null, null, null, null, new OneshotSupplierImpl<>(),
                    new OneshotSupplierImpl<>(), new OneshotSupplierImpl<>(), () -> null);
        });
        ShareHelper.setLastShareComponentName(new ComponentName("test.package", "test.activity"));
        // Skips the capture of screenshot and notifies with an empty file.
        ShareDelegateImpl.setScreenshotCaptureSkippedForTesting(true);

        WindowAndroid window = TestThreadUtils.runOnUiThreadBlocking(() -> {
            return new WindowAndroid(mActivityTestRule.getActivity()) {
                @Override
                public WeakReference<Activity> getActivity() {
                    return new WeakReference<>(mockActivity);
                }
            };
        });
        TestThreadUtils.runOnUiThreadBlocking(
                () -> mockActivity.getActivityTab().updateAttachment(window, null));

        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> rootUiCoordinator.onShareMenuItemSelected(
                                true /* shareDirectly */, false /* isIncognito */));

        mockActivity.waitForFileCheck();

        ShareHelper.setLastShareComponentName(new ComponentName("", ""));
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            mockActivity.getActivityTab().updateAttachment(null, null);
            window.destroy();
        });
    }

    @Before
    public void setUp() throws InterruptedException {
        mActivityTestRule.startMainActivityOnBlankPage();
    }

    @After
    public void tearDown() {
        ShareDelegateImpl.setScreenshotCaptureSkippedForTesting(false);
    }
}
