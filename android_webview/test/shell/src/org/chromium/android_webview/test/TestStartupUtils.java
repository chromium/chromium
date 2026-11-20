// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.test;

import org.chromium.android_webview.StartupCallSite;
import org.chromium.android_webview.StartupController;
import org.chromium.android_webview.StartupDiagnostics;
import org.chromium.android_webview.shell.ContextManager;
import org.chromium.base.ContextUtils;
import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.ThreadUtils;
import org.chromium.base.library_loader.LibraryProcessType;
import org.chromium.build.annotations.Nullable;
import org.chromium.content_public.browser.ChildProcessCreationParams;
import org.chromium.content_public.browser.ChildProcessLauncherHelper;
import org.chromium.net.NetworkChangeNotifier;

/** Test-only utility for orchestrating StartupController in integration tests and test shell. */
public class TestStartupUtils {
    public static class TestStartupDelegate implements StartupController.Delegate {
        private final boolean mUseVulkan;

        public TestStartupDelegate() {
            this(false);
        }

        public TestStartupDelegate(boolean useVulkan) {
            mUseVulkan = useVulkan;
        }

        @Override
        public void waitForJavaResourcesSetup() {}

        @Override
        public void configureChildProcessLauncher() {
            final boolean isExternalService = false;
            final boolean ignoreVisibilityForImportance = false;
            final boolean isNativeWebViewZygoteEnabled = false;
            ChildProcessCreationParams.set(
                    ContextUtils.getApplicationContext().getPackageName(),
                    ContextUtils.getApplicationContext().getPackageName(),
                    isExternalService,
                    LibraryProcessType.PROCESS_WEBVIEW_CHILD,
                    ignoreVisibilityForImportance,
                    isNativeWebViewZygoteEnabled);
            ChildProcessLauncherHelper.initialize();
        }

        @Override
        public long getDrawFnFunctionTable() {
            return ContextManager.getDrawFnFunctionTable(mUseVulkan);
        }

        @Override
        public long getDrawSWFunctionTable() {
            return 0;
        }

        @Override
        public @Nullable SelectionActionMenuClientWrapper getSelectionActionMenuClient() {
            return null;
        }

        @Override
        public void onStartupComplete() {}

        @Override
        public void onStartupDiagnosticsReady(StartupDiagnostics diagnostics) {}
    }

    public static StartupController startForTesting() {
        return startForTesting(false);
    }

    public static StartupController startForTesting(boolean useVulkan) {
        return startForTesting(new TestStartupDelegate(useVulkan));
    }

    public static StartupController startForTesting(StartupController.Delegate delegate) {
        StartupController controller;
        try {
            controller = StartupController.getInstance();
        } catch (IllegalStateException e) {
            controller = StartupController.initialize(delegate);
        }
        // Tell startup which looper to use rather than letting it bind the main looper itself:
        // tests that run the UI thread on a background looper (e.g. AwUncaughtExceptionTest) have
        // already set it, and ThreadUtils would reject a second, different looper. For everything
        // else this resolves to the main looper, which is what startup would have picked anyway.
        controller.maybeSetChromiumUiThread(ThreadUtils.getUiThreadLooper());
        controller.triggerAndWaitForChromiumStarted(StartupCallSite.TEST_SHELL);
        NetworkChangeNotifier.forceConnectivityState(true);
        return controller;
    }
}
