// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.app.Application;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.net.Uri;
import android.os.Build;
import android.os.Looper;
import android.os.Process;
import android.os.SystemClock;
import android.os.UserHandle;
import android.os.UserManager;
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

import org.chromium.android_webview.ApkType;
import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserMainParts;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwCookieManager;
import org.chromium.android_webview.AwServiceWorkerController;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.ManifestMetadataUtil;
import org.chromium.android_webview.R;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.AwFeatures;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.FlagOverrideHelper;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.safe_mode.BrowserSafeModeActionList;
import org.chromium.android_webview.safe_mode.DisableStartupTasksSafeModeAction;
import org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction;
import org.chromium.base.BuildInfo;
import org.chromium.base.BundleUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.TraceEvent;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.base.version_info.VersionConstants;
import org.chromium.blink_public.common.BlinkFeatures;
import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.content_public.browser.LGEmailActionModeWorkaround;

import java.io.File;
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
 * <ul>
 *
 * <li>On API 21 (no longer supported), the platform invoked a parameterless constructor. Since we
 * didn't have a WebViewDelegate instance, this required us to invoke WebViewDelegate methods via
 * reflection. This constructor has been removed from the code as we no longer support Android
 * 21.</li>
 *
 * <li>From API 22 through API 25, the platform instead directly calls the constructor with a
 * WebViewDelegate parameter (See internal CL http://ag/577188 or the public AOSP cherrypick
 * https://r.android.com/114870). API 22 (no longer supported) would fallback to the
 * parameterless constructor if the first constructor call throws an exception, however this
 * fallback was removed in API 23.</li>
 *
 * <li>Starting in API 26, the platform calls {@link #create} instead of calling the constructor
 * directly (see internal CLs http://ag/1334128 and http://ag/1846560).</li>
 *
 * <li>From API 27 onward, the platform code is updated during each release to use the {@code
 * WebViewChromiumFactoryProviderForX} subclass, where "X" is replaced by the actual platform API
 * version (ex. "ForOMR1"). It still invokes the {@link #create} method on the subclass. While the
 * OS version is still under development, the "ForX" subclass implements the new platform APIs (in a
 * private codebase). Once the APIs for that version have been finalized, we eventually roll these
 * implementations into this class and the "ForX" subclass just calls directly into this
 * implementation.</li>
 *
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

    // This is an ID hardcoded by WebLayer for resources stored in locale splits. See
    // WebLayerImpl.java for more info.
    private static final int SHARED_LIBRARY_MAX_ID = 36;

    /**
     * This holds objects of classes that are defined in P and above to ensure that run-time class
     * verification does not occur until it is actually used for P and above.
     */
    @RequiresApi(Build.VERSION_CODES.P)
    private static class ObjectHolderForP {
        public TracingController mTracingController;
    }

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
     * Class that takes care of chromium lazy initialization.
     * This is package-public so that a downstream subclass can access it.
     */
    /* package */ WebViewChromiumAwInit mAwInit;

    private SharedPreferences mWebViewPrefs;
    private WebViewDelegate mWebViewDelegate;

    protected boolean mShouldDisableThreadChecking;

    @GuardedBy("mAwInit.getLazyInitLock()")
    private Statics mStaticsAdapter;

    @GuardedBy("mAwInit.getLazyInitLock()")
    private ServiceWorkerController mServiceWorkerController;

    private boolean mIsSafeModeEnabled;

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

    @GuardedBy("mAwInit.getLazyInitLock()")
    @RequiresApi(Build.VERSION_CODES.P)
    private final ObjectHolderForP mObjectHolderForP =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ? new ObjectHolderForP() : null;

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
        try (ScopedSysTraceEvent e2 =
                ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.createAwInit")) {
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
        try (ScopedSysTraceEvent e2 =
                ScopedSysTraceEvent.scoped(
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

    @SuppressWarnings({"NoContextGetApplicationContext", "DiscouragedApi"})
    private void initialize(WebViewDelegate webViewDelegate) {
        mInitInfo.mStartTime = SystemClock.uptimeMillis();
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

            mAwInit = createAwInit();
            if (Looper.myLooper() == Looper.getMainLooper()) {
                mAwInit.setProviderInitOnMainLooperLocation(
                        new Throwable(
                                "Location where WebViewChromiumFactoryProvider init was"
                                        + " started on the Android main looper"));
            }
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

            try (ScopedSysTraceEvent e2 =
                    ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.initCommandLine")) {
                // This may take ~20 ms only on userdebug devices.
                CommandLineUtil.initCommandLine();
            }

            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                // Since N, getSharedPreferences creates the preference dir if it doesn't exist,
                // causing a disk write.
                mWebViewPrefs = ctx.getSharedPreferences(CHROMIUM_PREFS_NAME, Context.MODE_PRIVATE);
                WebViewCachedFlags.init(mWebViewPrefs);
            }

            if (shouldEnableContextExperiment(ctx)) {
                try {
                    Context override =
                            ctx.createPackageContext(
                                    packageInfo.packageName,
                                    Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY);
                    // Use standard Android theme for standalone WebView, use custom theme for
                    // everything else. Check package id of the theme resource to determine.
                    boolean isStandaloneWebView =
                            (override.getResources()
                                                    .getIdentifier(
                                                            "WebViewBaseTheme",
                                                            "style",
                                                            packageInfo.packageName)
                                            & 0xff000000)
                                    != 0x7f000000;
                    ClassLoaderContextWrapperFactory.setOverrideInfo(
                            packageInfo.packageName,
                            isStandaloneWebView || Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                                    ? android.R.style.Theme_DeviceDefault_DayNight
                                    : R.style.WebViewBaseTheme,
                            Context.CONTEXT_INCLUDE_CODE | Context.CONTEXT_IGNORE_SECURITY);
                    // Use this to report the actual state of the feature at runtime.
                    AwBrowserMainParts.setUseWebViewContext(true);
                } catch (PackageManager.NameNotFoundException e) {
                    Log.e(TAG, "Could not get resource override context.");
                }
            }

            // WebView needs to make sure to always use the wrapped application context.
            ctx = ClassLoaderContextWrapperFactory.get(ctx);
            ContextUtils.initApplicationContext(ctx);

            // Ensuring we set this before we might read it in any future calls to BuildInfo.
            // BuildInfo requires ContextUtils' application context, so this has to happen after.
            BuildInfo.setBrowserPackageInfo(packageInfo);
            // Trigger the creation of the BuildInfo singleton to avoid potential issues reading
            // the command line if this happens on another thread.
            BuildInfo.getInstance();

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
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q
                    && AwBrowserProcess.getApkType() != ApkType.TRICHROME
                    && packageId > SHARED_LIBRARY_MAX_ID) {
                throw new RuntimeException("Package ID too high for WebView: " + packageId);
            }

            mAwInit.setUpResourcesOnBackgroundThread(packageId, ctx);

            AndroidXProcessGlobalConfig.extractConfigFromApp(application.getClassLoader());

            // Limiting scope of the command line switch object before it is passed to native.
            // The reference to `cl` eventually becomes a stale object, causing incorrect behavior,
            // since Java switches are incongruent with Native switches.
            {
                CommandLine cl = CommandLine.getInstance();

                boolean multiProcess = webViewDelegate.isMultiProcessEnabled();
                if (multiProcess) {
                    cl.appendSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
                }
                Log.i(
                        TAG,
                        "version=%s (%s) minSdkVersion=%s isBundle=%s multiprocess=%s packageId=%s",
                        VersionConstants.PRODUCT_VERSION,
                        BuildConfig.VERSION_CODE,
                        BuildConfig.MIN_SDK_VERSION,
                        BundleUtils.isBundle(),
                        multiProcess,
                        packageId);

                // Enable modern SameSite cookie behavior if the app targets at least S.
                if (ctx.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.S) {
                    cl.appendSwitch(AwSwitches.WEBVIEW_ENABLE_MODERN_COOKIE_SAME_SITE);
                }

                // Enable logging JS console messages in system logs only if the app is debuggable
                // or
                // it's a debuggable android build.
                if (BuildInfo.isDebugAndroidOrApp()) {
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
                try {
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
                try (ScopedSysTraceEvent e2 =
                        ScopedSysTraceEvent.scoped(
                                "WebViewChromiumFactoryProvider.loadChromiumLibrary")) {
                    String dataDirectoryBasePath = androidXConfig.getDataDirectoryBasePathOrNull();
                    String cacheDirectoryBasePath =
                            androidXConfig.getCacheDirectoryBasePathOrNull();
                    String dataDirectorySuffix;
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        dataDirectorySuffix =
                                GlueApiHelperForP.getDataDirectorySuffix(webViewDelegate);
                    } else {
                        // Try the AndroidX library version
                        dataDirectorySuffix = androidXConfig.getDataDirectorySuffixOrNull();
                    }
                    AwBrowserProcess.loadLibrary(
                            dataDirectoryBasePath, cacheDirectoryBasePath, dataDirectorySuffix);
                }

                try (ScopedSysTraceEvent e2 =
                        ScopedSysTraceEvent.scoped(
                                "WebViewChromiumFactoryProvider.loadGlueLayerPlatSupportLibrary")) {
                    System.loadLibrary("webviewchromium_plat_support");
                }

                deleteContentsOnPackageDowngrade(packageInfo);
            }

            boolean partitionedCookies =
                    androidXConfig.getPartitionedCookiesEnabled() == null
                            ? !WebViewCachedFlags.get()
                                    .isCachedFeatureEnabled(AwFeatures.WEBVIEW_DISABLE_CHIPS)
                            : androidXConfig.getPartitionedCookiesEnabled();
            // We use this to report the state of our partitioned override experiment if set.
            // Applying this after the override of the Android X API has potentially been set
            // otherwise our metrics could be misleading.
            AwBrowserMainParts.setPartitionedCookiesDefaultState(partitionedCookies);
            if (!partitionedCookies) {
                AwCookieManager.disablePartitionedCookiesGlobal();
                Log.d(TAG, "CHIPS Disabled");
            } else {
                Log.d(TAG, "CHIPS Enabled");
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

            // This check must happen after pref value has been read and SafeMode setup has
            // completed.
            boolean enableStartupTasksExperiment = shouldEnableStartupTasksExperiment();
            mAwInit.setStartupTaskExperimentEnabled(enableStartupTasksExperiment);
            AwBrowserMainParts.setWebViewStartupTasksLogicIsEnabled(enableStartupTasksExperiment);

            if (!FastVariationsSeedSafeModeAction.hasRun()) {
                mAwInit.startVariationsInit();
            }

            mShouldDisableThreadChecking = shouldDisableThreadChecking(ctx);

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
            mInitInfo.mTotalFactoryInitStartTime =
                    mWebViewDelegate.getStartupTimestamps().getWebViewLoadStart();
            mInitInfo.mTotalFactoryInitDuration =
                    SystemClock.uptimeMillis() - mInitInfo.mTotalFactoryInitStartTime;
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.Startup.CreationTime.TotalFactoryInitTime",
                    mInitInfo.mTotalFactoryInitDuration);
        }
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
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.P && BundleUtils.isBundle()) {
            // Apply workaround if we're a bundle on O, where the split APK handling bug exists.
            SplitApkWorkaround.apply();
        }

        for (String library : NativeLibraries.LIBRARIES) {
            System.loadLibrary(library);
        }
        return true;
    }

    SharedPreferences getWebViewPrefs() {
        return mWebViewPrefs;
    }

    @Override
    public Statics getStatics() {
        SharedStatics sharedStatics = mAwInit.getStatics();
        synchronized (mAwInit.getLazyInitLock()) {
            if (mStaticsAdapter == null) {
                mStaticsAdapter =
                        new WebViewChromiumFactoryProvider.Statics() {
                            @Override
                            public String findAddress(String addr) {
                                return sharedStatics.findAddress(addr);
                            }

                            @Override
                            public String getDefaultUserAgent(Context context) {
                                return sharedStatics.getDefaultUserAgent(context);
                            }

                            @Override
                            public void setWebContentsDebuggingEnabled(boolean enable) {
                                sharedStatics.setWebContentsDebuggingEnabled(enable);
                            }

                            @Override
                            public void clearClientCertPreferences(Runnable onCleared) {
                                sharedStatics.clearClientCertPreferences(onCleared);
                            }

                            @Override
                            public void freeMemoryForTests() {
                                sharedStatics.freeMemoryForTests();
                            }

                            @Override
                            public void enableSlowWholeDocumentDraw() {
                                sharedStatics.enableSlowWholeDocumentDraw();
                            }

                            @Override
                            public Uri[] parseFileChooserResult(int resultCode, Intent intent) {
                                return sharedStatics.parseFileChooserResult(resultCode, intent);
                            }

                            @Override
                            public void initSafeBrowsing(
                                    Context context, ValueCallback<Boolean> callback) {
                                sharedStatics.initSafeBrowsing(
                                        context, CallbackConverter.fromValueCallback(callback));
                            }

                            @Override
                            public void setSafeBrowsingWhitelist(
                                    List<String> urls, ValueCallback<Boolean> callback) {
                                sharedStatics.setSafeBrowsingAllowlist(
                                        urls, CallbackConverter.fromValueCallback(callback));
                            }

                            @Override
                            public Uri getSafeBrowsingPrivacyPolicyUrl() {
                                return sharedStatics.getSafeBrowsingPrivacyPolicyUrl();
                            }

                            @SuppressWarnings("UnusedMethod")
                            public boolean isMultiProcessEnabled() {
                                return sharedStatics.isMultiProcessEnabled();
                            }

                            @SuppressWarnings("UnusedMethod")
                            public String getVariationsHeader() {
                                return sharedStatics.getVariationsHeader();
                            }
                        };
            }
            return mStaticsAdapter;
        }
    }

    @Override
    public WebViewProvider createWebView(WebView webView, WebView.PrivateAccess privateAccess) {
        return new WebViewChromium(this, webView, privateAccess, mShouldDisableThreadChecking);
    }

    // Workaround for IME thread crashes on legacy OEM apps.
    private boolean shouldDisableThreadChecking(Context context) {
        String appName = context.getPackageName();
        int versionCode = PackageUtils.getPackageVersion(appName);
        int appTargetSdkVersion = context.getApplicationInfo().targetSdkVersion;
        if (versionCode == -1) return false;

        boolean shouldDisable = false;

        // crbug.com/651706
        final String lgeMailPackageId = "com.lge.email";
        if (lgeMailPackageId.equals(appName)) {
            if (appTargetSdkVersion > Build.VERSION_CODES.N) return false;
            if (LGEmailActionModeWorkaround.isSafeVersion(versionCode)) return false;
            shouldDisable = true;
        }

        // crbug.com/655759
        // Also want to cover ".att" variant suffix package name.
        final String yahooMailPackageId = "com.yahoo.mobile.client.android.mail";
        if (appName.startsWith(yahooMailPackageId)) {
            if (appTargetSdkVersion > Build.VERSION_CODES.M) return false;
            if (versionCode > 1315850) return false;
            shouldDisable = true;
        }

        // crbug.com/622151
        final String htcMailPackageId = "com.htc.android.mail";
        if (htcMailPackageId.equals(appName)) {
            if (appTargetSdkVersion > Build.VERSION_CODES.M) return false;
            // This value is provided by HTC.
            if (versionCode >= 866001861) return false;
            shouldDisable = true;
        }

        if (shouldDisable) {
            Log.w(
                    TAG,
                    "Disabling thread check in WebView. "
                            + "APK name: "
                            + appName
                            + ", versionCode: "
                            + versionCode
                            + ", targetSdkVersion: "
                            + appTargetSdkVersion);
        }
        return shouldDisable;
    }

    /**
     * Returns the cached SafeMode state. This must only be called after initialize(), which is when
     * the SafeMode state is cached.
     */
    public boolean isSafeModeEnabled() {
        return mIsSafeModeEnabled;
    }

    @Override
    public GeolocationPermissions getGeolocationPermissions() {
        try (TraceEvent event =
                TraceEvent.scoped("WebView.APICall.Framework.GET_GEOLOCATION_PERMISSIONS")) {
            SharedStatics.recordStaticApiCall(ApiCall.GET_GEOLOCATION_PERMISSIONS);
            return mAwInit.getDefaultGeolocationPermissions();
        }
    }

    @Override
    public CookieManager getCookieManager() {
        return mAwInit.getDefaultCookieManager();
    }

    @Override
    public ServiceWorkerController getServiceWorkerController() {
        AwServiceWorkerController serviceWorkerController =
                mAwInit.getDefaultServiceWorkerController();
        synchronized (mAwInit.getLazyInitLock()) {
            if (mServiceWorkerController == null) {
                mServiceWorkerController =
                        new ServiceWorkerControllerAdapter(serviceWorkerController);
            }
            return mServiceWorkerController;
        }
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
        return mAwInit.getDefaultWebStorage();
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
        try (ScopedSysTraceEvent e =
                ScopedSysTraceEvent.scoped(
                        "WebViewChromiumFactoryProvider.insideCreateWebViewContentsClientAdapter")) {
            return new WebViewContentsClientAdapter(webView, context, mWebViewDelegate);
        }
    }

    void startYourEngines(boolean onMainThread) {
        try (ScopedSysTraceEvent e1 =
                ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.startYourEngines")) {
            mAwInit.startYourEngines(onMainThread);
        }
    }

    boolean isChromiumInitialized() {
        return mAwInit.isChromiumInitialized();
    }

    // Only on UI thread.
    AwBrowserContext getDefaultBrowserContextOnUiThread() {
        return mAwInit.getDefaultBrowserContextOnUiThread();
    }

    WebViewChromiumAwInit getAwInit() {
        return mAwInit;
    }

    @RequiresApi(Build.VERSION_CODES.P)
    @Override
    public TracingController getTracingController() {
        mAwInit.triggerAndWaitForChromiumStarted(
                true, WebViewChromiumAwInit.CallSite.GET_TRACING_CONTROLLER);
        synchronized (mAwInit.getLazyInitLock()) {
            if (mObjectHolderForP.mTracingController == null) {
                mObjectHolderForP.mTracingController =
                        GlueApiHelperForP.createTracingControllerAdapter(this, mAwInit);
            }
            return mObjectHolderForP.mTracingController;
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
            return false;
        }

        // Allow the developer to opt in or opt out of the experiment.
        ManifestMetadataUtil.ensureMetadataCacheInitialized(ctx);
        Boolean valueFromManifest = ManifestMetadataUtil.shouldEnableContextExperiment();
        if (valueFromManifest != null) {
            return valueFromManifest;
        }

        // We also want to enable by default on the listed package names.
        if ("com.aurora.launcher".equals(ctx.getPackageName())
                || "com.qiku.android.launcher3".equals(ctx.getPackageName())) {
            return true;
        }

        return WebViewCachedFlags.get()
                .isCachedFeatureEnabled(AwFeatures.WEBVIEW_SEPARATE_RESOURCE_CONTEXT);
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
