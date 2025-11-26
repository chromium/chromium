// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
import android.os.flagging.AconfigPackage;
import android.provider.DeviceConfig;
import android.provider.DeviceConfig.Properties;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.PacProcessor;
import android.webkit.ServiceWorkerController;
import android.webkit.TokenBindingService;
import android.webkit.TracingController;
import android.webkit.ValueCallback;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.webkit.WebViewDatabase;
import android.webkit.WebViewDelegate;
import android.webkit.WebViewFactory;
import android.webkit.WebViewFactoryProvider;
import android.webkit.WebViewProvider;

import androidx.annotation.GuardedBy;
import androidx.annotation.IntDef;
import androidx.annotation.RequiresApi;

import com.android.webview.chromium.SharedStatics.ApiCall;
import com.android.webview.chromium.WebViewChromiumAwInit.CallSite;

import org.chromium.android_webview.AwBrowserMainParts;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.DualTraceEvent;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.FlagOverrideHelper;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.PlatformServiceBridge;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.common.WebViewCachedFlags;
import org.chromium.android_webview.safe_mode.BrowserSafeModeActionList;
import org.chromium.android_webview.safe_mode.DisableStartupTasksSafeModeAction;
import org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction;
import org.chromium.base.ApkInfo;
import org.chromium.base.BundleUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.EarlyTraceEvent;
import org.chromium.base.Log;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.task.PostTask;
import org.chromium.base.task.TaskTraits;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.services.tracing.TracingServiceFeatures;
import org.chromium.support_lib_boundary.ProcessGlobalConfigConstants;

import java.io.File;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.List;
import java.util.Map;
import java.util.Set;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

/**
 * Entry point to the WebView. The system framework talks to this class to get instances of the
 * implementation classes.
 *
 * <p>The exact initialization process depends on the platform OS level:
 *
 * <ul>
 *   <li>On API 21 (no longer supported), the platform invoked a parameterless constructor. Since we
 *       didn't have a WebViewDelegate instance, this required us to invoke WebViewDelegate methods
 *       via reflection. This constructor has been removed from the code as we no longer support
 *       Android 21.
 *   <li>From API 22 through API 25, the platform instead directly calls the constructor with a
 *       WebViewDelegate parameter (See internal CL http://ag/577188 or the public AOSP cherrypick
 *       https://r.android.com/114870). API 22 (no longer supported) would fallback to the
 *       parameterless constructor if the first constructor call throws an exception, however this
 *       fallback was removed in API 23.
 *   <li>Starting in API 26, the platform calls {@link #create} instead of calling the constructor
 *       directly (see internal CLs http://ag/1334128 and http://ag/1846560).
 *   <li>From API 27 onward, the platform code is updated during each release to use the {@code
 *       WebViewChromiumFactoryProviderForX} subclass, where "X" is replaced by the actual platform
 *       API version (ex. "ForOMR1"). It still invokes the {@link #create} method on the subclass.
 *       While the OS version is still under development, the "ForX" subclass implements the new
 *       platform APIs (in a private codebase). Once the APIs for that version have been finalized,
 *       we eventually roll these implementations into this class and the "ForX" subclass just calls
 *       directly into this implementation.
 * </ul>
 */
@SuppressWarnings("deprecation")
@Lifetime.Singleton
public class WebViewChromiumFactoryProvider implements WebViewFactoryProvider {
    private static final String TAG = "WVCFactoryProvider";

    private static final String CHROMIUM_PREFS_NAME = "WebViewChromiumPrefs";
    private static final String VERSION_CODE_PREF = "lastVersionCodeUsed";

    private static final String SUPPORT_LIB_GLUE_AND_BOUNDARY_INTERFACE_PREFIX =
            "org.chromium.support_lib_";

    private static final String ASSET_PATH_WORKAROUND_HISTOGRAM_NAME =
            "Android.WebView.AssetPathWorkaroundUsed.FactoryInit";

    private static final String REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME =
            "Android.WebView.RegisterResourcePathsAvailable2";

    private static final String REGISTER_RESOURCE_PATHS_TIMES_HISTOGRAM_NAME =
            "Android.WebView.RegisterResourcePathsTimeTaken";

    @GuardedBy("mAwInit.getLazyInitLock()")
    private TracingController mTracingController;

    private static final Object sSingletonLock = new Object();
    private static WebViewChromiumFactoryProvider sSingleton;

    private final WebViewChromiumRunQueue mRunQueue = new WebViewChromiumRunQueue();

    /* package */ WebViewChromiumRunQueue getRunQueue() {
        return mRunQueue;
    }

    // We have a 4 second timeout to try to detect deadlocks to detect and aid in debugging
    // deadlocks.
    // Do not call this method while on the UI thread!
    /* package */ void runVoidTaskOnUiThreadBlocking(Runnable r) {
        mRunQueue.runVoidTaskOnUiThreadBlocking(r);
    }

    /* package */ <T> T runOnUiThreadBlocking(Callable<T> c) {
        return mRunQueue.runBlockingFuture(new FutureTask<T>(c));
    }

    /* package */ void addTask(Runnable task) {
        mRunQueue.addTask(task);
    }

    /**
     * Class that takes care of chromium lazy initialization. This is package-public so that a
     * downstream subclass can access it.
     */
    /* package */ WebViewChromiumAwInit mAwInit;

    private SharedStatics mSharedStatics;

    // Should only be called after initialize.
    public SharedStatics getSharedStatics() {
        assert mSharedStatics != null;
        return mSharedStatics;
    }

    private SharedPreferences mWebViewPrefs;
    private WebViewDelegate mWebViewDelegate;

    private static class StaticsAdapter implements Statics {
        private final SharedStatics mSharedStatics;

        StaticsAdapter(SharedStatics sharedStatics) {
            mSharedStatics = sharedStatics;
        }

        @Override
        public String findAddress(String addr) {
            return mSharedStatics.findAddress(addr);
        }

        @Override
        public String getDefaultUserAgent(Context context) {
            return mSharedStatics.getDefaultUserAgent(context);
        }

        @Override
        public void setWebContentsDebuggingEnabled(boolean enable) {
            mSharedStatics.setWebContentsDebuggingEnabled(enable);
        }

        @Override
        public void clearClientCertPreferences(Runnable onCleared) {
            mSharedStatics.clearClientCertPreferences(onCleared);
        }

        @Override
        public void freeMemoryForTests() {
            mSharedStatics.freeMemoryForTests();
        }

        @Override
        public void enableSlowWholeDocumentDraw() {
            mSharedStatics.enableSlowWholeDocumentDraw();
        }

        @Override
        public Uri[] parseFileChooserResult(int resultCode, Intent intent) {
            return mSharedStatics.parseFileChooserResult(resultCode, intent);
        }

        @Override
        public void initSafeBrowsing(Context context, ValueCallback<Boolean> callback) {
            mSharedStatics.initSafeBrowsing(context, CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public void setSafeBrowsingWhitelist(List<String> urls, ValueCallback<Boolean> callback) {
            mSharedStatics.setSafeBrowsingAllowlist(
                    urls, CallbackConverter.fromValueCallback(callback));
        }

        @Override
        public Uri getSafeBrowsingPrivacyPolicyUrl() {
            return mSharedStatics.getSafeBrowsingPrivacyPolicyUrl();
        }

        @SuppressWarnings("UnusedMethod")
        public boolean isMultiProcessEnabled() {
            return mSharedStatics.isMultiProcessEnabled();
        }

        @SuppressWarnings("UnusedMethod")
        public String getVariationsHeader() {
            return mSharedStatics.getVariationsHeader();
        }
    }
    ;

    private Statics mStaticsAdapter;

    private boolean mIsSafeModeEnabled;
    private boolean mIsMultiProcessEnabled;
    private boolean mIsAsyncStartupWithMultiProcessExperimentEnabled;

    public static class InitInfo {
        // Timestamp of init start and duration, used in the
        // 'WebView.Startup.CreationTime.Stage1.FactoryInit' trace event.
        public long mStartTime;
        public long mDuration;

        // Timestamp of the framework getProvider() method start and elapsed time until init is
        // finished, used in the 'WebView.Startup.CreationTime.TotalFactoryInitTime'
        // trace event.
        public long mTotalFactoryInitStartTime;
        public long mTotalFactoryInitDuration;
    }

    private final InitInfo mInitInfo = new InitInfo();

    /** Thread-safe way to set the one and only WebViewChromiumFactoryProvider. */
    private static void setSingleton(WebViewChromiumFactoryProvider provider) {
        synchronized (sSingletonLock) {
            if (sSingleton != null) {
                throw new RuntimeException(
                        "WebViewChromiumFactoryProvider should only be set once!");
            }
            sSingleton = provider;
        }
    }

    /** Thread-safe way to get the one and only WebViewChromiumFactoryProvider. */
    static WebViewChromiumFactoryProvider getSingleton() {
        synchronized (sSingletonLock) {
            if (sSingleton == null) {
                throw new RuntimeException("WebViewChromiumFactoryProvider has not been set!");
            }
            return sSingleton;
        }
    }

    /** Entry point for Android 26 (Oreo) and above. See class docs for initialization details. */
    public static WebViewChromiumFactoryProvider create(WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProvider(delegate);
    }

    /**
     * Entry point for Android 22 (LMR1) through Android 25 (NMR1). Although this is still invoked
     * by {@link #create}, this constructor was invoked directly before {@link #create} was defined.
     * See class docs for initialization details.
     */
    public WebViewChromiumFactoryProvider(WebViewDelegate delegate) {
        initialize(delegate);
    }

    // Separate method to allow downstream to override when needed.
    WebViewChromiumAwInit createAwInit() {
        try (DualTraceEvent ignored =
                DualTraceEvent.scoped("WebViewChromiumFactoryProvider.createAwInit")) {
            return new WebViewChromiumAwInit(this);
        }
    }

    // Separate method to allow downstream to override when needed.
    ContentSettingsAdapter createContentSettingsAdapter(AwSettings settings) {
        return new ContentSettingsAdapter(settings);
    }

    // Overridden in downstream subclass when building using the unreleased Android SDK.
    boolean shouldEnableUserAgentReduction() {
        return false;
    }

    // Overridden in downstream subclass when building using the unreleased Android SDK.
    boolean shouldEnableFileSystemAccess() {
        return false;
    }

    private void deleteContentsOnPackageDowngrade(PackageInfo packageInfo) {
        try (DualTraceEvent e2 =
                DualTraceEvent.scoped(
                        "WebViewChromiumFactoryProvider.deleteContentsOnPackageDowngrade")) {
            // Use shared preference to check for package downgrade.
            int lastVersion = mWebViewPrefs.getInt(VERSION_CODE_PREF, 0);
            int currentVersion = packageInfo.versionCode;
            if (isBranchDowngrade(currentVersion, lastVersion)) {
                // The WebView package has been downgraded since we last ran in this
                // application. Delete the WebView data directory's contents.
                String dataDir = PathUtils.getDataDirectory();
                Log.i(
                        TAG,
                        "WebView package downgraded from "
                                + lastVersion
                                + " to "
                                + currentVersion
                                + "; deleting contents of "
                                + dataDir);
                deleteContents(new File(dataDir));
            }
            if (lastVersion != currentVersion) {
                mWebViewPrefs.edit().putInt(VERSION_CODE_PREF, currentVersion).apply();
            }
        }
    }

    /**
     * This must not be called until {@link #initialize(WebViewDelegate)} has set mWebViewDelegate.
     */
    public void addWebViewAssetPath(Context ctx) {
        mWebViewDelegate.addWebViewAssetPath(ctx);
    }

    private boolean shouldEnableStartupTasksExperiment() {
        if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_USE_STARTUP_TASKS_LOGIC)) {
            return true;
        }
        // TODO: Remove this once WebViewCachedFlags has landed (and seems safe).
        if (DisableStartupTasksSafeModeAction.isStartupTasksExperimentDisabled()) {
            return false;
        }
        return WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_USE_STARTUP_TASKS_LOGIC);
    }

    private boolean shouldEnableStartupTasksExperimentP2() {
        if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_USE_STARTUP_TASKS_LOGIC_P2)) {
            return true;
        }

        return WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_USE_STARTUP_TASKS_LOGIC_P2);
    }

    private boolean shouldEnableStartupTasksYieldToNativeExperiment() {
        if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_STARTUP_TASKS_YIELD_TO_NATIVE)) {
            return true;
        }

        return WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_STARTUP_TASKS_YIELD_TO_NATIVE);
    }

    boolean isAsyncStartupWithMultiProcessExperimentEnabled() {
        if (CommandLine.getInstance()
                .hasSwitch(AwSwitches.WEBVIEW_STARTUP_TASKS_PLUS_MULTI_PROCESS)) {
            return true;
        }

        return mIsAsyncStartupWithMultiProcessExperimentEnabled;
    }

    @SuppressWarnings({"NoContextGetApplicationContext"})
    private void initialize(WebViewDelegate webViewDelegate) {
        // Capture startup init time before anything else.
        mInitInfo.mStartTime = SystemClock.uptimeMillis();
        // Use `ScopedSysTraceEvent` until `EarlyTraceEvent` is potentially enabled further down.
        try (ScopedSysTraceEvent e1 =
                ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.initialize")) {
            ThreadUtils.setWillOverrideUiThread();
            PackageInfo packageInfo;
            try (ScopedSysTraceEvent e2 =
                    ScopedSysTraceEvent.scoped(
                            "WebViewChromiumFactoryProvider.getLoadedPackageInfo")) {
                // The package is used to locate the services for copying crash minidumps and
                // requesting variations seeds. So it must be set before initializing variations and
                // before a renderer has a chance to crash.
                packageInfo = WebViewFactory.getLoadedPackageInfo();
            }
            AwBrowserProcess.setWebViewPackageName(packageInfo.packageName);
            AwBrowserProcess.initializeApkType(packageInfo.applicationInfo);

            mWebViewDelegate = webViewDelegate;
            Application application = webViewDelegate.getApplication();
            Context ctx = application.getApplicationContext();
            // If the application context is DE, but we have credentials, use a CE context instead
            try (ScopedSysTraceEvent e2 =
                    ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.checkStorage")) {
                checkStorageIsNotDeviceProtected(application);
            } catch (IllegalArgumentException e) {
                if (!ctx.getSystemService(UserManager.class).isUserUnlocked()) {
                    throw e;
                }
                ctx = ctx.createCredentialProtectedStorageContext();
            }

            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                // Since N, getSharedPreferences creates the preference dir if it doesn't exist,
                // causing a disk write.
                mWebViewPrefs = ctx.getSharedPreferences(CHROMIUM_PREFS_NAME, Context.MODE_PRIVATE);
                WebViewCachedFlags.init(mWebViewPrefs);
            }

            if (WebViewCachedFlags.get()
                    .isCachedFeatureEnabled(AwFeatures.WEBVIEW_EARLY_STARTUP_TRACING)) {
                // Enable capture of early timestamps for Perfetto traces.
                // This is reset in `WebViewChromiumAwInit#recordStartupMetrics`.
                // `TraceEvent` and `DualTraceEvent` can be used from this point.
                EarlyTraceEvent.enable();
            }

            mAwInit = createAwInit();
            mSharedStatics = new SharedStatics(mAwInit);
            mStaticsAdapter = new StaticsAdapter(mSharedStatics);
            if (Looper.myLooper() == Looper.getMainLooper()) {
                mAwInit.setProviderInitOnMainLooperLocation(
                        new Throwable(
                                "Location where WebViewChromiumFactoryProvider init was"
                                        + " started on the Android main looper"));
            }

            try (DualTraceEvent ignored =
                    DualTraceEvent.scoped("WebViewChromiumFactoryProvider.initCommandLine")) {
                // This may take ~20 ms only on userdebug devices.
                CommandLineUtil.initCommandLine();
            }

            if (shouldEnableContextExperiment(ctx)) {
                try (DualTraceEvent ignored =
                        DualTraceEvent.scoped(
                                "WebViewChromiumFactoryProvider.enableContextExperiment")) {
                    ClassLoaderContextWrapperFactory.setOverrideInfo(
                            packageInfo.packageName,
                            android.R.style.Theme_DeviceDefault_DayNight,
                            Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY);
                    // Use this to report the actual state of the feature at runtime.
                    AwBrowserMainParts.setUseWebViewContext(true);
                }
            }

            // WebView needs to make sure to always use the wrapped application context.
            ctx = ClassLoaderContextWrapperFactory.get(ctx);
            ContextUtils.initApplicationContext(ctx);

            // Ensuring we set this before we might read it in any future calls to ApkInfo.
            // ApkInfo requires ContextUtils' application context, so this has to happen after.
            ApkInfo.setBrowserPackageInfo(packageInfo);

            // Find the package ID for the package that WebView's resources come from.
            // This will be the donor package if there is one, not our main package.
            String resourcePackage = packageInfo.packageName;
            if (packageInfo.applicationInfo.metaData != null) {
                resourcePackage =
                        packageInfo.applicationInfo.metaData.getString(
                                "com.android.webview.WebViewDonorPackage", resourcePackage);
            }
            int packageId;
            try {
                packageId = webViewDelegate.getPackageId(ctx.getResources(), resourcePackage);
                RecordHistogram.recordBooleanHistogram(ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, false);
            } catch (RuntimeException e) {
                // We failed to find the package ID, which likely means this context's AssetManager
                // doesn't have WebView loaded in it. This may be because WebViewFactory doesn't add
                // the package persistently to ResourcesManager and the app's AssetManager has been
                // recreated. Try adding it again using WebViewDelegate, which does add it
                // persistently.
                RecordHistogram.recordBooleanHistogram(ASSET_PATH_WORKAROUND_HISTOGRAM_NAME, true);
                addWebViewAssetPath(ctx);
                packageId = webViewDelegate.getPackageId(ctx.getResources(), resourcePackage);
            }

            mAwInit.setUpResourcesOnBackgroundThread(packageId, ctx);

            AndroidXProcessGlobalConfig.extractConfigFromApp(application.getClassLoader());

            // Limiting scope of the command line switch object before it is passed to native.
            // The reference to `cl` eventually becomes a stale object, causing incorrect behavior,
            // since Java switches are incongruent with Native switches.
            {
                CommandLine cl = CommandLine.getInstance();

                mIsMultiProcessEnabled = webViewDelegate.isMultiProcessEnabled();
                if (mIsMultiProcessEnabled) {
                    cl.appendSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
                }
                Log.i(
                        TAG,
                        "version=%s (%s) minSdkVersion=%s multiprocess=%s packageId=%s splits=%s",
                        VersionConstants.PRODUCT_VERSION,
                        BuildConfig.VERSION_CODE,
                        BuildConfig.MIN_SDK_VERSION,
                        mIsMultiProcessEnabled,
                        packageId,
                        BundleUtils.getInstalledSplitNamesForLogging());

                // Enable modern SameSite cookie behavior if the app targets at least S.
                if (ctx.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.S) {
                    cl.appendSwitch(AwSwitches.WEBVIEW_ENABLE_MODERN_COOKIE_SAME_SITE);
                }

                // Enable logging JS console messages in system logs only if the app is debuggable
                // or
                // it's a debuggable android build.
                if (ApkInfo.isDebugAndroidOrApp()) {
                    cl.appendSwitch(AwSwitches.WEBVIEW_LOG_JS_CONSOLE_MESSAGES);
                }
            }

            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
                checkProcessUid();
            }

            String webViewPackageName = AwBrowserProcess.getWebViewPackageName();
            boolean isDeveloperModeEnabled =
                    DeveloperModeUtils.isDeveloperModeEnabled(webViewPackageName);
            RecordHistogram.recordBooleanHistogram(
                    "Android.WebView.DevUi.DeveloperModeEnabled", isDeveloperModeEnabled);
            Map<String, Boolean> flagOverrides = null;
            if (isDeveloperModeEnabled) {
                long start = SystemClock.elapsedRealtime();
                try (DualTraceEvent ignored =
                        DualTraceEvent.scoped("WebViewChromiumFactoryProvider.getFlagOverrides")) {
                    FlagOverrideHelper helper =
                            new FlagOverrideHelper(ProductionSupportedFlagList.sFlagList);
                    flagOverrides = DeveloperModeUtils.getFlagOverrides(webViewPackageName);
                    helper.applyFlagOverrides(flagOverrides);

                    RecordHistogram.recordCount100Histogram(
                            "Android.WebView.DevUi.ToggledFlagCount", flagOverrides.size());
                } finally {
                    long end = SystemClock.elapsedRealtime();
                    RecordHistogram.recordTimesHistogram(
                            "Android.WebView.DevUi.FlagLoadingBlockingTime", end - start);
                }
            }

            AndroidXProcessGlobalConfig androidXConfig = AndroidXProcessGlobalConfig.getConfig();
            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                try (DualTraceEvent e2 =
                        DualTraceEvent.scoped(
                                "WebViewChromiumFactoryProvider.loadChromiumLibrary")) {
                    String dataDirectoryBasePath = androidXConfig.getDataDirectoryBasePathOrNull();
                    String cacheDirectoryBasePath =
                            androidXConfig.getCacheDirectoryBasePathOrNull();

                    RecordHistogram.recordBooleanHistogram(
                            "Android.WebView.AppliedProcessGlobalDataDirectoryBasePath",
                            dataDirectoryBasePath != null);
                    RecordHistogram.recordBooleanHistogram(
                            "Android.WebView.AppliedProcessGlobalCacheDirectoryBasePath",
                            cacheDirectoryBasePath != null);

                    String dataDirectorySuffix = webViewDelegate.getDataDirectorySuffix();

                    AwBrowserProcess.loadLibrary(
                            dataDirectoryBasePath, cacheDirectoryBasePath, dataDirectorySuffix);
                }

                if (WebViewCachedFlags.get()
                        .isCachedFeatureEnabled(AwFeatures.WEBVIEW_EARLY_PERFETTO_INIT)) {
                    AwBrowserProcess.initPerfetto(
                            WebViewCachedFlags.get()
                                    .isCachedFeatureEnabled(
                                            TracingServiceFeatures.ENABLE_PERFETTO_SYSTEM_TRACING));
                }

                try (DualTraceEvent e2 =
                        DualTraceEvent.scoped(
                                "WebViewChromiumFactoryProvider.loadGlueLayerPlatSupportLibrary")) {
                    System.loadLibrary("webviewchromium_plat_support");
                }

                deleteContentsOnPackageDowngrade(packageInfo);
            }

            boolean partitionedCookies =
                    androidXConfig.getPartitionedCookiesEnabled() == null
                            ? true
                            : androidXConfig.getPartitionedCookiesEnabled();

            AwBrowserMainParts.setPartitionedCookiesDefaultState(partitionedCookies);
            if (!partitionedCookies) {
                AwCookieManager.disablePartitionedCookiesGlobal();
            }

            // Now safe to use WebView data directory.

            if (flagOverrides != null) {
                AwContentsStatics.logFlagOverridesWithNative(flagOverrides);
            }

            SafeModeController controller = SafeModeController.getInstance();
            controller.registerActions(BrowserSafeModeActionList.sList);
            mIsSafeModeEnabled = controller.isSafeModeEnabled(webViewPackageName);
            RecordHistogram.recordBooleanHistogram(
                    "Android.WebView.SafeMode.SafeModeEnabled", mIsSafeModeEnabled);
            if (mIsSafeModeEnabled) {
                try {
                    long safeModeQueryExecuteStart = SystemClock.elapsedRealtime();
                    Set<String> actions = controller.queryActions(webViewPackageName);
                    Log.w(
                            TAG,
                            "WebViewSafeMode is enabled: received %d SafeModeActions",
                            actions.size());
                    controller.executeActions(actions);
                    long safeModeQueryExecuteEnd = SystemClock.elapsedRealtime();
                    RecordHistogram.recordTimesHistogram(
                            "Android.WebView.SafeMode.QueryAndExecuteBlockingTime",
                            safeModeQueryExecuteEnd - safeModeQueryExecuteStart);
                } catch (Throwable t) {
                    // Don't let SafeMode crash WebView. Instead just log the error.
                    Log.e(TAG, "WebViewSafeMode threw exception: ", t);
                }
            }

            // This must happen after pref value has been read and SafeMode setup has completed.
            setupStartupTaskExperiments(androidXConfig);

            if (!FastVariationsSeedSafeModeAction.hasRun()) {
                mAwInit.startVariationsInit();
            }

            if (WebViewCachedFlags.get()
                    .isCachedFeatureEnabled(AwFeatures.WEBVIEW_MOVE_WORK_TO_PROVIDER_INIT)) {
                PostTask.postTask(
                        TaskTraits.USER_VISIBLE,
                        () -> {
                            PlatformServiceBridge.getInstance();
                        });
                mAwInit.runNonUiThreadCapableStartupTasks();
            }

            FlagOverrideHelper helper =
                    new FlagOverrideHelper(ProductionSupportedFlagList.sFlagList);
            helper.applyFlagOverrides(
                    Map.of(AwFeatures.WEBVIEW_FILE_SYSTEM_ACCESS, shouldEnableFileSystemAccess()));

            // Apply user-agent reduction overrides for WebView. These features
            // are intended to be enabled only for Android B+.
            // 1) ReduceUserAgentMinorVersion: Enables reduction of the user-agent minor version.
            // 2) WebViewReduceUAAndroidVersionDeviceModel: Enables reduction of the user-agent
            //    Android version and device model.
            helper.applyFlagOverrides(
                    Map.of(
                            AwFeatures.WEBVIEW_REDUCE_UA_ANDROID_VERSION_DEVICE_MODEL,
                            shouldEnableUserAgentReduction(),
                            BlinkFeatures.REDUCE_USER_AGENT_MINOR_VERSION,
                            shouldEnableUserAgentReduction()));

            setSingleton(this);
        }

        mInitInfo.mDuration = SystemClock.uptimeMillis() - mInitInfo.mStartTime;
        RecordHistogram.recordTimesHistogram(
                "Android.WebView.Startup.CreationTime.Stage1.FactoryInit", mInitInfo.mDuration);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.S) {
            WebViewFactory.StartupTimestamps startupTimestamps =
                    mWebViewDelegate.getStartupTimestamps();
            mInitInfo.mTotalFactoryInitStartTime = startupTimestamps.getWebViewLoadStart();
            mInitInfo.mTotalFactoryInitDuration =
                    SystemClock.uptimeMillis() - mInitInfo.mTotalFactoryInitStartTime;
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.TotalFactoryInitTime",
                    mInitInfo.mTotalFactoryInitDuration);
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.CreateContextTime",
                    startupTimestamps.getCreateContextEnd()
                            - startupTimestamps.getCreateContextStart());
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.AssetsAddTime",
                    startupTimestamps.getAddAssetsEnd() - startupTimestamps.getAddAssetsStart());
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.GetClassLoaderTime",
                    startupTimestamps.getGetClassLoaderEnd()
                            - startupTimestamps.getGetClassLoaderStart());
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.NativeLoadTime",
                    startupTimestamps.getNativeLoadEnd() - startupTimestamps.getNativeLoadStart());
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.GetProviderClassForNameTime",
                    startupTimestamps.getProviderClassForNameEnd()
                            - startupTimestamps.getProviderClassForNameStart());
        }
    }

    private void setupStartupTaskExperiments(AndroidXProcessGlobalConfig androidXConfig) {
        switch (androidXConfig.getUiThreadStartupMode()) {
            case ProcessGlobalConfigConstants.UI_THREAD_STARTUP_MODE_DEFAULT:
                setStartupTaskExperimentValues(
                        shouldEnableStartupTasksExperiment(),
                        shouldEnableStartupTasksExperimentP2(),
                        shouldEnableStartupTasksYieldToNativeExperiment());
                return;
            case ProcessGlobalConfigConstants.UI_THREAD_STARTUP_MODE_SYNC:
                setStartupTaskExperimentValues(
                        /* enablePhase1= */ false,
                        /* enablePhase2= */ false,
                        /* enableYieldToNative= */ false);
                return;
            case ProcessGlobalConfigConstants.UI_THREAD_STARTUP_MODE_ASYNC_LONG_TASKS:
                setStartupTaskExperimentValues(
                        /* enablePhase1= */ true,
                        /* enablePhase2= */ false,
                        /* enableYieldToNative= */ false);
                return;
            case ProcessGlobalConfigConstants.UI_THREAD_STARTUP_MODE_ASYNC_SHORT_TASKS:
                setStartupTaskExperimentValues(
                        /* enablePhase1= */ false,
                        /* enablePhase2= */ true,
                        /* enableYieldToNative= */ false);
                return;
            case ProcessGlobalConfigConstants.UI_THREAD_STARTUP_MODE_ASYNC_VERY_SHORT_TASKS:
                setStartupTaskExperimentValues(
                        /* enablePhase1= */ false,
                        /* enablePhase2= */ false,
                        /* enableYieldToNative= */ true);
                return;
            case ProcessGlobalConfigConstants.UI_THREAD_STARTUP_MODE_ASYNC_PLUS_MULTI_PROCESS:
                setStartupTaskExperimentValues(
                        /* enablePhase1= */ false,
                        /* enablePhase2= */ false,
                        /* enableYieldToNative= */ true);
                mIsAsyncStartupWithMultiProcessExperimentEnabled = true;
                return;
            default:
                throw new RuntimeException(
                        "Invalid AndroidXProcessGlobalConfig UI thread startup mode: "
                                + androidXConfig.getUiThreadStartupMode());
        }
    }

    private void setStartupTaskExperimentValues(
            boolean enablePhase1, boolean enablePhase2, boolean enableYieldToNative) {
        mAwInit.setStartupTaskExperimentEnabled(enablePhase1);
        AwBrowserMainParts.setWebViewStartupTasksLogicIsEnabled(enablePhase1);

        mAwInit.setStartupTaskExperimentP2Enabled(enablePhase2);
        AwBrowserMainParts.setWebViewStartupTasksExperimentEnabledP2(enablePhase2);

        mAwInit.setStartupTasksYieldToNativeExperimentEnabled(enableYieldToNative);
        AwBrowserMainParts.setWebViewStartupTasksYieldToNativeIsEnabled(enableYieldToNative);
    }

    /* package */ static void checkStorageIsNotDeviceProtected(Context context) {
        // The PAC processor service uses WebViewFactoryProvider.getPacProcessor() to
        // get the JS engine it needs to run PAC scripts. It doesn't use the rest of
        // WebView and this use case does not really store any meaningful data in the
        // WebView data directory, but the PAC service needs to be able to run before
        // the device is unlocked so that other apps running in that state can make
        // proxy lookups. So, we just skip the check for it and don't care whether it
        // is using DE or CE storage.
        if ("com.android.pacprocessor".equals(context.getPackageName())) {
            return;
        }

        if (context.isDeviceProtectedStorage()) {
            throw new IllegalArgumentException(
                    "WebView cannot be used with device protected storage");
        }
    }

    /**
     * Compare two WebView provider versionCodes to see if the current version is an older Chromium
     * branch than the last-used version.
     *
     * @return true if the branch portion of currentVersion is lower than the branch portion of
     *     lastVersion.
     */
    private static boolean isBranchDowngrade(int currentVersion, int lastVersion) {
        // The WebView versionCode is 9 decimal digits "BBBBPPPXX":
        // BBBB: 4 digit branch number. It monotonically increases over time.
        // PPP: patch number in the branch. It is padded with zeroes to the left.
        // XX: differentiates different architectures/build types/etc.
        int currentBranch = currentVersion / 100000;
        int lastBranch = lastVersion / 100000;

        return currentBranch < lastBranch;
    }

    private static void deleteContents(File dir) {
        File[] files = dir.listFiles();
        if (files != null) {
            for (File file : files) {
                if (file.isDirectory()) {
                    deleteContents(file);
                }
                if (!file.delete()) {
                    Log.w(TAG, "Failed to delete " + file);
                }
            }
        }
    }

    public static boolean preloadInZygote() {
        for (String library : NativeLibraries.LIBRARIES) {
            System.loadLibrary(library);
        }
        return true;
    }

    SharedPreferences getWebViewPrefs() {
        return mWebViewPrefs;
    }

    // Should be called only after initialize()
    @Override
    public Statics getStatics() {
        assert mStaticsAdapter != null;
        return mStaticsAdapter;
    }

    @Override
    public WebViewProvider createWebView(WebView webView, WebView.PrivateAccess privateAccess) {
        return new WebViewChromium(this, webView, privateAccess);
    }

    /**
     * Returns the cached SafeMode state. This must only be called after initialize(), which is when
     * the SafeMode state is cached.
     */
    public boolean isSafeModeEnabled() {
        return mIsSafeModeEnabled;
    }

    /**
     * @return true if WebView is running in multiprocess mode.
     */
    boolean isMultiProcessEnabled() {
        return mIsMultiProcessEnabled;
    }

    @Override
    public GeolocationPermissions getGeolocationPermissions() {
        try (DualTraceEvent event =
                DualTraceEvent.scoped("WebView.APICall.Framework.GET_GEOLOCATION_PERMISSIONS")) {
            SharedStatics.recordStaticApiCall(ApiCall.GET_GEOLOCATION_PERMISSIONS);
            return mAwInit.getDefaultProfile(CallSite.GET_DEFAULT_GEOLOCATION_PERMISSIONS)
                    .getGeolocationPermissions();
        }
    }

    @Override
    public CookieManager getCookieManager() {
        return mAwInit.getDefaultCookieManager();
    }

    @Override
    public ServiceWorkerController getServiceWorkerController() {
        return mAwInit.getDefaultProfile(CallSite.GET_DEFAULT_SERVICE_WORKER_CONTROLLER)
                .getServiceWorkerController();
    }

    @Override
    public TokenBindingService getTokenBindingService() {
        return null;
    }

    @Override
    public android.webkit.WebIconDatabase getWebIconDatabase() {
        return mAwInit.getWebIconDatabase();
    }

    @Override
    public WebStorage getWebStorage() {
        return mAwInit.getDefaultProfile(CallSite.GET_DEFAULT_WEB_STORAGE).getWebStorage();
    }

    @Override
    public WebViewDatabase getWebViewDatabase(final Context context) {
        return mAwInit.getDefaultWebViewDatabase(context);
    }

    WebViewDelegate getWebViewDelegate() {
        return mWebViewDelegate;
    }

    WebViewContentsClientAdapter createWebViewContentsClientAdapter(
            WebView webView, Context context) {
        try (DualTraceEvent e =
                DualTraceEvent.scoped(
                        "WebViewChromiumFactoryProvider.insideCreateWebViewContentsClientAdapter")) {
            return new WebViewContentsClientAdapter(webView, context, mWebViewDelegate);
        }
    }

    WebViewChromiumAwInit getAwInit() {
        return mAwInit;
    }

    @Override
    public TracingController getTracingController() {
        mAwInit.triggerAndWaitForChromiumStarted(
                WebViewChromiumAwInit.CallSite.GET_TRACING_CONTROLLER);
        synchronized (mAwInit.getLazyInitLock()) {
            if (mTracingController == null) {
                mTracingController =
                        new TracingControllerAdapter(
                                new SharedTracingControllerAdapter(
                                        mAwInit.getRunQueue(), mAwInit.getAwTracingController()));
            }
            return mTracingController;
        }
    }

    private static class FilteredClassLoader extends ClassLoader {
        FilteredClassLoader(ClassLoader delegate) {
            super(delegate);
        }

        @Override
        protected Class<?> findClass(String name) throws ClassNotFoundException {
            final String message =
                    "This ClassLoader should only be used for the androidx.webkit support library";

            if (name == null) {
                throw new ClassNotFoundException(message);
            }

            // We only permit this ClassLoader to load classes required for support library
            // reflection, as applications should not use this for any other purpose. So, we permit
            // anything in the support_lib_glue and support_lib_boundary packages (and their
            // subpackages).
            if (name.startsWith(SUPPORT_LIB_GLUE_AND_BOUNDARY_INTERFACE_PREFIX)) {
                return super.findClass(name);
            }

            throw new ClassNotFoundException(message);
        }
    }

    @Override
    public ClassLoader getWebViewClassLoader() {
        return new FilteredClassLoader(WebViewChromiumFactoryProvider.class.getClassLoader());
    }

    @RequiresApi(Build.VERSION_CODES.R)
    @Override
    public PacProcessor getPacProcessor() {
        return GlueApiHelperForR.getPacProcessor();
    }

    @RequiresApi(Build.VERSION_CODES.R)
    @Override
    public PacProcessor createPacProcessor() {
        return GlueApiHelperForR.createPacProcessor();
    }

    public InitInfo getInitInfo() {
        return mInitInfo;
    }

    private boolean shouldEnableContextExperiment(Context ctx) {
        // Command line switch overrides all other conditions.
        if (CommandLine.getInstance().hasSwitch(AwSwitches.WEBVIEW_USE_SEPARATE_RESOURCE_CONTEXT)) {
            return true;
        }

        // Don't enable on V+.
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            return !isRegisterResourcePathsAvailable();
        }

        // Allow the developer to opt in or opt out of the experiment.
        ManifestMetadataUtil.ensureMetadataCacheInitialized(ctx);
        Boolean valueFromManifest = ManifestMetadataUtil.shouldEnableContextExperiment();
        if (valueFromManifest != null) {
            return valueFromManifest;
        }

        return true;
    }

    @Retention(RetentionPolicy.SOURCE)
    @IntDef({ResourcePathsApi.DISABLED, ResourcePathsApi.ENABLED, ResourcePathsApi.ERROR})
    private @interface ResourcePathsApi {
        int DISABLED = 0;
        int ENABLED = 1;
        int ERROR = 2;
        int NUM_ENTRIES = 3;
    }

    /** Returns whether the registerResourcePaths API is available to use. */
    @RequiresApi(Build.VERSION_CODES.VANILLA_ICE_CREAM)
    private boolean isRegisterResourcePathsAvailable() {
        if (Build.VERSION.SDK_INT == Build.VERSION_CODES.VANILLA_ICE_CREAM) {
            try {
                long before = SystemClock.uptimeMillis();
                Properties properties = DeviceConfig.getProperties("resource_manager");
                boolean isEnabled =
                        properties.getBoolean("android.content.res.register_resource_paths", false);
                long after = SystemClock.uptimeMillis();
                RecordHistogram.recordEnumeratedHistogram(
                        REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME,
                        isEnabled ? ResourcePathsApi.ENABLED : ResourcePathsApi.DISABLED,
                        ResourcePathsApi.NUM_ENTRIES);
                RecordHistogram.recordTimesHistogram(
                        REGISTER_RESOURCE_PATHS_TIMES_HISTOGRAM_NAME, after - before);
                return isEnabled;
            } catch (Exception e) {
                RecordHistogram.recordEnumeratedHistogram(
                        REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME,
                        ResourcePathsApi.ERROR,
                        ResourcePathsApi.NUM_ENTRIES);
                // Default to pre-V workaround if we error checking the flag value.
                return false;
            }
        } else if (Build.VERSION.SDK_INT == Build.VERSION_CODES.BAKLAVA) {
            try {
                long before = SystemClock.uptimeMillis();
                boolean isEnabled =
                        AconfigPackage.load("android.content.res")
                                .getBooleanFlagValue("register_resource_paths", false);
                long after = SystemClock.uptimeMillis();
                RecordHistogram.recordEnumeratedHistogram(
                        REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME,
                        isEnabled ? ResourcePathsApi.ENABLED : ResourcePathsApi.DISABLED,
                        ResourcePathsApi.NUM_ENTRIES);
                RecordHistogram.recordTimesHistogram(
                        REGISTER_RESOURCE_PATHS_TIMES_HISTOGRAM_NAME, after - before);
                return isEnabled;
            } catch (Exception e) {
                RecordHistogram.recordEnumeratedHistogram(
                        REGISTER_RESOURCE_PATHS_HISTOGRAM_NAME,
                        ResourcePathsApi.ERROR,
                        ResourcePathsApi.NUM_ENTRIES);
                // Default to pre-V workaround if we error checking the flag value.
                return false;
            }
        }
        // On newer OS versions, registerResourcePaths will always be available.
        return true;
    }

    // These values are persisted to logs. Entries should not be renumbered and
    // numeric values should never be reused.
    //
    // LINT.IfChange(UidType)
    @IntDef({
        UidType.ROOT,
        UidType.SYSTEM,
        UidType.PHONE,
        UidType.NFC,
        UidType.BLUETOOTH,
        UidType.WIFI,
        UidType.SHELL,
        UidType.OTHER_NON_APP
    })
    private @interface UidType {
        int ROOT = 0;
        int SYSTEM = 1;
        int PHONE = 2;
        int NFC = 3;
        int BLUETOOTH = 4;
        int WIFI = 5;
        int SHELL = 6;
        int OTHER_NON_APP = 7;
        int COUNT = 8;
    }

    // LINT.ThenChange(//tools/metrics/histograms/metadata/android/enums.xml:AndroidUidType)

    private static void recordNonAppUid(@UidType int uidType) {
        RecordHistogram.recordEnumeratedHistogram(
                "Android.WebView.NonAppUid", uidType, UidType.COUNT);
    }

    @RequiresApi(Build.VERSION_CODES.R)
    private static void checkProcessUid() {
        int appId = UserHandle.getAppId(Process.myUid());
        switch (appId) {
            case Process.ROOT_UID:
                recordNonAppUid(UidType.ROOT);
                break;
            case Process.SYSTEM_UID:
                recordNonAppUid(UidType.SYSTEM);
                break;
            case Process.PHONE_UID:
                recordNonAppUid(UidType.PHONE);
                break;
            case 1027 /* Process.NFC_UID */:
                recordNonAppUid(UidType.NFC);
                break;
            case Process.BLUETOOTH_UID:
                recordNonAppUid(UidType.BLUETOOTH);
                break;
            case Process.WIFI_UID:
                recordNonAppUid(UidType.WIFI);
                break;
            case Process.SHELL_UID:
                recordNonAppUid(UidType.SHELL);
                break;
            default:
                if (appId < Process.FIRST_APPLICATION_UID) {
                    recordNonAppUid(UidType.OTHER_NON_APP);
                }
                break;
        }
    }
}
