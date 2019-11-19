// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.base.library_loader;

import android.annotation.SuppressLint;
import android.content.Context;
import android.content.pm.ApplicationInfo;
import android.os.Build;
import android.os.Build.VERSION_CODES;
import android.os.SystemClock;
import android.support.v4.content.ContextCompat;
import android.system.Os;

import androidx.annotation.NonNull;
import androidx.annotation.VisibleForTesting;

import org.chromium.base.BaseSwitches;
import org.chromium.base.BuildConfig;
import org.chromium.base.BuildInfo;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.FileUtils;
import org.chromium.base.Log;
import org.chromium.base.StreamUtil;
import org.chromium.base.StrictModeContext;
import org.chromium.base.TraceEvent;
import org.chromium.base.annotations.CalledByNative;
import org.chromium.base.annotations.JNINamespace;
import org.chromium.base.annotations.MainDex;
import org.chromium.base.annotations.NativeMethods;
import org.chromium.base.compat.ApiHelperForM;
import org.chromium.base.metrics.CachedMetrics;
import org.chromium.base.metrics.RecordHistogram;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;
import java.util.Locale;
import java.util.zip.ZipEntry;
import java.util.zip.ZipFile;

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

    // Experience shows that on some devices, the PackageManager fails to properly extract
    // native shared libraries to the /data partition at installation or upgrade time,
    // which creates all kind of chaos (https://crbug.com/806998).
    //
    // We implement a fallback when we detect the issue by manually extracting the library
    // into Chromium's own data directory, then retrying to load the new library from here.
    //
    // This will work for any device running K-. Starting with Android L, render processes
    // cannot access the file system anymore, and extraction will always fail for them.
    // However, the issue doesn't seem to appear in the field for Android L.
    //
    // Also, starting with M, the issue doesn't exist if shared libraries are stored
    // uncompressed in the APK (as Chromium does), because the system linker can access them
    // directly, and the PackageManager will thus never extract them in the first place.
    public static final boolean PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION =
            Build.VERSION.SDK_INT <= VERSION_CODES.KITKAT;

    // Location of extracted native libraries.
    private static final String LIBRARY_DIR = "native_libraries";

    // Shared preferences key for the reached code profiler.
    private static final String REACHED_CODE_PROFILER_ENABLED_KEY = "reached_code_profiler_enabled";

    // The singleton instance of LibraryLoader. Never null (not final for tests).
    private static LibraryLoader sInstance = new LibraryLoader();

    // One-way switch becomes true when the libraries are initialized (
    // by calling LibraryLoaderJni.get().libraryLoaded, which forwards to LibraryLoaded(...) in
    // library_loader_hooks.cc).
    // Note that this member should remain a one-way switch, since it accessed from multiple
    // threads without a lock.
    private volatile boolean mInitialized;

    // Guards all fields below.
    private final Object mLock = new Object();

    private NativeLibraryPreloader mLibraryPreloader;
    private boolean mLibraryPreloaderCalled;

    // Whether to use the Chromium linker vs system linker.
    private boolean mUseChromiumLinker;

    // Whether to use ModernLinker, vs LegacyLinker.
    private boolean mUseModernLinker;

    // Whether the configuration has been set.
    private boolean mConfigurationSet;

    // One-way switch becomes true when the libraries are loaded.
    private boolean mLoaded;

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

    // Stores information about attempts to load the library and eventually emits those bits as
    // UMA histograms.
    private static final LoadStatusRecorder sLoadStatusRecorder = new LoadStatusRecorder();

    /**
     * Call this method to determine if the chromium project must load the library
     * directly from a zip file.
     */
    private static boolean isInZipFile() {
        // The auto-generated NativeLibraries.sUseLibraryInZipFile variable will be true
        // iff the library remains embedded in the APK zip file on the target.
        return NativeLibraries.sUseLibraryInZipFile;
    }

    /**
     * Set native library preloader, if set, the NativeLibraryPreloader.loadLibrary will be invoked
     * before calling System.loadLibrary, this only applies when not using the chromium linker.
     *
     * @param loader the NativeLibraryPreloader, it shall only be set once and before the
     *               native library loaded.
     */
    public void setNativeLibraryPreloader(NativeLibraryPreloader loader) {
        synchronized (mLock) {
            assert mLibraryPreloader == null && !mLoaded;
            mLibraryPreloader = loader;
        }
    }

    @GuardedBy("mLock")
    private void setConfigurationIfNeeded() {
        if (mConfigurationSet) return;

        // Cannot use initializers for the variables below, as this makes roboelectric tests fail,
        // since they don't have a NativeLibraries class.
        mUseChromiumLinker = NativeLibraries.sUseLinker;
        mUseModernLinker = NativeLibraries.sUseModernLinker;
        mConfigurationSet = true;
    }

    public static LibraryLoader getInstance() {
        return sInstance;
    }

    private LibraryLoader() {}

    /**
     * Sets the configuration for library loading.
     *
     * Must be called before loading the library.
     *
     * @param useChromiumLinker Whether to use the chromium linker.
     * @param useModernLinker Whether to use ModernLinker.
     */
    public void setConfiguration(boolean useChromiumLinker, boolean useModernLinker) {
        synchronized (mLock) {
            assert !mInitialized;

            mUseChromiumLinker = useChromiumLinker;
            mUseModernLinker = useModernLinker;

            Log.d(TAG, "Configuration, useChromiumLinker = %b, useModernLinker = %b",
                    mUseChromiumLinker, mUseModernLinker);
            mConfigurationSet = true;
        }
    }

    public boolean useChromiumLinker() {
        return mUseChromiumLinker;
    }

    public boolean useModernLinker() {
        return mUseModernLinker;
    }

    public boolean areTestsEnabled() {
        return NativeLibraries.sEnableLinkerTests;
    }

    /**
     * Return if library is already loaded successfully by the zygote.
     */
    public boolean isLoadedByZygote() {
        return mLoadedByZygote;
    }

    /**
     *  This method blocks until the library is fully loaded and initialized.
     *
     * @param processType the process the shared library is loaded in.
     */
    public void ensureInitialized(@LibraryProcessType int processType) {
        synchronized (mLock) {
            if (mInitialized) return;

            loadAlreadyLocked(ContextUtils.getApplicationContext().getApplicationInfo(),
                    false /* inZygote */);
            initializeAlreadyLocked(processType);
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
            setConfigurationIfNeeded();
            if (mUseChromiumLinker) return;
            preloadAlreadyLocked(appContext.getApplicationInfo());
        }
    }

    private void preloadAlreadyLocked(ApplicationInfo appInfo) {
        try (TraceEvent te = TraceEvent.scoped("LibraryLoader.preloadAlreadyLocked")) {
            // Preloader uses system linker, we shouldn't preload if Chromium linker is used.
            assert !mUseChromiumLinker;
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
        return mInitialized;
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
            if (mLoaded && appContext != ContextUtils.getApplicationContext()) {
                throw new IllegalStateException("Attempt to load again from alternate context.");
            }
            loadAlreadyLocked(appContext.getApplicationInfo(), false /* inZygote */);
        }
    }

    public void loadNowInZygote(ApplicationInfo appInfo) {
        synchronized (mLock) {
            assert !mLoaded;
            loadAlreadyLocked(appInfo, true /* inZygote */);
            mLoadedByZygote = true;
        }
    }

    /**
     * Initializes the library here and now: must be called on the thread that the
     * native will call its "main" thread. The library must have previously been
     * loaded with loadNow.
     *
     * @param processType the process the shared library is loaded in.
     */
    public void initialize(@LibraryProcessType int processType) {
        synchronized (mLock) {
            initializeAlreadyLocked(processType);
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
     */
    public static void setReachedCodeProfilerEnabledOnNextRuns(boolean enabled) {
        ContextUtils.getAppSharedPreferences()
                .edit()
                .putBoolean(REACHED_CODE_PROFILER_ENABLED_KEY, enabled)
                .apply();
    }

    /**
     * @return whether to enable reached code profiler (see
     *         setReachedCodeProfilerEnabledOnNextRuns()).
     */
    private static boolean isReachedCodeProfilerEnabled() {
        try (StrictModeContext ignored = StrictModeContext.allowDiskReads()) {
            return ContextUtils.getAppSharedPreferences().getBoolean(
                    REACHED_CODE_PROFILER_ENABLED_KEY, false);
        }
    }

    // Helper for loadAlreadyLocked(). Load a native shared library with the Chromium linker.
    // Records UMA histograms depending on the results of loading.
    private static void loadLibraryWithCustomLinker(
            Linker linker, String library, boolean isFirstAttempt) {
        // Attempt shared RELROs, and if that fails then retry without.
        boolean loadAtFixedAddress = true;
        boolean success = true;
        try {
            linker.loadLibrary(library, true /* isFixedAddressPermitted */);
        } catch (UnsatisfiedLinkError e) {
            Log.w(TAG, "Failed to load native library with shared RELRO, retrying without");
            sLoadStatusRecorder.recordLoadAttempt(
                    false /* success */, isFirstAttempt, true /* loadAtFixedAddress */);
            loadAtFixedAddress = false;
            success = false;
            linker.loadLibrary(library, false /* isFixedAddressPermitted */);
            success = true;
        } finally {
            sLoadStatusRecorder.recordLoadAttempt(success, isFirstAttempt, loadAtFixedAddress);
        }
    }

    // Experience shows that on some devices, the system sometimes fails to extract native libraries
    // at installation or update time from the APK. This function will extract the library and
    // return the extracted file path.
    static String getExtractedLibraryPath(ApplicationInfo appInfo, String libName) {
        assert PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION;
        Log.w(TAG, "Failed to load libName %s, attempting fallback extraction then trying again",
                libName);
        String libraryEntry = LibraryLoader.makeLibraryPathInZipFile(libName, false, false);
        return extractFileIfStale(appInfo, libraryEntry, makeLibraryDirAndSetPermission());
    }

    private static void loadWithChromiumLinker(ApplicationInfo appInfo, String library) {
        Linker linker = Linker.getInstance();

        if (isInZipFile()) {
            String sourceDir = appInfo.sourceDir;
            linker.setApkFilePath(sourceDir);
            Log.i(TAG, " Loading %s from within %s", library, sourceDir);
        } else {
            Log.i(TAG, "Loading %s", library);
        }

        try {
            // Load the library using this Linker. May throw UnsatisfiedLinkError.
            loadLibraryWithCustomLinker(linker, library, true /* isFirstAttempt */);
        } catch (UnsatisfiedLinkError e) {
            if (!isInZipFile() && PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION) {
                loadLibraryWithCustomLinker(linker, getExtractedLibraryPath(appInfo, library),
                        false /* isFirstAttempt */);
            } else {
                throw e;
            }
        }
    }

    @GuardedBy("mLock")
    @SuppressLint("UnsafeDynamicallyLoadedCode")
    private void loadWithSystemLinkerAlreadyLocked(ApplicationInfo appInfo) {
        setEnvForNative();
        preloadAlreadyLocked(appInfo);

        // If the libraries are located in the zip file, assert that the device API level is M or
        // higher. On devices <=M, the libraries should always be loaded by LegacyLinker.
        assert !isInZipFile() || Build.VERSION.SDK_INT >= VERSION_CODES.M;

        // Load libraries using the system linker.
        for (String library : NativeLibraries.LIBRARIES) {
            if (!isInZipFile()) {
                // The extract and retry logic isn't needed because this path is used only for local
                // development.
                System.loadLibrary(library);
            } else {
                // Load directly from the APK.
                boolean is64Bit = ApiHelperForM.isProcess64Bit();
                String zipFilePath = appInfo.sourceDir;
                String fullPath =
                        zipFilePath + "!/" + makeLibraryPathInZipFile(library, false, is64Bit);

                Log.i(TAG, "libraryName: %s", fullPath);
                System.load(fullPath);
            }
        }
    }

    // Invoke either Linker.loadLibrary(...), System.loadLibrary(...) or System.load(...),
    // triggering JNI_OnLoad in native code.
    @GuardedBy("mLock")
    private void loadAlreadyLocked(ApplicationInfo appInfo, boolean inZygote) {
        try (TraceEvent te = TraceEvent.scoped("LibraryLoader.loadAlreadyLocked")) {
            if (mLoaded) return;
            assert !mInitialized;
            setConfigurationIfNeeded();

            long startTime = SystemClock.uptimeMillis();

            if (mUseChromiumLinker && !inZygote) {
                Log.d(TAG, "Loading with the Chromium linker.");
                // See base/android/linker/config.gni, the chromium linker is only enabled when
                // we have a single library.
                assert NativeLibraries.LIBRARIES.length == 1;
                String library = NativeLibraries.LIBRARIES[0];
                loadWithChromiumLinker(appInfo, library);
            } else {
                Log.d(TAG, "Loading with the System linker.");
                loadWithSystemLinkerAlreadyLocked(appInfo);
            }

            long stopTime = SystemClock.uptimeMillis();
            mLibraryLoadTimeMs = stopTime - startTime;
            Log.d(TAG, "Time to load native libraries: %d ms", mLibraryLoadTimeMs);

            mLoaded = true;
        } catch (UnsatisfiedLinkError e) {
            throw new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED, e);
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
        assert mLoaded;
        if (mCommandLineSwitched) {
            return;
        }
        CommandLine.enableNativeProxy();
        mCommandLineSwitched = true;
    }

    // Invoke base::android::LibraryLoaded in library_loader_hooks.cc
    @GuardedBy("mLock")
    private void initializeAlreadyLocked(@LibraryProcessType int processType) {
        if (mInitialized) {
            if (mLibraryProcessType != processType) {
                throw new ProcessInitException(LoaderErrors.NATIVE_LIBRARY_LOAD_FAILED);
            }
            return;
        }
        mLibraryProcessType = processType;
        sLoadStatusRecorder.setProcessType(processType);

        // Add a switch for the reached code profiler as late as possible since it requires a read
        // from the shared preferences. At this point the shared preferences are usually warmed up.
        if (mLibraryProcessType == LibraryProcessType.PROCESS_BROWSER
                && isReachedCodeProfilerEnabled()) {
            CommandLine.getInstance().appendSwitch(BaseSwitches.ENABLE_REACHED_CODE_PROFILER);
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

        // From now on, keep tracing in sync with native.
        TraceEvent.registerNativeEnabledObserver();

        if (processType == LibraryProcessType.PROCESS_BROWSER
                && PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION) {
            // Perform the detection and deletion of obsolete native libraries on a
            // background thread.
            new Thread(() -> {
                final String suffix = BuildInfo.getInstance().extractedFileSuffix;
                final File[] files = getLibraryDir().listFiles();
                if (files == null) return;

                for (File file : files) {
                    // NOTE: Do not simply look for <suffix> at the end of the file.
                    //
                    // Extracted library files have names like 'libfoo.so<suffix>', but
                    // extractFileIfStale() will use FileUtils.copyFileStreamAtomicWithBuffer()
                    // to create them, and this method actually uses a transient temporary file
                    // named like 'libfoo.so<suffix>.tmp' to do that. These temporary files, if
                    // detected here, should be preserved; hence the reason why contains() is
                    // used below.
                    if (!file.getName().contains(suffix)) {
                        String fileName = file.getName();
                        if (!file.delete()) {
                            Log.w(TAG, "Unable to remove %s", fileName);
                        } else {
                            Log.i(TAG, "Removed obsolete file %s", fileName);
                        }
                    }
                }
            }).start();
        }

        // From this point on, native code is ready to use and checkIsReady()
        // shouldn't complain from now on (and in fact, it's used by the
        // following calls).
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
     * Override the library loader (normally with a mock) for testing.
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

    @CalledByNative
    public static void onUmaRecordingReadyInRenderer() {
        CachedMetrics.commitCachedMetrics();
    }

    // Android system sometimes fails to extract libraries from APK (https://crbug.com/806998).
    // This function manually extract libraries as a fallback.
    @SuppressLint({"SetWorldReadable"})
    private static String extractFileIfStale(
            ApplicationInfo appInfo, String pathWithinApk, File destDir) {
        assert PLATFORM_REQUIRES_NATIVE_FALLBACK_EXTRACTION;

        String apkPath = appInfo.sourceDir;
        String fileName =
                (new File(pathWithinApk)).getName() + BuildInfo.getInstance().extractedFileSuffix;
        File libraryFile = new File(destDir, fileName);

        if (!libraryFile.exists()) {
            ZipFile zipFile = null;
            try {
                zipFile = new ZipFile(apkPath);
                ZipEntry zipEntry = zipFile.getEntry(pathWithinApk);
                if (zipEntry == null) {
                    throw new RuntimeException("Cannot find ZipEntry" + pathWithinApk);
                }
                InputStream inputStream = zipFile.getInputStream(zipEntry);

                FileUtils.copyStreamToFile(inputStream, libraryFile);
                libraryFile.setReadable(true, false);
                libraryFile.setExecutable(true, false);
            } catch (IOException e) {
                throw new RuntimeException(e);
            } finally {
                StreamUtil.closeQuietly(zipFile);
            }
        }
        return libraryFile.getAbsolutePath();
    }

    // Ensure the extracted native libraries is created with the right permissions.
    private static File makeLibraryDirAndSetPermission() {
        if (!ContextUtils.isIsolatedProcess()) {
            File cacheDir = ContextCompat.getCodeCacheDir(ContextUtils.getApplicationContext());
            File libDir = new File(cacheDir, LIBRARY_DIR);
            cacheDir.mkdir();
            cacheDir.setExecutable(true, false);
            libDir.mkdir();
            libDir.setExecutable(true, false);
        }
        return getLibraryDir();
    }

    // Return File object for the directory containing extracted native libraries.
    private static File getLibraryDir() {
        return new File(
                ContextCompat.getCodeCacheDir(ContextUtils.getApplicationContext()), LIBRARY_DIR);
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

        // Records the number of milliseconds it took to load the libraries in the renderer.
        void recordRendererLibraryLoadTime(long libraryLoadTime);

        // Get the version of the native library. This is needed so that we can check we
        // have the right version before initializing the (rest of the) JNI.
        String getVersionNumber();
    }
}
