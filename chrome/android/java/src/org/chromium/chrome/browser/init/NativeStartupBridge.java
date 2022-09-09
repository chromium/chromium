// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.chrome.browser.init;

import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.task.PostTask;
import org.chromium.content_public.browser.BrowserStartupController;
import org.chromium.content_public.browser.UiThreadTaskTraits;

/**
 * A class for native code to request full browser start when running in minimal browser mode.
 */
public class NativeStartupBridge {
    private static final String TAG = "NativeStartupBridge";

    @CalledByNative
    private static void loadFullBrowser() {
        if (BrowserStartupController.getInstance().isFullBrowserStarted()) return;
        final BrowserParts parts = new EmptyBrowserParts() {};

        PostTask.postTask(UiThreadTaskTraits.DEFAULT, new Runnable() {
            @Override
            public void run() {
                ChromeBrowserInitializer.getInstance().handlePreNativeStartupAndLoadLibraries(
                        parts);
                ChromeBrowserInitializer.getInstance().handlePostNativeStartup(
                        true /* isAsync */, parts);
            }
        });
    }
}
