// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.content.Context;
import android.content.res.Resources;
import android.os.Build;
import android.webkit.SelectionActionMenuClient;
import android.webkit.WebViewDelegate;

import org.chromium.android_webview.DualTraceEvent;
import org.chromium.android_webview.R;
import org.chromium.android_webview.StartupController;
import org.chromium.android_webview.StartupDiagnostics;
import org.chromium.android_webview.StartupMetrics;
import org.chromium.android_webview.StartupTasksRunner;
import org.chromium.android_webview.common.AwResource;
import org.chromium.base.AconfigFlaggedApiDelegate;
import org.chromium.base.ContextUtils;
import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.SelectionActionMenuClientWrapper;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.build.annotations.NullMarked;
import org.chromium.build.annotations.Nullable;
import org.chromium.ui.base.DeviceFormFactor;

import java.util.concurrent.ExecutionException;
import java.util.concurrent.FutureTask;

/** Delegate implementation for StartupController in the glue layer. */
@NullMarked
class StartupDelegateImpl implements StartupController.Delegate {
    private static final String STARTUP_ASSET_PATH_WORKAROUND_HISTOGRAM_NAME =
            "Android.WebView.AssetPathWorkaroundUsed.StartChromiumLocked";

    private final WebViewDelegate mWebViewDelegate;
    private final Runnable mOnStartupComplete;
    // This is only accessed during WebViewChromiumFactoryProvider.initialize() which is guarded by
    // the WebViewFactory lock in the framework, and on the UI thread during startChromium
    // which cannot be called before initialize() has completed.
    private @Nullable FutureTask<Void> mSetUpResourcesTask;
    private @Nullable FactoryStartupTimings mStartupTimings;

    StartupDelegateImpl(WebViewDelegate webViewDelegate, Runnable onStartupComplete) {
        mWebViewDelegate = webViewDelegate;
        mOnStartupComplete = onStartupComplete;
    }

    void setStartupTimings(FactoryStartupTimings timings) {
        mStartupTimings = timings;
    }

    /**
     * Set up resources on a background thread, in parallel with chromium initialization as it takes
     * some time. This method is called once during WebViewChromiumFactoryProvider initialization
     * which is guaranteed to finish before this field is accessed by waitForJavaResourcesSetup.
     *
     * @param packageId The package ID.
     * @param context The context.
     */
    void setUpResourcesOnBackgroundThread(int packageId, Context context) {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("StartupDelegateImpl.setUpResourcesOnBackgroundThread")) {
            assert mSetUpResourcesTask == null : "This method shouldn't be called twice.";

            Runnable setUpResourcesRunnable =
                    () -> {
                        try (DualTraceEvent e2 =
                                DualTraceEvent.scoped("StartupDelegateImpl.setUpResources")) {
                            R.onResourcesLoaded(packageId);

                            AwResource.setResources(context.getResources());
                            AwResource.setConfigKeySystemUuidMapping(
                                    android.R.array.config_keySystemUuidMapping);
                        }
                    };

            // Make sure that ResourceProvider is initialized before starting the browser process.
            mSetUpResourcesTask = new FutureTask<>(setUpResourcesRunnable, null);
            PostTask.postTask(TaskTraits.USER_VISIBLE, mSetUpResourcesTask);
        }
    }

    @Override
    public void waitForJavaResourcesSetup() {
        try (DualTraceEvent e =
                DualTraceEvent.scoped("StartupDelegateImpl.waitForJavaResourcesSetup")) {
            assert mSetUpResourcesTask != null;
            mSetUpResourcesTask.get();
        } catch (InterruptedException | ExecutionException e) {
            throw new RuntimeException(e);
        }
        // TODO(crbug.com/400413041) : Remove this workaround.
        // Try to work around the resources problem.
        //
        // WebViewFactory adds WebView's asset path to the host app before any
        // of the code in the APK starts running, but it adds it using an old
        // mechanism that doesn't persist if the app's resource configuration
        // changes for any other reason.
        //
        // By the time we get here, it's possible it's gone missing due to
        // something on the UI thread having triggered a resource update. This
        // can happen either because WebView initialization was triggered by a
        // background thread (and thus this code is running inside a posted task
        // on the UI thread which may have taken any amount of time to actually
        // run), or because the app used CookieManager first, which triggers the
        // code being loaded and WebViewFactory doing the initial resources add,
        // but does not call startChromium until the app uses some other
        // API, an arbitrary amount of time later. So, we can try to add them
        // again using the "better" method in WebViewDelegate.
        //
        // However, we only want to try this if the resources are actually
        // missing, because in the past we've seen this cause apps that were
        // working to *start* crashing. The first resource that gets accessed in
        // startup happens during the AwBrowserProcess.start() call when trying
        // to determine if the device is a tablet, and that's the most common
        // place for us to crash. So, try calling that same method and see if it
        // throws - if so then we're unlikely to make the situation any worse by
        // trying to fix the path.
        //
        // This cannot fix the problem in all cases - if the app is using a
        // weird ContextWrapper or doing other unusual things with
        // resources/assets then even adding it with this mechanism might not
        // help.
        try {
            DeviceFormFactor.isTablet();
            RecordHistogram.recordBooleanHistogram(
                    STARTUP_ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, false);
        } catch (Resources.NotFoundException e) {
            RecordHistogram.recordBooleanHistogram(
                    STARTUP_ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, true);
            mWebViewDelegate.addWebViewAssetPath(ContextUtils.getApplicationContext());
        }
    }

    @Override
    public boolean shouldForceNativeSandboxedServices() {
        AconfigFlaggedApiDelegate aconfigDelegate = AconfigFlaggedApiDelegate.getInstance();
        return aconfigDelegate != null
                && aconfigDelegate.isNativeWebViewZygoteEnabled(mWebViewDelegate);
    }

    @Override
    public long getDrawFnFunctionTable() {
        return DrawFunctor.getDrawFnFunctionTable();
    }

    @Override
    public long getDrawSWFunctionTable() {
        return GraphicsUtils.getDrawSWFunctionTable();
    }

    @Override
    public @Nullable SelectionActionMenuClientWrapper getSelectionActionMenuClient() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.CINNAMON_BUN) {
            SelectionActionMenuClient client =
                    mWebViewDelegate.getSelectionActionMenuClient(
                            ContextUtils.getApplicationContext());
            if (client != null) {
                return new SelectionActionMenuClientAdapter(client);
            }
        }
        return null;
    }

    @Override
    public void onStartupComplete() {
        mOnStartupComplete.run();
    }

    @Override
    public void onStartupDiagnosticsReady(StartupDiagnostics diagnostics) {
        // Stop early trace event collection.
        // They have already been emitted if a trace session was started to capture startup.
        EarlyTraceEvent.reset();

        StartupTasksRunner.StartupTimings timings = diagnostics.getStartupTimings();
        assert timings != null;

        // Record histograms
        StartupMetrics.recordChromiumInitTimes(timings);
        // Also create the trace events for the earlier WebViewChromiumFactoryProvider init,
        // which happens before tracing is ready.
        if (mStartupTimings != null) {
            mStartupTimings.recordInitTraces();
        }
    }
}
