// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.hardware_acceleration;

import android.app.Dialog;
import android.view.View;
import android.view.ViewTreeObserver.OnPreDrawListener;

import org.junit.Assert;

import org.chromium.base.SysUtils;
import org.chromium.base.ThreadUtils;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.test.util.CallbackHelper;
import org.chromium.chrome.browser.app.ChromeActivity;

import java.util.HashSet;
import java.util.Set;
import java.util.concurrent.atomic.AtomicBoolean;

/** Various utils for hardware acceleration tests. */
public class Utils {

    /**
     * Asserts that activity is hardware accelerated only on high-end devices. I.e. on low-end
     * devices hardware acceleration must be off.
     */
    public static void assertHardwareAcceleration(ChromeActivity activity) throws Exception {
        assertActivityAcceleration(activity);
        assertChildWindowAcceleration(activity);
    }

    /**
     * Asserts that there is no thread named 'RenderThread' (which is essential for hardware
     * acceleration).
     */
    public static void assertNoRenderThread() {
        Assert.assertFalse(collectThreadNames().contains("RenderThread"));
    }

    /** Asserts that the argument is true when HW acceleration is enabled and false otherwise. */
    public static void assertAcceleration(boolean accelerated) {
        if (SysUtils.isLowEndDevice()) {
            Assert.assertFalse(accelerated);
        } else {
            Assert.assertTrue(accelerated);
        }
    }

    private static void assertActivityAcceleration(final ChromeActivity activity) throws Exception {
        final AtomicBoolean accelerated = new AtomicBoolean();
        final CallbackHelper listenerCalled = new CallbackHelper();

        ThreadUtils.runOnUiThreadBlocking(
                () -> {
                    final View view = activity.getWindow().getDecorView();
                    view.getViewTreeObserver()
                            .addOnPreDrawListener(
                                    new OnPreDrawListener() {
                                        @Override
                                        public boolean onPreDraw() {
                                            view.getViewTreeObserver()
                                                    .removeOnPreDrawListener(this);
                                            accelerated.set(view.isHardwareAccelerated());
                                            listenerCalled.notifyCalled();
                                            return true;
                                        }
                                    });
                    view.invalidate();
                });

        listenerCalled.waitForCallback(0);
        assertAcceleration(accelerated.get());
    }

    private static void assertChildWindowAcceleration(final ChromeActivity activity)
            throws Exception {
        final AtomicBoolean accelerated = new AtomicBoolean();
        final CallbackHelper listenerCalled = new CallbackHelper();

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        final Dialog dialog = new Dialog(activity);
                        dialog.setContentView(
                                new View(activity) {
                                    @Override
                                    public void onAttachedToWindow() {
                                        super.onAttachedToWindow();
                                        accelerated.set(isHardwareAccelerated());
                                        listenerCalled.notifyCalled();
                                        dialog.dismiss();
                                    }
                                });
                        dialog.show();
                    }
                });

        listenerCalled.waitForCallback(0);
        assertAcceleration(accelerated.get());
    }

    private static Set<String> collectThreadNames() {
        Set<String> names = new HashSet<String>();
        for (Thread thread : Thread.getAllStackTraces().keySet()) {
            names.add(thread.getName());
        }
        return names;
    }
}
