// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview;

import android.os.Build;

import org.chromium.android_webview.gfx.AwDrawFnImpl;
import org.chromium.base.ContextUtils;
import org.chromium.base.library_loader.LibraryLoader;
import org.chromium.build.annotations.NullMarked;
import org.chromium.ui.base.ResourceBundle;

import java.util.concurrent.CountDownLatch;

/** Controller responsible for managing WebView startup lifecycle and tasks. */
@NullMarked
public class StartupController {
    /** Delegate interface for callbacks needed during WebView global startup. */
    public interface Delegate {
        /** Wait until it's possible to access Android resources defined in the Chromium APK. */
        void waitForJavaResourcesSetup();

        /** Returns whether to use native sandboxed services. */
        boolean shouldForceNativeSandboxedServices();

        // TODO(abhijithnair): Rethink whether `getDrawFnFunctionTable` and `getDrawSWFunctionTable`
        // are the right interface. See
        // https://chromium-review.git.corp.google.com/c/chromium/src/+/8257352/comment/d9c4282e_3fa74a88/
        /** Returns the function table pointer for hardware-accelerated drawing. */
        long getDrawFnFunctionTable();

        /** Returns the function table pointer for software drawing. */
        long getDrawSWFunctionTable();
    }

    private final Delegate mDelegate;

    private final CountDownLatch mNonUiThreadCapableStartupTasksLatch = new CountDownLatch(1);

    public StartupController(Delegate delegate) {
        mDelegate = delegate;
    }

    // These are startup tasks that can either run during provider init or during `startChromium`.
    // This is extracted out so that we can experiment with calling this in either of these
    // locations.
    public void runNonUiThreadCapableStartupTasks() {
        assert mDelegate != null;
        try {
            ResourceBundle.setAvailablePakLocales(AwLocaleConfig.getWebViewSupportedPakLocales());

            try (DualTraceEvent ignored2 =
                    DualTraceEvent.scoped("LibraryLoader.ensureInitialized")) {
                LibraryLoader.getInstance().ensureInitialized();
            }

            configureDrawingFunctions();
            AwContentsStatics.setCheckClearTextPermitted(
                    ContextUtils.getApplicationContext().getApplicationInfo().targetSdkVersion
                            >= Build.VERSION_CODES.O);
        } finally {
            mNonUiThreadCapableStartupTasksLatch.countDown();
        }
    }

    private void configureDrawingFunctions() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("StartupController.configureDrawingFunctions")) {
            AwDrawFnImpl.setDrawFnFunctionTable(mDelegate.getDrawFnFunctionTable());
            AwContents.setAwDrawSWFunctionTable(mDelegate.getDrawSWFunctionTable());
        }
    }

    public void waitForNonUiThreadCapableStartupTasks() {
        try (DualTraceEvent e2 =
                DualTraceEvent.scoped(
                        "StartupController.waitForNonUiThreadCapableStartupTasks")) {
            mNonUiThreadCapableStartupTasksLatch.await();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }
}
