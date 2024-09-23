// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser;

import android.content.Context;

import com.google.android.gms.gcm.TaskParams;

import org.jni_zero.NativeMethods;
import org.junit.Assert;

import org.chromium.base.ThreadUtils;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.chrome.browser.init.BrowserParts;
import org.chromium.chrome.browser.init.ChromeBrowserInitializer;
import org.chromium.chrome.browser.init.EmptyBrowserParts;
import org.chromium.content_public.browser.BrowserStartupController;

/** Class for launching the minimal browser mode for tests. */
public class ServicificationBackgroundService extends ChromeBackgroundServiceImpl {
    private boolean mLaunchBrowserCalled;
    private boolean mNativeLoaded;
    private boolean mSupportsMinimalBrowser;

    public ServicificationBackgroundService(boolean supportsMinimalBrowser) {
        mSupportsMinimalBrowser = supportsMinimalBrowser;
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

        final BrowserParts parts =
                new EmptyBrowserParts() {
                    @Override
                    public void finishNativeInitialization() {
                        mNativeLoaded = true;
                    }

                    @Override
                    public boolean startMinimalBrowser() {
                        return mSupportsMinimalBrowser;
                    }
                };

        ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(parts);
        ChromeBrowserInitializer.getInstance().handlePostNativeStartup(true, parts);
    }

    // Posts an assertion task to the UI thread. Since this is only called after the call
    // to onRunTask, it will be enqueued after any possible call to launchBrowser, and we
    // can reliably check whether launchBrowser was called.
    protected void assertLaunchBrowserCalled() {
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(mLaunchBrowserCalled);
                });
    }

    public void waitForNativeLoaded() {
        CriteriaHelper.pollUiThread(
                () -> mNativeLoaded, "Failed while waiting for starting minimal browser.");
    }

    public void setSupportsMinimalBrowser(boolean supportsMinimalBrowser) {
        mSupportsMinimalBrowser = supportsMinimalBrowser;
    }

    public static void assertMinimalBrowserStarted() {
        // This task will always be queued and executed after
        // the BrowserStartupControllerImpl#browserStartupComplete() is called on the UI thread when
        // the full browser starts. So we can use it to checks whether the
        // {@link mFullBrowserStartupDone} has been set to true.
        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    Assert.assertTrue(
                            "Native has not been started.",
                            BrowserStartupController.getInstance().isNativeStarted());
                    Assert.assertFalse(
                            "The full browser is started instead of a minimal browser.",
                            BrowserStartupController.getInstance().isFullBrowserStarted());
                });
    }

    public static void assertFullBrowserStarted() {
        // This task will always be queued and executed after
        // the BrowserStartupControllerImpl#browserStartupComplete() is called on the UI thread when
        // the full browser starts. So we can use it to checks whether the
        // {@link mFullBrowserStartupDone} has been set to true.
        ThreadUtils.runOnUiThreadBlocking(
                () ->
                        Assert.assertTrue(
                                "The full browser has not been started",
                                BrowserStartupController.getInstance().isFullBrowserStarted()));
    }

    public void assertPersistentHistogramsOnDiskSystemProfile() {
        Assert.assertTrue(
                ServicificationBackgroundServiceJni.get()
                        .testPersistentHistogramsOnDiskSystemProfile());
    }

    public void assertBackgroundSessionStart() {
        Assert.assertTrue(ServicificationBackgroundServiceJni.get().isBackgroundSessionStart());
    }

    @NativeMethods
    interface Natives {
        boolean testPersistentHistogramsOnDiskSystemProfile();

        boolean isBackgroundSessionStart();
    }
}
