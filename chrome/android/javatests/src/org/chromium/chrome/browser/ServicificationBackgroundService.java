// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import com.google.android.gms.gcm.TaskParams;

import org.junit.Assert;

import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.test.util.Criteria;
import org.chromium.content_public.browser.test.util.CriteriaHelper;
import org.chromium.content_public.browser.test.util.TestThreadUtils;

/**
 * Class for launching the service manager only mode for tests.
 */
public class ServicificationBackgroundService extends ChromeBackgroundService {
    private boolean mLaunchBrowserCalled;
    private boolean mNativeLoaded;
    private boolean mSupportsServiceManagerOnly;

    public ServicificationBackgroundService(boolean supportsServiceManagerOnly) {
        mSupportsServiceManagerOnly = supportsServiceManagerOnly;
    }

    @Override
    public int onRunTask(final TaskParams params) {
        mLaunchBrowserCalled = false;
        return super.onRunTask(params);
    }

    @Override
    protected void launchBrowser(Context context, String tag) {
        mLaunchBrowserCalled = true;
        mNativeLoaded = false;

        final BrowserParts parts = new EmptyBrowserParts() {
            @Override
            public void finishNativeInitialization() {
                mNativeLoaded = true;
            }

            @Override
            public boolean startServiceManagerOnly() {
                return mSupportsServiceManagerOnly;
            }
        };

        ChromeBrowserInitializer.getInstance().handlePreNativeStartup(parts);
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
    }

    // Posts an assertion task to the UI thread. Since this is only called after the call
    // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
    // can reliably check whether launchBrowser was called.
    protected void assertLaunchBrowserCalled() {
        TestThreadUtils.runOnUiThreadBlocking(() -> { Assert.assertTrue(mLaunchBrowserCalled); });
    }

    public void waitForNativeLoaded() {
        CriteriaHelper.pollUiThread(
                new Criteria("Failed while waiting for starting Service Manager.") {
                    @Override
                    public boolean isSatisfied() {
                        return mNativeLoaded;
                    }
                });
    }

    public void setSupportsServiceManagerOnly(boolean supportsServiceManagerOnly) {
        mSupportsServiceManagerOnly = supportsServiceManagerOnly;
    }

    public static void assertOnlyServiceManagerStarted() {
        // This task will always be queued and executed after
        // the BrowserStartupControllerImpl#browserStartupComplete() is called on the UI thread when
        // the full browser starts. So we can use it to checks whether the
        // {@link mFullBrowserStartupDone} has been set to true.
        TestThreadUtils.runOnUiThreadBlocking(() -> {
            Assert.assertTrue("Native has not been started.",
                    BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isNativeStarted());
            Assert.assertFalse("The full browser is started instead of ServiceManager only.",
                    BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                            .isFullBrowserStarted());
        });
    }

    public static void assertFullBrowserStarted() {
        // This task will always be queued and executed after
        // the BrowserStartupControllerImpl#browserStartupComplete() is called on the UI thread when
        // the full browser starts. So we can use it to checks whether the
        // {@link mFullBrowserStartupDone} has been set to true.
        TestThreadUtils.runOnUiThreadBlocking(
                ()
                        -> Assert.assertTrue("The full browser has not been started",
                                BrowserStartupController.get(LibraryProcessType.PROCESS_BROWSER)
                                        .isFullBrowserStarted()));
    }

    public void assertPersistentHistogramsOnDiskSystemProfile() {
        Assert.assertTrue(nativeTestPersistentHistogramsOnDiskSystemProfile());
    }

    private static native boolean nativeTestPersistentHistogramsOnDiskSystemProfile();
}
