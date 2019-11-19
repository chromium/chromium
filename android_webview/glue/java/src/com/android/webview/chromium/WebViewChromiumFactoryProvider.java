// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package com.android.webview.chromium;

import android.annotation.TargetApi;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.content.pm.PackageInfo;
import android.net.Uri;
import android.os.Build;
import android.os.SystemClock;
import android.provider.Settings;
import android.view.ViewGroup;
import android.webkit.CookieManager;
import android.webkit.GeolocationPermissions;
import android.webkit.ServiceWorkerController;
import android.webkit.TokenBindingService;
import android.webkit.TracingController;
import android.webkit.ValueCallback;
import android.webkit.WebStorage;
import android.webkit.WebView;
import android.webkit.WebViewDatabase;
import android.webkit.WebViewFactory;
import android.webkit.WebViewFactoryProvider;
import android.webkit.WebViewProvider;

import com.android.webview.chromium.WebViewDelegateFactory.WebViewDelegate;

import org.chromium.android_webview.AwAutofillProvider;
import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.AwSwitches;
import org.chromium.android_webview.WebViewChromiumRunQueue;
import org.chromium.android_webview.common.CommandLineUtil;
import org.chromium.base.BuildInfo;
import org.chromium.base.BundleUtils;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.PackageUtils;
import org.chromium.base.PathUtils;
import org.chromium.base.StrictModeContext;
import org.chromium.base.ThreadUtils;
import org.chromium.base.annotations.VerifiesOnN;
import org.chromium.base.annotations.VerifiesOnP;
import org.chromium.base.library_loader.NativeLibraries;
import org.chromium.base.metrics.CachedMetrics.TimesHistogramSample;
import org.chromium.base.metrics.ScopedSysTraceEvent;
import org.chromium.components.autofill.AutofillProvider;
import org.chromium.components.embedder_support.application.ClassLoaderContextWrapperFactory;
import org.chromium.content_public.browser.LGEmailActionModeWorkaround;

import java.io.File;
import java.util.List;
import java.util.concurrent.Callable;
import java.util.concurrent.FutureTask;

/**
 * Entry point to the WebView. The system framework talks to this class to get instances of the
 * implementation classes.
 */
@SuppressWarnings("deprecation")
public class WebViewChromiumFactoryProvider implements WebViewFactoryProvider {
    private static final String TAG = "WVCFactoryProvider";

    private static final String CHROMIUM_PREFS_NAME = "WebViewChromiumPrefs";
    private static final String VERSION_CODE_PREF = "lastVersionCodeUsed";

    private static final String SUPPORT_LIB_GLUE_AND_BOUNDARY_INTERFACE_PREFIX =
            "org.chromium.support_lib_";

    /**
     * This holds objects of classes that are defined in N and above to ensure that run-time class
     * verification does not occur until it is actually used for N and above.
     */
    @TargetApi(Build.VERSION_CODES.N)
    @VerifiesOnN
    private static class ObjectHolderForN {
        public ServiceWorkerController mServiceWorkerController;
    }

    /**
     * This holds objects of classes that are defined in P and above to ensure that run-time class
     * verification does not occur until it is actually used for P and above.
     */
    @TargetApi(Build.VERSION_CODES.P)
    @VerifiesOnP
    private static class ObjectHolderForP {
        public TracingController mTracingController;
    }

    private static final Object sSingletonLock = new Object();
    private static WebViewChromiumFactoryProvider sSingleton;

    private final WebViewChromiumRunQueue mRunQueue = new WebViewChromiumRunQueue(
            () -> { return WebViewChromiumFactoryProvider.this.mAwInit.hasStarted(); });

    /* package */ WebViewChromiumRunQueue getRunQueue() {
        return mRunQueue;
    }

    // We have a 4 second timeout to try to detect deadlocks to detect and aid in debuggin
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

    @TargetApi(Build.VERSION_CODES.N)
    private ObjectHolderForN mObjectHolderForN =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.N ? new ObjectHolderForN() : null;

    @TargetApi(Build.VERSION_CODES.P)
    private ObjectHolderForP mObjectHolderForP =
            Build.VERSION.SDK_INT >= Build.VERSION_CODES.P ? new ObjectHolderForP() : null;

    /**
     * Thread-safe way to set the one and only WebViewChromiumFactoryProvider.
     */
    private static void setSingleton(WebViewChromiumFactoryProvider provider) {
        synchronized (sSingletonLock) {
            if (sSingleton != null) {
                throw new RuntimeException(
                        "WebViewChromiumFactoryProvider should only be set once!");
            }
            sSingleton = provider;
        }
    }

    /**
     * Thread-safe way to get the one and only WebViewChromiumFactoryProvider.
     */
    static WebViewChromiumFactoryProvider getSingleton() {
        synchronized (sSingletonLock) {
            if (sSingleton == null) {
                throw new RuntimeException("WebViewChromiumFactoryProvider has not been set!");
            }
            return sSingleton;
        }
    }

    /**
     * Entry point for newer versions of Android.
     */
    public static WebViewChromiumFactoryProvider create(android.webkit.WebViewDelegate delegate) {
        return new WebViewChromiumFactoryProvider(delegate);
    }

    /**
     * Constructor called by the API 21 version of {@link WebViewFactory} and earlier.
     */
    public WebViewChromiumFactoryProvider() {
        initialize(WebViewDelegateFactory.createApi21CompatibilityDelegate());
    }

    /**
     * Constructor called by the API 22 version of {@link WebViewFactory} and later.
     */
    public WebViewChromiumFactoryProvider(android.webkit.WebViewDelegate delegate) {
        initialize(WebViewDelegateFactory.createProxyDelegate(delegate));
    }

    /**
     * Constructor for internal use when a proxy delegate has already been created.
     */
    WebViewChromiumFactoryProvider(WebViewDelegate delegate) {
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
        try (ScopedSysTraceEvent e2 = ScopedSysTraceEvent.scoped(
                     "WebViewChromiumFactoryProvider.deleteContentsOnPackageDowngrade")) {
            // Use shared preference to check for package downgrade.
            // Since N, getSharedPreferences creates the preference dir if it doesn't exist,
            // causing a disk write.
            mWebViewPrefs = ContextUtils.getApplicationContext().getSharedPreferences(
                    CHROMIUM_PREFS_NAME, Context.MODE_PRIVATE);
            int lastVersion = mWebViewPrefs.getInt(VERSION_CODE_PREF, 0);
            int currentVersion = packageInfo.versionCode;
            if (!versionCodeGE(currentVersion, lastVersion)) {
                // The WebView package has been downgraded since we last ran in this
                // application. Delete the WebView data directory's contents.
                String dataDir = PathUtils.getDataDirectory();
                Log.i(TAG,
                        "WebView package downgraded from " + lastVersion + " to "
                                + currentVersion + "; deleting contents of " + dataDir);
                deleteContents(new File(dataDir));
            }
            if (lastVersion != currentVersion) {
                mWebViewPrefs.edit().putInt(VERSION_CODE_PREF, currentVersion).apply();
            }
        }
    }

    @SuppressWarnings("NoContextGetApplicationContext")
    private void initialize(WebViewDelegate webViewDelegate) {
        long startTime = SystemClock.elapsedRealtime();
        try (ScopedSysTraceEvent e1 =
                        ScopedSysTraceEvent.scoped("WebViewChromiumFactoryProvider.initialize")) {
            PackageInfo packageInfo;
            try (ScopedSysTraceEvent e2 = ScopedSysTraceEvent.scoped(
                         "WebViewChromiumFactoryProvider.getLoadedPackageInfo")) {
                // The package is used to locate the services for copying crash minidumps and
                // requesting variations seeds. So it must be set before initializing variations and
                // before a renderer has a chance to crash.
                packageInfo = WebViewFactory.getLoadedPackageInfo();
            }
            AwBrowserProcess.setWebViewPackageName(packageInfo.packageName);

            mAwInit = createAwInit();
            mWebViewDelegate = webViewDelegate;
            Context ctx = webViewDelegate.getApplication().getApplicationContext();

            // If the application context is DE, but we have credentials, use a CE context instead
            try (ScopedSysTraceEvent e2 = ScopedSysTraceEvent.scoped(
                         "WebViewChromiumFactoryProvider.checkStorage")) {
                checkStorageIsNotDeviceProtected(webViewDelegate.getApplication());
            } catch (IllegalArgumentException e) {
                assert Build.VERSION.SDK_INT >= Build.VERSION_CODES.N;
                if (!GlueApiHelperForN.isUserUnlocked(ctx)) {
                    throw e;
                }
                ctx = GlueApiHelperForN.createCredentialProtectedStorageContext(ctx);
            }

            // WebView needs to make sure to always use the wrapped application context.
            ctx = ClassLoaderContextWrapperFactory.get(ctx);
            ContextUtils.initApplicationContext(ctx);

            // Find the package ID for the package that WebView's resources come from.
            // This will be the donor package if there is one, not our main package.
            String resourcePackage = packageInfo.packageName;
            if (packageInfo.applicationInfo.metaData != null) {
                resourcePackage = packageInfo.applicationInfo.metaData.getString(
                        "com.android.webview.WebViewDonorPackage", resourcePackage);
            }
            int packageId = webViewDelegate.getPackageId(ctx.getResources(), resourcePackage);

            mAwInit.setUpResourcesOnBackgroundThread(packageId, ctx);

            try (ScopedSysTraceEvent e2 = ScopedSysTraceEvent.scoped(
                         "WebViewChromiumFactoryProvider.initCommandLine")) {
                // This may take ~20 ms only on userdebug devices.
                CommandLineUtil.initCommandLine();
            }

            boolean multiProcess = false;
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.O) {
                // Ask the system if multiprocess should be enabled on O+.
                multiProcess = webViewDelegate.isMultiProcessEnabled();
            } else if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N) {
                // Check the multiprocess developer setting directly on N.
                multiProcess = Settings.Global.getInt(ctx.getContentResolver(),
                                       Settings.Global.WEBVIEW_MULTIPROCESS, 0)
                        == 1;
            }
            if (multiProcess) {
                CommandLine cl = CommandLine.getInstance();
                cl.appendSwitch(AwSwitches.WEBVIEW_SANDBOXED_RENDERER);
            }

            int applicationFlags = ctx.getApplicationInfo().flags;
            boolean isAppDebuggable = (applicationFlags & ApplicationInfo.FLAG_DEBUGGABLE) != 0;
            boolean isOsDebuggable = BuildInfo.isDebugAndroid();
            // Enable logging JS console messages in system logs only if the app is debuggable or
            // it's a debugable android build.
            if (isAppDebuggable || isOsDebuggable) {
                CommandLine cl = CommandLine.getInstance();
                cl.appendSwitch(AwSwitches.WEBVIEW_LOG_JS_CONSOLE_MESSAGES);
            }

            ThreadUtils.setWillOverrideUiThread(true);
            BuildInfo.setBrowserPackageInfo(packageInfo);

            try (StrictModeContext ignored = StrictModeContext.allowDiskWrites()) {
                try (ScopedSysTraceEvent e2 = ScopedSysTraceEvent.scoped(
                             "WebViewChromiumFactoryProvider.loadChromiumLibrary")) {
                    String dataDirectorySuffix = null;
                    if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
                        dataDirectorySuffix = webViewDelegate.getDataDirectorySuffix();
                    }
                    AwBrowserProcess.loadLibrary(dataDirectorySuffix);
                }

                try (ScopedSysTraceEvent e2 = ScopedSysTraceEvent.scoped(
                             "WebViewChromiumFactoryProvider.loadGlueLayerPlatSupportLibrary")) {
                    System.loadLibrary("webviewchromium_plat_support");
                }

                deleteContentsOnPackageDowngrade(packageInfo);
            }

            // Now safe to use WebView data directory.

            mAwInit.startVariationsInit();

            mShouldDisableThreadChecking = shouldDisableThreadChecking(ctx);

            setSingleton(this);
        }

        TimesHistogramSample histogram =
                new TimesHistogramSample("Android.WebView.Startup.CreationTime.Stage1.FactoryInit");
        histogram.record(SystemClock.elapsedRealtime() - startTime);
    }

    /* package */ static void checkStorageIsNotDeviceProtected(Context context) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.N
                && GlueApiHelperForN.isDeviceProtectedStorage(context)) {
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
                && Build.VERSION.SDK_INT < Build.VERSION_CODES.P && BundleUtils.isBundle()) {
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
                mStaticsAdapter = new WebViewChromiumFactoryProvider.Statics() {
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
                    public void initSafeBrowsing(Context context, ValueCallback<Boolean> callback) {
                        sharedStatics.initSafeBrowsing(
                                context, CallbackConverter.fromValueCallback(callback));
                    }

                    @Override
                    public void setSafeBrowsingWhitelist(
                            List<String> urls, ValueCallback<Boolean> callback) {
                        sharedStatics.setSafeBrowsingWhitelist(
                                urls, CallbackConverter.fromValueCallback(callback));
                    }

                    @Override
                    public Uri getSafeBrowsingPrivacyPolicyUrl() {
                        return sharedStatics.getSafeBrowsingPrivacyPolicyUrl();
                    }

                    public boolean isMultiProcessEnabled() {
                        return sharedStatics.isMultiProcessEnabled();
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

    // Workaround for IME thread crashes on grandfathered OEM apps.
    private boolean shouldDisableThreadChecking(Context context) {
        String appName = context.getPackageName();
        int versionCode = PackageUtils.getPackageVersion(context, appName);
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
            Log.w(TAG, "Disabling thread check in WebView. "
                            + "APK name: " + appName + ", versionCode: " + versionCode
                            + ", targetSdkVersion: " + appTargetSdkVersion);
        }
        return shouldDisable;
    }

    @Override
    public GeolocationPermissions getGeolocationPermissions() {
        return mAwInit.getGeolocationPermissions();
    }

    @Override
    public CookieManager getCookieManager() {
        return mAwInit.getCookieManager();
    }

    @Override
    public ServiceWorkerController getServiceWorkerController() {
        synchronized (mAwInit.getLock()) {
            if (mObjectHolderForN.mServiceWorkerController == null) {
                mObjectHolderForN.mServiceWorkerController =
                        GlueApiHelperForN.createServiceWorkerControllerAdapter(mAwInit);
            }
        }
        return mObjectHolderForN.mServiceWorkerController;
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
        return mAwInit.getWebStorage();
    }

    @Override
    public WebViewDatabase getWebViewDatabase(final Context context) {
        return mAwInit.getWebViewDatabase(context);
    }

    WebViewDelegate getWebViewDelegate() {
        return mWebViewDelegate;
    }

    WebViewContentsClientAdapter createWebViewContentsClientAdapter(WebView webView,
            Context context) {
        try (ScopedSysTraceEvent e = ScopedSysTraceEvent.scoped(
                     "WebViewChromiumFactoryProvider.insideCreateWebViewContentsClientAdapter")) {
            return new WebViewContentsClientAdapter(webView, context, mWebViewDelegate);
        }
    }

    AutofillProvider createAutofillProvider(Context context, ViewGroup containerView) {
        if (Build.VERSION.SDK_INT < Build.VERSION_CODES.O) return null;
        return new AwAutofillProvider(context, containerView);
    }

    void startYourEngines(boolean onMainThread) {
        try (ScopedSysTraceEvent e1 = ScopedSysTraceEvent.scoped(
                     "WebViewChromiumFactoryProvider.startYourEngines")) {
            mAwInit.startYourEngines(onMainThread);
        }
    }

    boolean hasStarted() {
        return mAwInit.hasStarted();
    }

    // Only on UI thread.
    AwBrowserContext getBrowserContextOnUiThread() {
        return mAwInit.getBrowserContextOnUiThread();
    }

    WebViewChromiumAwInit getAwInit() {
        return mAwInit;
    }

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
}
