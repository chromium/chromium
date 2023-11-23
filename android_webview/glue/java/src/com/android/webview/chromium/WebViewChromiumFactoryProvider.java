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
import android.os.SystemClock;
import android.os.UserManager;
import android.provider.Settings;
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

import androidx.annotation.RequiresApi;

import org.chromium.android_webview.ApkType;
import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContentsStatics;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.BrowserSafeModeActionList;
import org.chromium.android_webview.ProductConfig;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.AwSwitches;
import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.android_webview.common.DeveloperModeUtils;
import org.chromium.android_webview.common.FlagOverrideHelper;
import org.chromium.android_webview.common.Lifetime;
import org.chromium.android_webview.common.ProductionSupportedFlagList;
import org.chromium.android_webview.common.SafeModeController;
import org.chromium.android_webview.variations.FastVariationsSeedSafeModeAction;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.build.BuildConfig;
import org.chromium.build.NativeLibraries;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.components.version_info.VersionConstants;
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
    // Used to indicate if WebLayer and WebView are running in the same process.
    private static boolean sWebLayerRunningInSameProcess;

    private final WebViewChromiumRunQueue mRunQueue =
            new WebViewChromiumRunQueue(
                    () -> {
                        return WebViewChromiumFactoryProvider.this.mAwInit.hasStarted();
                    });

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

    // Initialization guarded by mAwInit.getLock()
    private Statics mStaticsAdapter;

    private boolean mIsSafeModeEnabled;

    private ServiceWorkerController mServiceWorkerController;

    public class InitInfo {
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
    ;

    private InitInfo mInitInfo = new InitInfo();

    @RequiresApi(Build.VERSION_CODES.P)
    private ObjectHolderForP mObjectHolderForP =
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

    // Protected to allow downstream to override.
    protected WebViewChromiumAwInit createAwInit() {
        try (ScopedSysTraceEvent e2 =
                ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.createAwInit")) {
            return new WebViewChromiumAwInit(this);
        }
    }

    // Protected to allow downstream to override.
    protected ContentSettingsAdapter createContentSettingsAdapter(AwSettings settings) {
        return new ContentSettingsAdapter(settings);
    }

    private void deleteContentsOnPackageDowngrade(PackageInfo packageInfo) {
        try (ScopedSysTraceEvent e2 =
                ScopedSysTraceEvent.scoped(
                        "WebViewChromiumFactoryProvider.deleteContentsOnPackageDowngrade")) {
            // Use shared preference to check for package downgrade.
            // Since N, getSharedPreferences creates the preference dir if it doesn't exist,
            // causing a disk write.
            mWebViewPrefs =
                    ContextUtils.getApplicationContext()
                            .getSharedPreferences(CHROMIUM_PREFS_NAME, Context.MODE_PRIVATE);
            int lastVersion = mWebViewPrefs.getInt(VERSION_CODE_PREF, 0);
            int currentVersion = packageInfo.versionCode;
            if (!versionCodeGE(currentVersion, lastVersion)) {
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

    /** This must not be called until {@link #initialize(WebViewDelegate)} has set mWebViewDelegate. */
    public void addWebViewAssetPath(Context ctx) {
        mWebViewDelegate.addWebViewAssetPath(ctx);
    }

    @SuppressWarnings("NoContextGetApplicationContext")
    private void initialize(WebViewDelegate webViewDelegate) {
        mInitInfo.mStartTime = SystemClock.uptimeMillis();
        try (ScopedSysTraceEvent e1 =
                ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.initialize")) {
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

            // WebView needs to make sure to always use the wrapped application context.
            ctx = ClassLoaderContextWrapperFactory.get(ctx);
            ContextUtils.initApplicationContext(ctx);

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
            } catch (RuntimeException e) {
                // We failed to find the package ID, which likely means this context's AssetManager
                // doesn't have WebView loaded in it. This may be because WebViewFactory doesn't add
                // the package persistently to ResourcesManager and the app's AssetManager has been
                // recreated. Try adding it again using WebViewDelegate, which does add it
                // persistently.
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

            try (ScopedSysTraceEvent e2 =
                    ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.initCommandLine")) {
                // This may take ~20 ms only on userdebug devices.
                CommandLineUtil.initCommandLine();
            }

            boolean multiProcess = false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                // Ask the system if multiprocess should be enabled on O+.
                multiProcess = GlueApiHelperForO.isMultiProcessEnabled(webViewDelegate);
            } else {
                // Check the multiprocess developer setting directly on N.
                multiProcess =
                        Settings.Global.getInt(
                                        ctx.getContentResolver(),
                                        Settings.Global.WEBVIEW_MULTIPROCESS,
                                        0)
                                == 1;
            }
            if (multiProcess) {
                CommandLine cl = CommandLine.getInstance();
                cl.appendSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
            }
            // Using concatenation rather than %s to allow values to be inlined by R8.
            Log.i(
                    TAG,
                    "Loaded version="
                            + VersionConstants.PRODUCT_VERSION
                            + " minSdkVersion="
                            + BuildConfig.MIN_SDK_VERSION
                            + " isBundle="
                            + ProductConfig.IS_BUNDLE
                            + " multiprocess=%s packageId=%s",
                    multiProcess,
                    packageId);

            // Enable modern SameSite cookie behavior if the app targets at least S.
            if (ctx.getApplicationInfo().targetSdkVersion >= Build.VERSION_CODES.S) {
                CommandLine cl = CommandLine.getInstance();
                cl.appendSwitch(AwSwitches.WEBVIEW_ENABLE_MODERN_COOKIE_SAME_SITE);
            }

            // Enable logging JS console messages in system logs only if the app is debuggable or
            // it's a debuggable android build.
            if (BuildInfo.isDebugAndroidOrApp()) {
                CommandLine cl = CommandLine.getInstance();
                cl.appendSwitch(AwSwitches.WEBVIEW_LOG_JS_CONSOLE_MESSAGES);
            }

            String webViewPackageName = AwBrowserProcess.getWebViewPackageName();
            long developerModeStart = SystemClock.elapsedRealtime();
            boolean isDeveloperModeEnabled =
                    DeveloperModeUtils.isDeveloperModeEnabled(webViewPackageName);
            long developerModeEnd = SystemClock.elapsedRealtime();
            RecordHistogram.recordTimesHistogram(
                    "Android.WebView.DevUi.DeveloperModeBlockingTime",
                    developerModeEnd - developerModeStart);
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

            ThreadUtils.setWillOverrideUiThread();
            BuildInfo.setBrowserPackageInfo(packageInfo);
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

            if (!FastVariationsSeedSafeModeAction.hasRun()) {
                mAwInit.startVariationsInit();
            }

            mShouldDisableThreadChecking = shouldDisableThreadChecking(ctx);

            setSingleton(this);

            // sWebLayerRunningInSameProcess may have been set before initialize().
            if (sWebLayerRunningInSameProcess) {
                addTask(
                        () -> {
                            getDefaultBrowserContextOnUiThread().setWebLayerRunningInSameProcess();
                        });
            }
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
        if (context.isDeviceProtectedStorage()) {
            throw new IllegalArgumentException(
                    "WebView cannot be used with device protected storage");
        }
    }

    /**
     * Both versionCodes should be from a WebView provider package implemented by Chromium.
     * VersionCodes from other kinds of packages won't make any sense in this method.
     *
     * An introduction to Chromium versionCode scheme:
     * "BBBBPPPAX"
     * BBBB: 4 digit branch number. It monotonically increases over time.
     * PPP: patch number in the branch. It is padded with zeroes to the left. These three digits may
     * change their meaning in the future.
     * A: architecture digit.
     * X: A digit to differentiate APKs for other reasons.
     *
     * This method takes the "BBBB" of versionCodes and compare them.
     *
     * @return true if versionCode1 is higher than or equal to versionCode2.
     */
    private static boolean versionCodeGE(int versionCode1, int versionCode2) {
        int v1 = versionCode1 / 100000;
        int v2 = versionCode2 / 100000;

        return v1 >= v2;
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
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O
                && Build.VERSION.SDK_INT < Build.VERSION_CODES.P
                && ProductConfig.IS_BUNDLE) {
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
        synchronized (mAwInit.getLock()) {
            SharedStatics sharedStatics = mAwInit.getStatics();
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

                            public boolean isMultiProcessEnabled() {
                                return sharedStatics.isMultiProcessEnabled();
                            }

                            public String getVariationsHeader() {
                                return sharedStatics.getVariationsHeader();
                            }
                        };
            }
        }
        return mStaticsAdapter;
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
        return mAwInit.getDefaultGeolocationPermissions();
    }

    @Override
    public CookieManager getCookieManager() {
        return mAwInit.getDefaultCookieManager();
    }

    @Override
    public ServiceWorkerController getServiceWorkerController() {
        synchronized (mAwInit.getLock()) {
            if (mServiceWorkerController == null) {
                mServiceWorkerController =
                        new ServiceWorkerControllerAdapter(
                                mAwInit.getDefaultServiceWorkerController());
            }
        }
        return mServiceWorkerController;
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

    boolean hasStarted() {
        return mAwInit.hasStarted();
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
        synchronized (mAwInit.getLock()) {
            mAwInit.ensureChromiumStartedLocked(true);
            // ensureChromiumStartedLocked() can release the lock on first call while
            // waiting for startup. Hence check the mTracingController here to ensure
            // the singleton property.
            if (mObjectHolderForP.mTracingController == null) {
                mObjectHolderForP.mTracingController =
                        GlueApiHelperForP.createTracingControllerAdapter(this, mAwInit);
            }
        }
        return mObjectHolderForP.mTracingController;
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

    // This is called from WebLayer when WebView and WebLayer are run in the same process. It's
    // used to set a crash key to help attribute crashes. It's entirely possible WebView has not
    // been initialized when this is called.
    public static void setWebLayerRunningInSameProcess() {
        // This may be called before initialize().
        synchronized (sSingletonLock) {
            sWebLayerRunningInSameProcess = true;
            if (sSingleton == null) {
                // initialize() hasn't been called yet. When initialize() is called
                // |sWebLayerRunningInSameProcess| will be checked.
                return;
            }
        }
        getSingleton()
                .addTask(
                        () -> {
                            getSingleton()
                                    .getDefaultBrowserContextOnUiThread()
                                    .setWebLayerRunningInSameProcess();
                        });
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
}
