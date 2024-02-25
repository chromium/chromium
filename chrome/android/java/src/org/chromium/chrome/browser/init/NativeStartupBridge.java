// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import org.jni_zero.CalledByNative;

import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.content_public.browser.BrowserStartupController;

/** A class for native code to request full browser start when running in minimal browser mode. */
public class NativeStartupBridge {
    @CalledByNative
    private static void loadFullBrowser() {
        if (BrowserStartupController.getInstance().isFullBrowserStarted()) return;
        final BrowserParts parts = new EmptyBrowserParts() {};

        PostTask.postTask(
                TaskTraits.UI_DEFAULT,
                new Runnable() {
                    @Override
                    public void run() {
                        ChromeBrowserInitializer.getInstance()
                                .handlePreNativeStartupAndLoadLibraries(parts);
                        ChromeBrowserInitializer.getInstance()
                                .handlePostNativeStartup(/* isAsync= */ true, parts);
                    }
                });
    }
}
