// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.SharedPreferences;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.SystemClock;
import android.system.Os;

import androidx.annotation.IntDef;
import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BaseSwitches;
import org.chromium.base.BuildConfig;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.JNIUtils;
import org.chromium.base.Log;
import org.chromium.base.NativeLibraryLoadedStatus;
import org.chromium.base.NativeLibraryLoadedStatus.NativeLibraryLoadedStatusProvider;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CheckDiscard;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.metrics.RecordHistogram;
import org.chromium.base.metrics.UmaRecorderHolder;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.util.Locale;

import javax.annotation.concurrent.GuardedBy;

/**
 * This class provides functionality to load and register the native libraries.
 * Callers are allowed to separate loading the libraries from initializing them.
 * This may be an advantage for Android Webview, where the libraries can be loaded
 * by the zygote process, but then needs per process initialization after the
 * application processes are forked from the zygote process.
 *
 * The libraries may be loaded and initialized from any thread. Synchronization
 * primitives are used to ensure that overlapping requests from different
 * threads are handled sequentially.
 *
 * See also base/android/library_loader/library_loader_hooks.cc, which contains
 * the native counterpart to this class.
 */
@MainDex
@JNINamespace("base::android")
public class LibraryLoader {
    private static final String TAG = "LibraryLoader";

    // Location of extracted native libraries.
    private static final String LIBRARY_DIR = "native_libraries";

    // Shared preferences key for the reached code profiler.
    private static final String DEPRECATED_REACHED_CODE_PROFILER_KEY =
            "reached_code_profiler_enabled";
    private static final String REACHED_CODE_SAMPLING_INTERVAL_KEY =
            "reached_code_sampling_interval";

    // Default sampling interval for reached code profiler in microseconds.
    private static final int DEFAULT_REACHED_CODE_SAMPLING_INTERVAL_US = 10000;

    // The singleton instance of LibraryLoader. Never null (not final for tests).
    private static LibraryLoader sInstance = new LibraryLoader();

    // One-way switch becomes true when the libraries are initialized (by calling
    // LibraryLoaderJni.get().libraryLoaded, which forwards to LibraryLoaded(...) in
    // library_loader_hooks.cc).  Note that this member should remain a one-way switch, since it
    // accessed from multiple threads without a lock.
    private volatile boolean mInitialized;

    // State that only transitions one-way from 0->1->2. Volatile for the same reasons as
    // mInitialized.
    private volatile @LoadState int mLoadState;

    // Guards all fields below.
    private final Object mLock = new Object();

    // Guards non-Main Dex initialization, which doesn't touch any fields guarded by mLock.
    private final Object mNonMainDexLock = new Object();

    private NativeLibraryPreloader mLibraryPreloader;
    private boolean mLibraryPreloaderCalled;

    // Whether to use the Chromium linker vs system linker.
    private boolean mUseChromiumLinker;

    // Whether to use ModernLinker, vs LegacyLinker.
    private boolean mUseModernLinker;

    // Whether the configuration has been set.
    private boolean mConfigurationSet;

    @IntDef({LoadState.NOT_LOADED, LoadState.MAIN_DEX_LOADED, LoadState.LOADED})
    @Retention(RetentionPolicy.SOURCE)
    private @interface LoadState {
        int NOT_LOADED = 0;
        int MAIN_DEX_LOADED = 1;
        int LOADED = 2;
    }

    // Similar to |mLoaded| but is limited case of being loaded in app zygote.
    // This is exposed to clients.
    private boolean mLoadedByZygote;

    // One-way switch becomes true when the Java command line is switched to
    // native.
    private boolean mCommandLineSwitched;

    // The type of process the shared library is loaded in.
    private @LibraryProcessType int mLibraryProcessType;

    // The number of milliseconds it took to load all the native libraries, which
    // will be reported via UMA. Set once when the libraries are done loading.
    private long mLibraryLoadTimeMs;

    /**
     * Call this method to determine if the chromium project must load the library
     * directly from a zip file.
     */
    private static boolean isInZipFile() {
        // The auto-generated NativeLibraries.sUseLibraryInZipFile variable will be true
        // iff the library remains embedded in the APK zip file on the target.
        return NativeLibraries.sUseLibraryInZipFile;
    }

    public static LibraryLoader getInstance() {
        return sInstance;
    }

    @VisibleForTesting
    protected LibraryLoader() {}

    /**
     * Set the {@Link LibraryProcessType} for this process.
     *
     * Since this function is called extremely early on in startup, locking is not required.
     *
     * @param type the process type.
     */
    public void setLibraryProcessType(@LibraryProcessType int type) {
        assert type != LibraryProcessType.PROCESS_UNINITIALIZED;
        if (type == mLibraryProcessType) return;
        if (mLibraryProcessType != LibraryProcessType.PROCESS_UNINITIALIZED) {
            throw new IllegalStateException(
                    String.format("Trying to change the LibraryProcessType from %d to %d",
                            mLibraryProcessType, type));
        }
        mLibraryProcessType = type;
    }

    /**
     * Set native library preloader, if set, the NativeLibraryPreloader.loadLibrary will be invoked
     * before calling System.loadLibrary, this only applies when not using the chromium linker.
     *
     * Since this function is called extremely early on in startup, locking is not required.
     *
     * @param loader the NativeLibraryPreloader, it shall only be set once and before the
     *               native library loaded.
     */
    public void setNativeLibraryPreloader(NativeLibraryPreloader loader) {
        assert mLibraryPreloader == null;
        assert mLoadState == LoadState.NOT_LOADED;
        mLibraryPreloader = loader;
    }

    /**
     * Sets the configuration for library loading.
     *
     * Must be called before loading the library. Since this function is called extremely early on
     * in startup, locking is not required.
     *
     * @param useChromiumLinker Whether to use the chromium linker.
     * @param useModernLinker Whether to use ModernLinker.
     */
    public void setLinkerImplementation(boolean useChromiumLinker, boolean useModernLinker) {
        assert !mInitialized;

        mUseChromiumLinker = useChromiumLinker;
        mUseModernLinker = useModernLinker;

        Log.d(TAG, "Configuration, useChromiumLinker = %b, useModernLinker = %b",
                mUseChromiumLinker, mUseModernLinker);
        mConfigurationSet = true;
    }

    @GuardedBy("mLock")
    private void setLinkerImplementationIfNeededAlreadyLocked() {
        if (mConfigurationSet) return;

        // Cannot use initializers for the variables below, as this makes roboelectric tests fail,
        // since they don't have a NativeLibraries class.
        mUseChromiumLinker = NativeLibraries.sUseLinker;
        mUseModernLinker = NativeLibraries.sUseModernLinker;
        mConfigurationSet = true;
    }

    // LegacyLinker is buggy on Android 10, causing crashes (see crbug.com/980304).
    //
    // Rather than preventing people from running chrome_public_apk on Android 10, fallback to the
    // system linker on this platform. We lose relocation sharing as a side-effect, but this
    // configuration does not ship to users (since we only use LegacyLinker for APKs targeted at
    // pre-N users).
    //
    // Note: This cannot be done in the build configuration, as otherwise chrome_public_apk cannot
    // both be used as the basis to ship on L, and the default APK used by developers on 10+.
    private boolean forceSystemLinker() {
        boolean result =
                mUseChromiumLinker && !mUseModernLinker && Build.VERSION.SDK_INT >= VERSION_CODES.Q;
        if (result) {
            Log.d(TAG,
                    "Forcing system linker, relocations will not be shared. "
                            + "This negatively impacts memory usage.");
        }
        return result;
    }

    public boolean useChromiumLinker() {
        return mUseChromiumLinker && !forceSystemLinker();
    }

    boolean useModernLinker() {
        return mUseModernLinker;
    }

    @CheckDiscard("Can't use @RemovableInRelease because Release build with DCHECK_IS_ON needs it")
    public void enableJniChecks() {
        if (!BuildConfig.DCHECK_IS_ON) return;

        NativeLibraryLoadedStatus.setProvider(new NativeLibraryLoadedStatusProvider() {
            @Override
            public boolean areMainDexNativeMethodsReady() {
                return mLoadState >= LoadState.MAIN_DEX_LOADED;
            }

            @Override
            public boolean areNativeMethodsReady() {
                return isInitialized();
            }
        });
    }

    /**
     * Return if library is already loaded successfully by the zygote.
     */
    public boolean isLoadedByZygote() {
        return mLoadedByZygote;
    }

    /**
     *  This method blocks until the library is fully loaded and initialized.
     */
    public void ensureInitialized() {
        if (isInitialized()) return;
        ensureMainDexInitialized();
        loadNonMainDex();
    }

    /**
     * This method blocks until the native library is initialized, and the Main Dex is loaded
     * (MainDex JNI is registered).
     *
     * You should use this if you would like to use isolated parts of the native library that don't
     * depend on content initialization, and only use MainDex classes with JNI.
     *
     * However, you should be careful not to call this too early in startup on the UI thread, or you
     * may significantly increase the time to first draw.
     */
    public void ensureMainDexInitialized() {
        synchronized (mLock) {
            loadMainDexAlreadyLocked(
                    ContextUtils.getApplicationContext().getApplicationInfo(), false);
            initializeAlreadyLocked();
        }
    }

    /**
     * Calls native library preloader (see {@link #setNativeLibraryPreloader}) with the app
     * context. If there is no preloader set, this function does nothing.
     * Preloader is called only once, so calling it explicitly via this method means
     * that it won't be (implicitly) called during library loading.
     */
    public void preloadNow() {
        preloadNowOverrideApplicationContext(ContextUtils.getApplicationContext());
    }

    /**
     * Similar to {@link #preloadNow}, but allows specifying app context to use.
     */
    public void preloadNowOverrideApplicationContext(Context appContext) {
        synchronized (mLock) {
            setLinkerImplementationIfNeededAlreadyLocked();
            if (mUseChromiumLinker) return;
            preloadAlreadyLocked(appContext.getApplicationInfo(), false /* inZygote */);
        }
    }

    @GuardedBy("mLock")
    private void preloadAlreadyLocked(ApplicationInfo appInfo, boolean inZygote) {
        try (TraceEvent te = TraceEvent.scoped("LibraryLoader.preloadAlreadyLocked")) {
            // Preloader uses system linker, we shouldn't preload if Chromium linker is used.
            assert !useChromiumLinker() || inZygote;
            if (mLibraryPreloader != null && !mLibraryPreloaderCalled) {
                mLibraryPreloader.loadLibrary(appInfo);
                mLibraryPreloaderCalled = true;
            }
        }
    }

    /**
     * Checks if library is fully loaded and initialized.
     */
    public boolean isInitialized() {
        return mInitialized && mLoadState == LoadState.LOADED;
    }

    /**
     * Loads the library and blocks until the load completes. The caller is responsible
     * for subsequently calling ensureInitialized().
     * May be called on any thread, but should only be called once. Note the thread
     * this is called on will be the thread that runs the native code's static initializers.
     * See the comment in doInBackground() for more considerations on this.
     */
    public void loadNow() {
        loadNowOverrideApplicationContext(ContextUtils.getApplicationContext());
    }

    /**
     * Override kept for callers that need to load from a different app context. Do not use unless
     * specifically required to load from another context that is not the current process's app
     * context.
     *
     * @param appContext The overriding app context to be used to load libraries.
     */
    public void loadNowOverrideApplicationContext(Context appContext) {
        synchronized (mLock) {
            if (mLoadState != LoadState.NOT_LOADED
                    && appContext != ContextUtils.getApplicationContext()) {
                throw new IllegalStateException("Attempt to load again from alternate context.");
            }
            loadMainDexAlreadyLocked(appContext.getApplicationInfo(), false /* inZygote */);
        }
        loadNonMainDex();
    }

    public void loadNowInZygote(ApplicationInfo appInfo) {
        synchronized (mLock) {
            assert mLoadState == LoadState.NOT_LOADED;
            loadMainDexAlreadyLocked(appInfo, true /* inZygote */);
            loadNonMainDex();
            mLoadedByZygote = true;
        }
    }

    /**
     * Initializes the library here and now: must be called on the thread that the
     * native will call its "main" thread. The library must have previously been
     * loaded with loadNow.
     */
    public void initialize() {
        synchronized (mLock) {
            initializeAlreadyLocked();
        }
    }

    /**
     * Enables the reached code profiler. The value comes from "ReachedCodeProfiler"
     * finch experiment, and is pushed on every run. I.e. the effect of the finch experiment
     * lags by one run, which is the best we can do considering that the profiler has to be enabled
     * before finch is initialized. Note that since LibraryLoader is in //base, it can't depend
     * on ChromeFeatureList, and has to rely on external code pushing the value.
     *
     * @param enabled whether to enable the reached code profiler.
     * @param samplingIntervalUs the sampling interval for reached code profiler.
     */
    public static void setReachedCodeProfilerEnabledOnNextRuns(
            boolean enabled, int samplingIntervalUs) {
        // Store 0 if the profiler is not enabled, otherwise store the sampling interval in
        // microseconds.
        if (enabled && samplingIntervalUs == 0) {
            samplingIntervalUs = DEFAULT_REACHED_CODE_SAMPLING_INTERVAL_US;
        } else if (!enabled) {
            samplingIntervalUs = 0;
        }
        SharedPreferences.Editor editor = ContextUtils.getAppSharedPreferences().edit();
        editor.remove(DEPRECATED_REACHED_CODE_PROFILER_KEY);
        editor.putInt(REACHED_CODE_SAMPLING_INTERVAL_KEY, samplingIntervalUs).apply();
    }

    /**
     * @return sampling interval for reached code profiler, or 0 when the profiler is disabled. (see
     *         setReachedCodeProfilerEnabledOnNextRuns()).
     */
    @VisibleForTesting
    public static int getReachedCodeSamplingIntervalUs() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            if (ContextUtils.getAppSharedPreferences().getBoolean(
                        DEPRECATED_REACHED_CODE_PROFILER_KEY, false)) {
                return DEFAULT_REACHED_CODE_SAMPLING_INTERVAL_US;
            }
            return ContextUtils.getAppSharedPreferences().getInt(
                    REACHED_CODE_SAMPLING_INTERVAL_KEY, 0);
        }
    }

    // Helper for loadAlreadyLocked(). Load a native shared library with the Chromium linker.
    // Records UMA histograms depending on the results of loading.
    private void loadLibraryWithCustomLinker(Linker linker, String library) {
        // Attempt shared RELROs, and if that fails then retry without.
        try {
            linker.loadLibrary(library, true /* isFixedAddressPermitted */);
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Failed to load native library with shared RELRO, retrying without");
            linker.loadLibrary(library, false /* isFixedAddressPermitted */);
        }
    }

    private void loadWithChromiumLinker(ApplicationInfo appInfo, String library) {
        Linker linker = Linker.getInstance();

        if (isInZipFile()) {
            String sourceDir = appInfo.sourceDir;
            linker.setApkFilePath(sourceDir);
            Log.i(TAG, " Loading %s from within %s", library, sourceDir);
        } else {
            Log.i(TAG, "Loading %s", library);
        }

        // Load the library using this Linker. May throw UnsatisfiedLinkError.
        loadLibraryWithCustomLinker(linker, library);
    }

    @GuardedBy("mLock")
    @SuppressLint("UnsafeDynamicallyLoadedCode")
    private void loadWithSystemLinkerAlreadyLocked(ApplicationInfo appInfo, boolean inZygote) {
        setEnvForNative();
        preloadAlreadyLocked(appInfo, inZygote);

        // If the libraries are located in the zip file, assert that the device API level is M or
        // higher. On devices <=M, the libraries should always be loaded by LegacyLinker.
        assert !isInZipFile() || Build.VERSION.SDK_INT >= VERSION_CODES.M;

        // Load libraries using the system linker.
        for (String library : NativeLibraries.LIBRARIES) {
            if (!isInZipFile()) {
                System.loadLibrary(library);
            } else {
                // Load directly from the APK.
                boolean is64Bit = ApiHelperForM.isProcess64Bit();
                String zipFilePath = appInfo.sourceDir;
                boolean crazyPrefix = forceSystemLinker(); // See comment in this function.
                String fullPath = zipFilePath + "!/"
                        + makeLibraryPathInZipFile(library, crazyPrefix, is64Bit);

                Log.i(TAG, "libraryName: %s", fullPath);
                System.load(fullPath);
            }
        }
    }

    // Invoke either Linker.loadLibrary(...), System.loadLibrary(...) or System.load(...),
    // triggering JNI_OnLoad in native code.
    @GuardedBy("mLock")
    @VisibleForTesting
    protected void loadMainDexAlreadyLocked(ApplicationInfo appInfo, boolean inZygote) {
        if (mLoadState >= LoadState.MAIN_DEX_LOADED) return;
        try (TraceEvent te = TraceEvent.scoped("LibraryLoader.loadMainDexAlreadyLocked")) {
            assert !mInitialized;
            assert mLibraryProcessType != LibraryProcessType.PROCESS_UNINITIALIZED || inZygote;
            setLinkerImplementationIfNeededAlreadyLocked();

            long startTime = SystemClock.uptimeMillis();

            if (useChromiumLinker() && !inZygote) {
                Log.d(TAG, "Loading with the Chromium linker.");
                // See base/android/linker/config.gni, the chromium linker is only enabled when
                // we have a single library.
                assert NativeLibraries.LIBRARIES.length == 1;
                String library = NativeLibraries.LIBRARIES[0];
                loadWithChromiumLinker(appInfo, library);
            } else {
                Log.d(TAG, "Loading with the System linker.");
                loadWithSystemLinkerAlreadyLocked(appInfo, inZygote);
            }

            long stopTime = SystemClock.uptimeMillis();
            mLibraryLoadTimeMs = stopTime - startTime;
            Log.d(TAG, "Time to load native libraries: %d ms", mLibraryLoadTimeMs);

            mLoadState = LoadState.MAIN_DEX_LOADED;
        } catch (UnsatisfiedLinkError e) {
            throw new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED, e);
        }
    }

    @VisibleForTesting
    // After Android M, this function is likely a no-op.
    protected void loadNonMainDex() {
        if (mLoadState == LoadState.LOADED) return;
        synchronized (mNonMainDexLock) {
            assert mLoadState != LoadState.NOT_LOADED;
            if (mLoadState == LoadState.LOADED) return;
            try (TraceEvent te = TraceEvent.scoped("LibraryLoader.loadNonMainDex")) {
                if (!JNIUtils.isSelectiveJniRegistrationEnabled()) {
                    LibraryLoaderJni.get().registerNonMainDexJni();
                }
                mLoadState = LoadState.LOADED;
            }
        }
    }

    /**
     * @param library The library name that is looking for.
     * @param crazyPrefix true iff adding crazy linker prefix to the file name.
     * @param is64Bit true if the caller think it's run on a 64 bit device.
     * @return the library path name in the zip file.
     */
    @NonNull
    public static String makeLibraryPathInZipFile(
            String library, boolean crazyPrefix, boolean is64Bit) {
        // Determine the ABI string that Android uses to find native libraries. Values are described
        // in: https://developer.android.com/ndk/guides/abis.html
        // The 'armeabi' is omitted here because it is not supported in Chrome/WebView, while Cronet
        // and Cast load the native library via other paths.
        String cpuAbi;
        switch (NativeLibraries.sCpuFamily) {
            case NativeLibraries.CPU_FAMILY_ARM:
                cpuAbi = is64Bit ? "arm64-v8a" : "armeabi-v7a";
                break;
            case NativeLibraries.CPU_FAMILY_X86:
                cpuAbi = is64Bit ? "x86_64" : "x86";
                break;
            case NativeLibraries.CPU_FAMILY_MIPS:
                cpuAbi = is64Bit ? "mips64" : "mips";
                break;
            default:
                throw new RuntimeException("Unknown CPU ABI for native libraries");
        }

        // When both the Chromium linker and zip-uncompressed native libraries are used,
        // the build system renames the native shared libraries with a 'crazy.' prefix
        // (e.g. "/lib/armeabi-v7a/libfoo.so" -> "/lib/armeabi-v7a/crazy.libfoo.so").
        //
        // This prevents the package manager from extracting them at installation/update time
        // to the /data directory. The libraries can still be accessed directly by the Chromium
        // linker from the APK.
        String crazyPart = crazyPrefix ? "crazy." : "";
        return String.format(
                Locale.US, "lib/%s/%s%s", cpuAbi, crazyPart, System.mapLibraryName(library));
    }

    // The WebView requires the Command Line to be switched over before
    // initialization is done. This is okay in the WebView's case since the
    // JNI is already loaded by this point.
    public void switchCommandLineForWebView() {
        synchronized (mLock) {
            ensureCommandLineSwitchedAlreadyLocked();
        }
    }

    // Switch the CommandLine over from Java to native if it hasn't already been done.
    // This must happen after the code is loaded and after JNI is ready (since after the
    // switch the Java CommandLine will delegate all calls the native CommandLine).
    @GuardedBy("mLock")
    private void ensureCommandLineSwitchedAlreadyLocked() {
        assert mLoadState >= LoadState.MAIN_DEX_LOADED;
        if (mCommandLineSwitched) {
            return;
        }
        CommandLine.enableNativeProxy();
        mCommandLineSwitched = true;
    }

    /**
     * Assert that library process type in the LibraryLoarder is compatible with provided type.
     *
     * @param libraryProcessType a library process type to assert.
     */
    public void assertCompatibleProcessType(@LibraryProcessType int libraryProcessType) {
        assert libraryProcessType == mLibraryProcessType;
    }

    // Invoke base::android::LibraryLoaded in library_loader_hooks.cc
    @GuardedBy("mLock")
    private void initializeAlreadyLocked() {
        if (mInitialized) return;
        assert mLibraryProcessType != LibraryProcessType.PROCESS_UNINITIALIZED;

        // Add a switch for the reached code profiler as late as possible since it requires a read
        // from the shared preferences. At this point the shared preferences are usually warmed up.
        if (mLibraryProcessType == LibraryProcessType.PROCESS_BROWSER) {
            int reachedCodeSamplingIntervalUs = getReachedCodeSamplingIntervalUs();
            if (reachedCodeSamplingIntervalUs > 0) {
                CommandLine.getInstance().appendSwitch(BaseSwitches.ENABLE_REACHED_CODE_PROFILER);
                CommandLine.getInstance().appendSwitchWithValue(
                        BaseSwitches.REACHED_CODE_SAMPLING_INTERVAL_US,
                        Integer.toString(reachedCodeSamplingIntervalUs));
            }
        }

        ensureCommandLineSwitchedAlreadyLocked();

        if (!LibraryLoaderJni.get().libraryLoaded(mLibraryProcessType)) {
            Log.e(TAG, "error calling LibraryLoaderJni.get().libraryLoaded");
            throw new ProcessInitException(LoaderErrors.FAILED_TO_REGISTER_JNI);
        }

        // Check that the version of the library we have loaded matches the version we expect
        if (!NativeLibraries.sVersionNumber.equals(LibraryLoaderJni.get().getVersionNumber())) {
            Log.e(TAG,
                    "Expected native library version number \"%s\", "
                            + "actual native library version number \"%s\"",
                    NativeLibraries.sVersionNumber, LibraryLoaderJni.get().getVersionNumber());
            throw new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_WRONG_VERSION);
        } else {
            // Log the version anyway as this is often helpful, but word it differently so it's
            // clear that it isn't an error.
            Log.i(TAG, "Loaded native library version number \"%s\"",
                    NativeLibraries.sVersionNumber);
        }
        UmaRecorderHolder.onLibraryLoaded();

        // From now on, keep tracing in sync with native.
        TraceEvent.onNativeTracingReady();

        // From this point on, native code is ready to use, but non-MainDex JNI may not yet have
        // been registered. Check isInitialized() to be sure that initialization is fully complete.
        // Note that this flag can be accessed asynchronously, so any initialization
        // must be performed before.
        mInitialized = true;
    }

    // Called after all native initializations are complete.
    public void onBrowserNativeInitializationComplete() {
        if (mUseChromiumLinker) {
            RecordHistogram.recordTimesHistogram(
                    "ChromiumAndroidLinker.BrowserLoadTime", mLibraryLoadTimeMs);
        }
    }

    // Records pending Chromium linker histogram state for renderer process. This cannot be
    // recorded as a histogram immediately because histograms and IPCs are not ready at the
    // time they are captured. This function stores a pending value, so that a later call to
    // RecordChromiumAndroidLinkerRendererHistogram() will record it correctly.
    public void registerRendererProcessHistogram() {
        if (!mUseChromiumLinker) return;
        synchronized (mLock) {
            LibraryLoaderJni.get().recordRendererLibraryLoadTime(mLibraryLoadTimeMs);
        }
    }

    /**
     * Overrides the library loader (normally with a mock) for testing.
     *
     * @param loader the mock library loader.
     */
    @VisibleForTesting
    public static void setLibraryLoaderForTesting(LibraryLoader loader) {
        sInstance = loader;
    }

    /**
     * Configure ubsan using $UBSAN_OPTIONS. This function needs to be called before any native
     * libraries are loaded because ubsan reads its configuration from $UBSAN_OPTIONS when the
     * native library is loaded.
     */
    public static void setEnvForNative() {
        // The setenv API was added in L. On older versions of Android, we should still see ubsan
        // reports, but they will not have stack traces.
        if (BuildConfig.IS_UBSAN && Build.VERSION.SDK_INT >= Build.VERSION_CODES.LOLLIPOP) {
            try {
                // This value is duplicated in build/android/pylib/constants/__init__.py.
                Os.setenv("UBSAN_OPTIONS",
                        "print_stacktrace=1 stack_trace_format='#%n pc %o %m' "
                                + "handle_segv=0 handle_sigbus=0 handle_sigfpe=0",
                        true);
            } catch (Exception e) {
                Log.w(TAG, "failed to set UBSAN_OPTIONS", e);
            }
        }
    }

    /**
     * This sets the LibraryLoader internal state to its fully initialized state and should *only*
     * be used by clients like NativeTests which manually load their native libraries without using
     * the LibraryLoader.
     */
    public void setLibrariesLoadedForNativeTests() {
        mLoadState = LoadState.LOADED;
        mInitialized = true;
    }

    @NativeMethods
    interface Natives {
        // Only methods needed before or during normal JNI registration are during System.OnLoad.
        // nativeLibraryLoaded is then called to register everything else.  This process is called
        // "initialization".  This method will be mapped (by generated code) to the LibraryLoaded
        // definition in base/android/library_loader/library_loader_hooks.cc.
        //
        // Return true on success and false on failure.
        boolean libraryLoaded(@LibraryProcessType int processType);

        void registerNonMainDexJni();

        // Records the number of milliseconds it took to load the libraries in the renderer.
        void recordRendererLibraryLoadTime(long libraryLoadTime);

        // Get the version of the native library. This is needed so that we can check we
        // have the right version before initializing the (rest of the) JNI.
        String getVersionNumber();
    }
}
