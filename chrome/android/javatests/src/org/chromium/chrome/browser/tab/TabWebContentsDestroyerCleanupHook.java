// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.tab;

import org.hamcrest.Matchers;
import org.jni_zero.JNINamespace;
import org.jni_zero.NativeMethods;

import org.chromium.base.Log;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.base.test.BaseJUnit4ClassRunner.ClassCleanupHook;
import org.chromium.base.test.util.Criteria;
import org.chromium.base.test.util.CriteriaHelper;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.ServiceImpl;
import org.chromium.content_public.browser.BrowserStartupController;

/**
 * Waits for pending {@code TabWebContentsDestroyer} instances to finish tearing down their {@code
 * WebContents} after all activities are closed and before {@code LifetimeAssert} checks run.
 *
 * <p>{@code @ServiceImpl(ClassCleanupHook.class)} registers this hook via {@code ServiceLoader} so
 * {@code BaseJUnit4ClassRunner} invokes it automatically after every test class.
 */
@JNINamespace("tabs")
@NullMarked
@ServiceImpl(ClassCleanupHook.class)
public class TabWebContentsDestroyerCleanupHook implements ClassCleanupHook {
    private static final String TAG = "TabWCDestroyerHook";

    @Override
    public void onAfterTestClass(Class<?> clazz) {
        if (!LibraryLoader.getInstance().isProcessInitialized()) return;

        try {
            CriteriaHelper.pollUiThread(
                    () -> {
                        if (!BrowserStartupController.getInstance().isFullBrowserStarted()) {
                            return;
                        }
                        Criteria.checkThat(
                                "Pending TabWebContentsDestroyer instances did not finish tearing"
                                        + " down",
                                TabWebContentsDestroyerCleanupHookJni.get()
                                        .getActiveInstanceCount(),
                                Matchers.is(0));
                    },
                    CriteriaHelper.DEFAULT_MAX_TIME_TO_POLL_LONG,
                    CriteriaHelper.DEFAULT_POLLING_INTERVAL);
        } catch (CriteriaHelper.TimeoutException e) {
            Log.e(
                    TAG,
                    "Timed out waiting for TabWebContentsDestroyer teardown after %s",
                    clazz.getSimpleName(),
                    e);
        }
    }

    @NativeMethods
    interface Natives {
        int getActiveInstanceCount();
    }
}
